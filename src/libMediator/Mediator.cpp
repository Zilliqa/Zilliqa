/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <array>

#include "Mediator.h"
#include "common/Constants.h"
#include "libCrypto/Sha2.h"
#include "libDirectoryService/DirectoryService.h"
#include "libEth/Filters.h"
#include "libLookup/Lookup.h"
#include "libMetrics/Api.h"
#include "libMetrics/TracedIds.h"
#include "libNode/Node.h"
#include "libServer/DedicatedWebsocketServer.h"
#include "libServer/GetWorkServer.h"
#include "libUpdater/DaemonListener.h"
#include "libUtils/CommonUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/ShardSizeCalculator.h"
#include "libValidator/Validator.h"

namespace zil {
namespace local {

class MediatorVariables {
  int currentEpochNum = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void SetCurrentEpochNum(int num) {
    Init();
    currentEpochNum = num;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "tx.mediator.gauge",
                                          "Mediator info", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(currentEpochNum, {{"counter", "CurrentEpochNum"}});
      });
    }
  }
};

static MediatorVariables variables{};

}  // namespace local
}  // namespace zil

using namespace std;

std::atomic<bool> Mediator::m_disableTxns(false);
std::atomic<bool> Mediator::m_disableGetSmartContractState(false);
std::atomic<bool> Mediator::m_disableGetPendingTxns(true);

Mediator::Mediator(const PairOfKey& key, const Peer& peer)
    : m_selfKey(key),
      m_selfPeer(peer),
      m_ds(nullptr),
      m_node(nullptr),
      m_lookup(nullptr),
      m_validator(nullptr),
      m_dsBlockChain(),
      m_txBlockChain(),
      m_blocklinkchain(),
      m_consensusID(0),
      m_DSCommittee(make_shared<DequeOfNode>()),
      m_initialDSCommittee(make_shared<vector<PubKey>>()),
      m_dsBlockRand({{0}}),
      m_txBlockRand({{0}}),
      m_isRetrievedHistory(false),
      m_isVacuousEpoch(false),
      m_curSWInfo(),
      m_disablePoW(false),
      m_validateState(ValidateState::IDLE),
      m_aveBlockTimeInSeconds(
          static_cast<double>(
              TX_DISTRIBUTE_TIME_IN_MS +
              (DS_ANNOUNCEMENT_DELAY_IN_MS + SHARD_ANNOUNCEMENT_DELAY_IN_MS)) /
          1000),
      m_filtersAPICache(evmproj::filters::APICache::Create()),
      m_websocketServer(rpc::DedicatedWebsocketServer::Create()) {
  SetupLogLevel();
}

Mediator::~Mediator() {}

void Mediator::RegisterColleagues(DirectoryService* ds, Node* node,
                                  Lookup* lookup, Validator* validator) {
  m_ds = ds;
  m_node = node;
  m_lookup = lookup;
  m_validator = validator;
}

void Mediator::UpdateDSBlockRand(bool isGenesis) {
  LOG_MARKER();

  if (isGenesis) {
    // genesis block
    LOG_GENERAL(INFO, "Genesis DSBlockchain")
    array<unsigned char, UINT256_SIZE> rand1{};
    DataConversion::HexStrToStdArray(RAND1_GENESIS, rand1);
    copy(rand1.begin(), rand1.end(), m_dsBlockRand.begin());
  } else {
    DSBlock lastBlock = m_dsBlockChain.GetLastBlock();
    SHA256Calculator sha2;
    zbytes vec;
    lastBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    zbytes randVec;
    randVec = sha2.Finalize();
    copy(randVec.begin(), randVec.end(), m_dsBlockRand.begin());
  }
}

void Mediator::UpdateTxBlockRand(bool isGenesis) {
  LOG_MARKER();

  if (isGenesis) {
    LOG_GENERAL(INFO, "Genesis txBlockchain")
    array<unsigned char, UINT256_SIZE> rand2{};
    DataConversion::HexStrToStdArray(RAND2_GENESIS, rand2);
    copy(rand2.begin(), rand2.end(), m_txBlockRand.begin());
  } else {
    TxBlock lastBlock = m_txBlockChain.GetLastBlock();
    SHA256Calculator sha2;
    zbytes vec;
    lastBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    zbytes randVec;
    randVec = sha2.Finalize();
    copy(randVec.begin(), randVec.end(), m_txBlockRand.begin());
  }
}

std::string Mediator::GetNodeMode(const Peer& peer) {
  std::lock_guard<mutex> lock(m_mutexDSCommittee);
  bool bFound = false;

  for (auto const& i : *m_DSCommittee) {
    if (i.second == peer) {
      bFound = true;
      break;
    }
  }

  if (bFound) {
    if (peer == (*m_DSCommittee)[0].second) {
      return "DSLD";
    } else {
      return "DSBU";
    }
  } else {
    return "SHRD";
  }
}

