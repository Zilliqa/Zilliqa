/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <array>

#include "Mediator.h"
#include "common/Constants.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/ShardSizeCalculator.h"
#include "libValidator/Validator.h"

using namespace std;

Mediator::Mediator(const pair<PrivKey, PubKey>& key, const Peer& peer)
    : m_selfKey(key), m_selfPeer(peer) {
  m_ds = nullptr;
  m_node = nullptr;
  m_validator = nullptr;
  m_currentEpochNum = 0;
  m_isRetrievedHistory = false;
  m_DSCommittee = make_shared<std::deque<pair<PubKey, Peer>>>();
  m_initialDSCommittee = make_shared<vector<PubKey>>();
  m_archDB = nullptr;
  m_archival = nullptr;
}

Mediator::~Mediator() {}

void Mediator::RegisterColleagues(DirectoryService* ds, Node* node,
                                  Lookup* lookup, ValidatorBase* validator,
                                  BaseDB* archDB, Archival* arch) {
  m_ds = ds;
  m_node = node;
  m_lookup = lookup;
  m_validator = validator;
  if (ARCHIVAL_NODE) {
    m_archDB = archDB;
    m_archival = arch;
  }
}

void Mediator::UpdateDSBlockRand(bool isGenesis) {
  LOG_MARKER();

  if (isGenesis) {
    // genesis block
    LOG_GENERAL(INFO, "Genesis DSBlockchain")
    array<unsigned char, UINT256_SIZE> rand1;
    rand1 = DataConversion::HexStrToStdArray(RAND1_GENESIS);
    copy(rand1.begin(), rand1.end(), m_dsBlockRand.begin());
  } else {
    DSBlock lastBlock = m_dsBlockChain.GetLastBlock();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    vector<unsigned char> vec;
    lastBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    vector<unsigned char> randVec;
    randVec = sha2.Finalize();
    copy(randVec.begin(), randVec.end(), m_dsBlockRand.begin());
  }
}

void Mediator::UpdateTxBlockRand(bool isGenesis) {
  LOG_MARKER();

  if (isGenesis) {
    LOG_GENERAL(INFO, "Genesis txBlockchain")
    array<unsigned char, UINT256_SIZE> rand2;
    rand2 = DataConversion::HexStrToStdArray(RAND2_GENESIS);
    copy(rand2.begin(), rand2.end(), m_txBlockRand.begin());
  } else {
    TxBlock lastBlock = m_txBlockChain.GetLastBlock();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    vector<unsigned char> vec;
    lastBlock.GetHeader().Serialize(vec, 0);
    sha2.Update(vec);
    vector<unsigned char> randVec;
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

void Mediator::HeartBeatLaunch() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Mediator::HeartBeatLaunch not expected to be called from "
                "LookUp node.");
    return;
  }

  m_heartBeatTime = 0;

  auto func = [this]() -> void {
    // Set base timeout to roughly around one DS epoch (doesn't have to be very
    // accurate)
    const unsigned int heartBeatTimeoutInSeconds =
        NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
        POWPACKETSUBMISSION_WINDOW_IN_SECONDS +
        ((TX_DISTRIBUTE_TIME_IN_MS + FINALBLOCK_DELAY_IN_MS) / 1000) *
            NUM_FINAL_BLOCK_PER_POW;

    if (heartBeatTimeoutInSeconds <= HEARTBEAT_INTERVAL_IN_SECONDS) {
      LOG_GENERAL(FATAL,
                  "The configured heartbeat checking interval must be "
                  "smaller than the timeout value.")
      return;
    }

    while (true) {
      // Sleep for a while
      this_thread::sleep_for(chrono::seconds(HEARTBEAT_INTERVAL_IN_SECONDS));

      // Update running time
      lock_guard<mutex> guard(m_mutexHeartBeat);
      m_heartBeatTime += HEARTBEAT_INTERVAL_IN_SECONDS;

      // Check for liveness
      if (m_heartBeatTime < heartBeatTimeoutInSeconds) {
        LOG_GENERAL(INFO, "Still alive - " << m_heartBeatTime
                                           << " less than threshold "
                                           << heartBeatTimeoutInSeconds);
        continue;
      }

#ifdef HEARTBEAT_TEST
      m_killPulse = false;
#endif  // HEARTBEAT_TEST

      // At this point, need to rejoin network
      LOG_GENERAL(WARNING, "No pulse detected - " << m_heartBeatTime
                                                  << " at or beyond threshold "
                                                  << heartBeatTimeoutInSeconds);

      // Rejoin as a normal node
      if (DirectoryService::Mode::IDLE == m_ds->m_mode) {
        m_node->RejoinAsNormal();
        m_heartBeatTime = 0;
        continue;
      }

      // Rejoin as a DS node

      // First, fetch current DS committee info
      m_ds->m_synchronizer.FetchDSInfo(m_lookup);
      {
        unique_lock<mutex> lock(m_lookup->m_mutexDSInfoUpdation);
        if (m_lookup->cv_dsInfoUpdate.wait_for(
                lock, chrono::seconds(POW_WINDOW_IN_SECONDS +
                                      POWPACKETSUBMISSION_WINDOW_IN_SECONDS)) ==
            std::cv_status::timeout) {
          // If timed out waiting for DSInfo, rejoin as a normal node
          m_node->RejoinAsNormal();
          m_heartBeatTime = 0;
          continue;
        }
      }

      // Check if I'm still part of the current DS committee
      bool found = false;
      {
        lock_guard<mutex> g(m_mutexDSCommittee);
        for (auto const& ds : *m_DSCommittee) {
          if (m_selfKey.second == ds.first) {
            // I'm still part of the committee -> rejoin as a DS node
            m_ds->RejoinAsDS();
            found = true;
            break;
          }
        }
      }

      // I'm no longer part of the DS committee -> rejoin as a normal node
      if (!found) {
        m_node->RejoinAsNormal();
      }
    }
  };
  DetachedFunction(1, func);
}