void Mediator::IncreaseEpochNum() {
  std::lock_guard<mutex> lock(m_mutexVacuousEpoch);
  m_currentEpochNum++;
  m_isVacuousEpoch = CommonUtils::IsVacuousEpoch(m_currentEpochNum);
  zil::local::variables.SetCurrentEpochNum(m_currentEpochNum);

  // Update GetWork Server info for nodes in shard
  if (GETWORK_SERVER_MINE) {
    // roughly calc how many seconds to next PoW
    auto num_block =
        NUM_FINAL_BLOCK_PER_POW - (m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW);

    num_block = num_block % NUM_FINAL_BLOCK_PER_POW;
    auto now = std::chrono::system_clock::now();

    // block time = average over last
    auto wait_seconds = chrono::seconds(
        static_cast<unsigned int>(m_aveBlockTimeInSeconds) * num_block);

    GetWorkServer::GetInstance().SetNextPoWTime(now + wait_seconds);
  }

  auto span =
      zil::trace::Tracing::CreateSpan(zil::trace::FilterClass::NODE, "Epoch");
  span.SetAttribute("epoch.num", m_currentEpochNum);
  TracedIds::GetInstance().SetCurrentEpochSpanIds(span.GetIds());

  LOG_GENERAL(INFO, "Epoch number is now " << m_currentEpochNum);
  LOG_STATE("Epoch = " << m_currentEpochNum);

  if ((m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) && m_daemonListener) {
    auto quiesceDSBlock = m_daemonListener->quiesceDSBlock();
    if (quiesceDSBlock &&
        (m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW == *quiesceDSBlock)) {
      LOG_GENERAL(
          WARNING,
          "Quiesce DS block has been reached; rejecting new transactions");
      m_disableTxns = true;
    } else {
      auto updateDSBlock = m_daemonListener->updateDSBlock();
      if (updateDSBlock &&
          (m_currentEpochNum / NUM_FINAL_BLOCK_PER_POW == *updateDSBlock)) {
        LOG_GENERAL(WARNING,
                    "Update DS block has been reached; shutting down to "
                    "upgrade zilliqa");

        m_daemonListenerAsioCtx->stop();
        exit(0);
      }
    }
  }
}

bool Mediator::GetIsVacuousEpoch() { return m_isVacuousEpoch; }

void Mediator::AddPendingTxn(const Transaction& tx) {
  return m_node->AddPendingTxn(tx);
}

std::vector<Transaction> Mediator::GetPendingTxns() {
  return m_node->GetPendingTxns();
}

uint32_t Mediator::GetShardSize(const bool& useShardStructure) const {
  if (COMM_SIZE > 0) {
    return COMM_SIZE;
  }

  uint32_t shardNodeNum = 0;

  if (useShardStructure) {
    for (const auto& shard : m_ds->m_shards) {
      shardNodeNum += shard.size();
    }
  } else {
    shardNodeNum = m_ds->GetAllPoWSize();
  }

  return ShardSizeCalculator::CalculateShardSize(shardNodeNum);
}

bool Mediator::CheckWhetherBlockIsLatest(const uint64_t& dsblockNum,
                                         const uint64_t& epochNum) {
  LOG_MARKER();

  uint64_t latestDSBlockNumInBlockchain =
      m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (dsblockNum < (latestDSBlockNumInBlockchain + 1)) {
    LOG_EPOCH(WARNING, m_currentEpochNum,
              "We are processing duplicated blocks\n"
                  << "cur block num: " << latestDSBlockNumInBlockchain << "\n"
                  << "incoming block num: " << dsblockNum);
    return false;
  } else if (dsblockNum > latestDSBlockNumInBlockchain + 1) {
    LOG_EPOCH(WARNING, m_currentEpochNum,
              "Missing of some DS blocks. Requested: "
                  << dsblockNum
                  << " while Present: " << latestDSBlockNumInBlockchain);
    // Todo: handle missing DS blocks.
    return false;
  }

  if (epochNum < m_currentEpochNum) {
    LOG_EPOCH(WARNING, m_currentEpochNum,
              "We are processing duplicated blocks\n"
                  << "incoming block epoch num: " << epochNum);
    return false;
  } else if (epochNum > m_currentEpochNum) {
    LOG_EPOCH(WARNING, m_currentEpochNum,
              "Missing of some Tx blocks. Requested: "
                  << m_currentEpochNum << " while present: " << epochNum);
    // Todo: handle missing Tx blocks.
    return false;
  }

  return true;
}

void Mediator::SetupLogLevel() {
  LOG_MARKER();
  switch (DEBUG_LEVEL) {
    case 1: {
      LOG_DISPLAY_LEVEL_ABOVE(FATAL);
      break;
    }
    case 2: {
      LOG_DISPLAY_LEVEL_ABOVE(WARNING);
      break;
    }
    case 3: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
    case 4: {
      LOG_DISPLAY_LEVEL_ABOVE(DEBUG);
      break;
    }
    default: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
  }
}

bool Mediator::ToProcessTransaction() {
  return !GetIsVacuousEpoch() &&
         ((m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty() >=
               TXN_SHARD_TARGET_DIFFICULTY &&
           m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty() >=
               TXN_DS_TARGET_DIFFICULTY) ||
          m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
              TXN_DS_TARGET_NUM);
}

uint64_t Mediator::GetEarliestTrieDSEpoch(const uint64_t& currDSEpochNum) {
  if (currDSEpochNum >
      INIT_TRIE_DB_SNAPSHOT_EPOCH + NUM_DS_EPOCHS_STATE_HISTORY) {
    return currDSEpochNum - NUM_DS_EPOCHS_STATE_HISTORY;
  } else {
    return INIT_TRIE_DB_SNAPSHOT_EPOCH;
  }
}