void Mediator::HeartBeatPulse() {
  LOG_MARKER();

#ifdef HEARTBEAT_TEST
  if (m_killPulse) {
    LOG_GENERAL(INFO, "Skipping pulse");
    return;
  }
#endif  // HEARTBEAT_TEST

  lock_guard<mutex> guard(m_mutexHeartBeat);

  m_heartBeatTime = 0;
}

void Mediator::IncreaseEpochNum() {
  std::lock_guard<mutex> lock(m_mutexVacuousEpoch);
  m_currentEpochNum++;
  if ((m_currentEpochNum + NUM_VACUOUS_EPOCHS) % NUM_FINAL_BLOCK_PER_POW == 0) {
    m_isVacuousEpoch = true;
  } else {
    m_isVacuousEpoch = false;
  }
}

bool Mediator::GetIsVacuousEpoch() { return m_isVacuousEpoch; }

bool Mediator::GetIsVacuousEpoch(const uint64_t& epochNum) {
  return ((epochNum + NUM_VACUOUS_EPOCHS) % NUM_FINAL_BLOCK_PER_POW) == 0;
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
    LOG_EPOCH(WARNING, to_string(m_currentEpochNum).c_str(),
              "We are processing duplicated blocks\n"
                  << "cur block num: " << latestDSBlockNumInBlockchain << "\n"
                  << "incoming block num: " << dsblockNum);
    return false;
  } else if (dsblockNum > latestDSBlockNumInBlockchain + 1) {
    LOG_EPOCH(WARNING, to_string(m_currentEpochNum).c_str(),
              "Missing of some DS blocks. Requested: "
                  << dsblockNum
                  << " while Present: " << latestDSBlockNumInBlockchain);
    // Todo: handle missing DS blocks.
    return false;
  }

  if (epochNum < m_currentEpochNum) {
    LOG_EPOCH(WARNING, to_string(m_currentEpochNum).c_str(),
              "We are processing duplicated blocks\n"
                  << "incoming block epoch num: " << epochNum);
    return false;
  } else if (epochNum > m_currentEpochNum) {
    LOG_EPOCH(WARNING, to_string(m_currentEpochNum).c_str(),
              "Missing of some Tx blocks. Requested: " << epochNum);
    // Todo: handle missing Tx blocks.
    return false;
  }

  return true;
}