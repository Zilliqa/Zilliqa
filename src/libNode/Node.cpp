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

#include <arpa/inet.h>
#include <array>
#include <chrono>
#include <functional>
#include <thread>
#include <tuple>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libPOW/pow.h"
#include "libPersistence/Retriever.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/SysCommand.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libValidator/Validator.h"

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

const unsigned int MIN_CLUSTER_SIZE = 2;
const unsigned int MIN_CHILD_CLUSTER_SIZE = 2;

#define IP_MAPPING_FILE_NAME "ipMapping.xml"

void Node::PopulateAccounts() {
  try {
    string line;
    fstream keys_file(PREGENED_ACCOUNTS_FILE, ios::in);

    unsigned int counter = 0;

    while (getline(keys_file, line) && counter < NUM_ACCOUNTS_PREGENERATE) {
      vector<string> key_pair;  // pub/priv
      boost::algorithm::split(key_pair, line, boost::algorithm::is_any_of(" "));
      Address t_addr = Account::GetAddressFromPublicKey(
          PubKey::GetPubKeyFromString(key_pair[0]));
      AccountStore::GetInstance().AddAccount(t_addr, {0, 0});
      m_populatedAddresses.emplace_back(t_addr);
      counter++;
    }
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Problem occured when processing keys on line: "
                             << m_populatedAddresses.size() + 1);
  }
}

void Node::AddBalanceToGenesisAccount() {
  LOG_MARKER();

  const uint128_t balance_each = TOTAL_GENESIS_TOKEN / GENESIS_WALLETS.size();
  const uint128_t balance_left = TOTAL_GENESIS_TOKEN % (GENESIS_WALLETS.size());

  const uint64_t nonce{0};
  bool moduloCredited = false;

  for (auto& walletHexStr : GENESIS_WALLETS) {
    bytes addrBytes;
    if (!DataConversion::HexStrToUint8Vec(walletHexStr, addrBytes)) {
      continue;
    }
    Address addr{addrBytes};
    uint128_t bal = 0;
    if (!moduloCredited) {
      bal = balance_each + balance_left;
      moduloCredited = true;
    } else {
      bal = balance_each;
    }
    AccountStore::GetInstance().AddAccount(addr, {bal, nonce});
    LOG_GENERAL(INFO,
                "add genesis account " << addr << " with balance " << bal);
  }

  // Init account for issuing coinbase rewards
  AccountStore::GetInstance().AddAccount(Address(),
                                         {TOTAL_COINBASE_REWARD, nonce});

  if (ENABLE_ACCOUNTS_POPULATING) {
    PopulateAccounts();
  }

  AccountStore::GetInstance().UpdateStateTrieAll();
}

Node::Node(Mediator& mediator, [[gnu::unused]] unsigned int syncType,
           [[gnu::unused]] bool toRetrieveHistory)
    : m_mediator(mediator) {}

Node::~Node() {}

bool Node::DownloadPersistenceFromS3() {
  LOG_MARKER();
  string output;
  // TBD - find better way to capture the exit status of command
  SysCommand::ExecuteCmdWithOutput("./downloadIncrDB.py", output);
  return (output.find("Done!") != std::string::npos);
}

bool Node::Install(const SyncType syncType, const bool toRetrieveHistory,
                   bool rejoiningAfterRecover) {
  LOG_MARKER();

  m_txn_distribute_window_open = false;

  // m_state = IDLE;
  bool runInitializeGenesisBlocks = true;

  if (syncType == SyncType::DB_VERIF) {
    m_mediator.m_dsBlockChain.Reset();
    m_mediator.m_txBlockChain.Reset();

    m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                           m_mediator.m_txBlockChain);
    const auto& dsBlock = m_mediator.m_dsBlockChain.GetBlock(0);
    m_mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                             dsBlock.GetBlockHash());

    return true;
  }

  if (toRetrieveHistory) {
    if (!StartRetrieveHistory(syncType, rejoiningAfterRecover)) {
      AddGenesisInfo(SyncType::NO_SYNC);
      this->Prepare(runInitializeGenesisBlocks);
      return false;
    }

    if (SyncType::NEW_SYNC == syncType ||
        SyncType::NEW_LOOKUP_SYNC == syncType ||
        (rejoiningAfterRecover && (SyncType::NORMAL_SYNC == syncType))) {
      return true;
    }

    m_mediator.m_currentEpochNum =
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    m_mediator.IncreaseEpochNum();

    if (RECOVERY_TRIM_INCOMPLETED_BLOCK) {
      m_mediator.m_consensusID = m_mediator.m_currentEpochNum == 1 ? 1 : 0;
    }

    m_consensusLeaderID = 0;
    runInitializeGenesisBlocks = false;
    m_mediator.UpdateDSBlockRand();
    m_mediator.UpdateTxBlockRand();
    m_mediator.m_ds->m_mode = DirectoryService::IDLE;

    for (const auto& ds : *m_mediator.m_DSCommittee) {
      if (ds.first == m_mediator.m_selfKey.second) {
        m_mediator.m_ds->SetConsensusMyID(0);

        for (auto const& i : *m_mediator.m_DSCommittee) {
          if (i.first == m_mediator.m_selfKey.second) {
            LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                      "My node ID for this PoW consensus is "
                          << m_mediator.m_ds->GetConsensusMyID());
            break;
          }

          m_mediator.m_ds->IncrementConsensusMyID();
        }

        m_consensusMyID = m_mediator.m_ds->GetConsensusMyID();

        if (m_mediator.m_DSCommittee
                ->at(m_mediator.m_ds->GetConsensusLeaderID())
                .first == m_mediator.m_selfKey.second) {
          m_mediator.m_ds->m_mode = DirectoryService::PRIMARY_DS;
          LOG_GENERAL(INFO, "Set as DS leader: "
                                << m_mediator.m_selfPeer.GetPrintableIPAddress()
                                << ":"
                                << m_mediator.m_selfPeer.m_listenPortHost);
          LOG_STATE("[IDENT][" << std::setw(15) << std::left
                               << m_mediator.m_selfPeer.GetPrintableIPAddress()
                               << "][" << m_mediator.m_currentEpochNum
                               << "] DSLD");
        } else {
          m_mediator.m_ds->m_mode = DirectoryService::BACKUP_DS;
          LOG_GENERAL(INFO, "Set as DS backup: "
                                << m_mediator.m_selfPeer.GetPrintableIPAddress()
                                << ":"
                                << m_mediator.m_selfPeer.m_listenPortHost);
          LOG_STATE("[IDENT][" << std::setw(15) << std::left
                               << m_mediator.m_selfPeer.GetPrintableIPAddress()
                               << "][" << std::setw(6) << std::left
                               << m_mediator.m_ds->GetConsensusMyID()
                               << "] DSBK");
        }

        break;
      }
    }

    /// When non-rejoin mode, call wake-up or recovery
    if (SyncType::NO_SYNC == m_mediator.m_lookup->GetSyncType() ||
        SyncType::RECOVERY_ALL_SYNC == syncType) {
      if (RECOVERY_TRIM_INCOMPLETED_BLOCK) {
        WakeupAtDSEpoch();
      } else {
        WakeupAtTxEpoch();
      }

      return true;
    }
  }

  if (runInitializeGenesisBlocks) {
    AddGenesisInfo(syncType);
  }

  this->Prepare(runInitializeGenesisBlocks);
  return true;
}

void Node::Init() {
  // Zilliqa first epoch start from 1 not 0. So for the first DS epoch, there
  // will be 1 less mini epoch only for the first DS epoch. Hence, we have to
  // set consensusID for first epoch to 1.
  LOG_MARKER();

  m_retriever->CleanAll();
  m_retriever.reset();
  m_mediator.m_dsBlockChain.Reset();
  m_mediator.m_txBlockChain.Reset();
  m_mediator.m_blocklinkchain.Reset();
  {
    std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    m_mediator.m_DSCommittee->clear();
  }
  // m_committedTransactions.clear();
  AccountStore::GetInstance().Init();

  {
    DequeOfNode buildDSComm;
    lock_guard<mutex> lock(m_mediator.m_mutexInitialDSCommittee);
    if (m_mediator.m_initialDSCommittee->size() != 0) {
      for (const auto& initDSCommKey : *m_mediator.m_initialDSCommittee) {
        buildDSComm.emplace_back(initDSCommKey, Peer());
        // Set initial ds committee with null peer
      }
    } else {
      LOG_GENERAL(WARNING, "Initial DS comm size 0 ");
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(buildDSComm);
  }

  m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                         m_mediator.m_txBlockChain);
  const auto& dsBlock = m_mediator.m_dsBlockChain.GetBlock(0);
  m_mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                           dsBlock.GetBlockHash());
}

void Node::AddGenesisInfo(SyncType syncType) {
  LOG_MARKER();

  this->Init();
  if (syncType == SyncType::NO_SYNC) {
    m_mediator.m_consensusID = 1;
    m_consensusLeaderID = 1;
    AddBalanceToGenesisAccount();
  } else {
    m_mediator.m_consensusID = 0;
    m_consensusLeaderID = 0;
  }
}

bool Node::CheckIntegrity() {
  DequeOfNode dsComm;

  for (const auto& dsKey : *m_mediator.m_initialDSCommittee) {
    dsComm.emplace_back(dsKey, Peer());
  }
  std::list<BlockLink> blocklinks;
  if (!BlockStorage::GetBlockStorage().GetAllBlockLink(blocklinks)) {
    LOG_GENERAL(WARNING, "BlockStorage skipped or incompleted");
    return false;
  }

  blocklinks.sort([](const BlockLink& a, const BlockLink& b) {
    return std::get<BlockLinkIndex::INDEX>(a) <
           std::get<BlockLinkIndex::INDEX>(b);
  });

  std::list<TxBlockSharedPtr> txblocks;
  if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(txblocks)) {
    LOG_GENERAL(WARNING, "Failed to get Tx Blocks");
    return false;
  }

  txblocks.sort([](const TxBlockSharedPtr& a, const TxBlockSharedPtr& b) {
    return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
  });

  const auto& latestTxBlockNum = txblocks.back()->GetHeader().GetBlockNum();
  const auto& latestDSIndex = txblocks.back()->GetHeader().GetDSBlockNum();

  vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
      dirBlocks;
  for (const auto& blocklink : blocklinks) {
    if (get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::DS) {
      auto blockNum = get<BlockLinkIndex::DSINDEX>(blocklink);
      if (blockNum == 0) {
        continue;
      }
      DSBlockSharedPtr dsblock;
      if (!BlockStorage::GetBlockStorage().GetDSBlock(blockNum, dsblock)) {
        LOG_GENERAL(WARNING, "Could not retrieve DS Block " << blockNum);
        return false;
      }
      if (latestTxBlockNum <= dsblock->GetHeader().GetEpochNum()) {
        LOG_GENERAL(INFO, "Break off at "
                              << latestTxBlockNum << " " << latestDSIndex << " "
                              << dsblock->GetHeader().GetBlockNum() << " "
                              << dsblock->GetHeader().GetEpochNum());
        break;
      }
      dirBlocks.emplace_back(*dsblock);

    } else if (get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::VC) {
      auto blockHash = get<BlockLinkIndex::BLOCKHASH>(blocklink);
      VCBlockSharedPtr vcblock;
      if (!BlockStorage::GetBlockStorage().GetVCBlock(blockHash, vcblock)) {
        LOG_GENERAL(WARNING, "Could not retrieve VC Block " << blockHash);
        return false;
      }
      if (latestTxBlockNum <= vcblock->GetHeader().GetViewChangeEpochNo()) {
        break;
      }
      dirBlocks.emplace_back(*vcblock);
    } else if (get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::FB) {
      auto blockHash = get<BlockLinkIndex::BLOCKHASH>(blocklink);
      FallbackBlockSharedPtr fallbackwshardingstruct;
      if (!BlockStorage::GetBlockStorage().GetFallbackBlock(
              std::get<BlockLinkIndex::BLOCKHASH>(blocklink),
              fallbackwshardingstruct)) {
        LOG_GENERAL(WARNING, "Could not retrieve FB blocks " << blockHash);
        return false;
      }
      dirBlocks.emplace_back(*fallbackwshardingstruct);
    }
  }
  if (!m_mediator.m_validator->CheckDirBlocks(dirBlocks, dsComm, 1, dsComm)) {
    LOG_GENERAL(WARNING, "Failed to verify Dir Blocks");
    return false;
  }

  vector<TxBlock> txBlocks;

  for (const auto& txblock : txblocks) {
    txBlocks.emplace_back(*txblock);
  }

  if (m_mediator.m_validator->CheckTxBlocks(
          txBlocks, dsComm, m_mediator.m_blocklinkchain.GetLatestBlockLink()) !=
      ValidatorBase::TxBlockValidationMsg::VALID) {
    LOG_GENERAL(WARNING, "Failed to verify TxBlocks");
    return false;
  }

  for (uint i = 1; i < txBlocks.size(); i++) {
    auto microblockInfos = txBlocks.at(i).GetMicroBlockInfos();
    for (const auto& mbInfo : microblockInfos) {
      MicroBlockSharedPtr mbptr;
      LOG_GENERAL(INFO, mbInfo.m_shardId);
      /// Skip because empty microblocks are not stored
      if (mbInfo.m_txnRootHash == TxnHash()) {
        continue;
      }
      if (BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                        mbptr)) {
        auto tranHashes = mbptr->GetTranHashes();
        for (const auto& tranHash : tranHashes) {
          TxBodySharedPtr tx;
          if (!BlockStorage::GetBlockStorage().GetTxBody(tranHash, tx)) {
            LOG_GENERAL(WARNING, " " << tranHash << " failed to fetch");
            return false;
          }
        }
      } else {
        LOG_GENERAL(WARNING, " " << mbInfo.m_microBlockHash
                                 << "failed to fetch microblock");
        return false;
      }
    }
  }
  return true;
}

bool Node::ValidateDB() {
  const string lookupIp = "127.0.0.1";
  const unsigned int port = SEED_PORT;

  if (!CheckIntegrity()) {
    LOG_GENERAL(WARNING, "DB validation failed");
    return false;
  }

  LOG_GENERAL(INFO, "ValidateDB Success");

  if (!BlockStorage::GetBlockStorage().ReleaseDB()) {
    LOG_GENERAL(WARNING, "BlockStorage::ReleaseDB failed");
    return false;
  }

  bytes message = {MessageType::LOOKUP, LookupInstructionType::SETHISTORICALDB};

  if (!Messenger::SetSeedNodeHistoricalDB(message, MessageOffset::BODY,
                                          m_mediator.m_selfKey, 1,
                                          PERSISTENCE_PATH)) {
    LOG_GENERAL(WARNING, "SetSeedNodeHistoricalDB failed");
    return false;
  }

  struct in_addr ip_addr;
  inet_pton(AF_INET, lookupIp.c_str(), &ip_addr);
  Peer seed((uint128_t)ip_addr.s_addr, port);
  P2PComm::GetInstance().SendMessage(seed, message);

  return true;
}

void Node::Prepare(bool runInitializeGenesisBlocks) {
  LOG_MARKER();
  m_mediator.m_currentEpochNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
  m_mediator.UpdateDSBlockRand(runInitializeGenesisBlocks);
  m_mediator.UpdateTxBlockRand(runInitializeGenesisBlocks);
  SetState(POW_SUBMISSION);
  POW::GetInstance().EthashConfigureClient(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1,
      FULL_DATASET_MINE);
}

bool Node::StartRetrieveHistory(const SyncType syncType,
                                bool rejoiningAfterRecover) {
  LOG_MARKER();

  m_mediator.m_txBlockChain.Reset();
  m_mediator.m_dsBlockChain.Reset();
  m_mediator.m_blocklinkchain.Reset();
  {
    DequeOfNode buildDSComm;
    lock_guard<mutex> lock(m_mediator.m_mutexInitialDSCommittee);
    if (m_mediator.m_initialDSCommittee->size() != 0) {
      for (const auto& initDSCommKey : *m_mediator.m_initialDSCommittee) {
        buildDSComm.emplace_back(initDSCommKey, Peer());
        // Set initial ds committee with null peer
      }
    } else {
      LOG_GENERAL(FATAL, "Initial DS comm size 0 ");
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(buildDSComm);
  }

  if (LOOKUP_NODE_MODE) {
    m_mediator.m_DSCommittee->clear();
  }

  uint16_t ds_consensusLeaderID = 0;

  if (!BlockStorage::GetBlockStorage().GetDSCommittee(m_mediator.m_DSCommittee,
                                                      ds_consensusLeaderID)) {
    LOG_GENERAL(WARNING,
                "Retrieve history error due to failed to get ds committee.");
    return false;
  }

  m_mediator.m_ds->SetConsensusLeaderID(ds_consensusLeaderID);

  unordered_map<string, Peer> ipMapping;
  GetIpMapping(ipMapping);

  if (!ipMapping.empty()) {
    for (auto& ds : *m_mediator.m_DSCommittee) {
      string pubKey;
      if (!DataConversion::SerializableToHexStr(ds.first, pubKey)) {
        LOG_GENERAL(WARNING, "Error converting pubkey to string");
        continue;
      }

      if (ipMapping.find(pubKey) != ipMapping.end()) {
        ds.second = ipMapping.at(pubKey);
      }
    }
  }

  bool bDS = false;
  for (auto& i : *m_mediator.m_DSCommittee) {
    if (i.first == m_mediator.m_selfKey.second) {
      i.second = Peer();
      bDS = true;
      break;
    }
  }

  // Add ds guard nodes to blacklist exclusion list
  Guard::GetInstance().AddDSGuardToBlacklistExcludeList(
      *m_mediator.m_DSCommittee);

  if (SyncType::RECOVERY_ALL_SYNC == syncType) {
    Blacklist::GetInstance().Enable(false);
  }

  if (!LOOKUP_NODE_MODE && SyncType::RECOVERY_ALL_SYNC == syncType) {
    LOG_GENERAL(INFO, "Non-lookup node, wait "
                          << WAIT_LOOKUP_WAKEUP_IN_SECONDS
                          << " seconds for lookup wakeup...");
    this_thread::sleep_for(chrono::seconds(WAIT_LOOKUP_WAKEUP_IN_SECONDS));
  }

  m_retriever = std::make_shared<Retriever>(m_mediator);

  /// Retrieve block link
  bool ds_result =
      m_retriever->RetrieveBlockLink(RECOVERY_TRIM_INCOMPLETED_BLOCK &&
                                     SyncType::RECOVERY_ALL_SYNC == syncType);

  /// Retrieve Tx blocks, relative final-block state-delta from persistence
  bool st_result = m_retriever->RetrieveStates();
  bool tx_result =
      m_retriever->RetrieveTxBlocks(RECOVERY_TRIM_INCOMPLETED_BLOCK);

  if (!tx_result) {
    return false;
  }

  if (SyncType::NEW_SYNC == syncType || SyncType::NEW_LOOKUP_SYNC == syncType ||
      (rejoiningAfterRecover &&
       (SyncType::NORMAL_SYNC == syncType || SyncType::DS_SYNC == syncType))) {
    return true;
  }

  /// Retrieve lacked Tx blocks from lookup nodes
  if (SyncType::NO_SYNC == m_mediator.m_lookup->GetSyncType() &&
      SyncType::RECOVERY_ALL_SYNC != syncType) {
    uint64_t oldTxNum = m_mediator.m_txBlockChain.GetBlockCount();

    if (LOOKUP_NODE_MODE) {
      if (!m_mediator.m_lookup->GetMyLookupOffline()) {
        LOG_GENERAL(WARNING, "Cannot fetch data from off-line lookup node!");
        return false;
      }
    } else {
      if (!GetOfflineLookups()) {
        LOG_GENERAL(WARNING, "Cannot fetch data from lookup node!");
        return false;
      }

      unique_lock<mutex> lock(m_mediator.m_lookup->m_MutexCVSetTxBlockFromSeed);
      m_mediator.m_lookup->SetSyncType(SyncType::LOOKUP_SYNC);

      do {
        m_mediator.m_lookup->GetTxBlockFromSeedNodes(
            m_mediator.m_txBlockChain.GetBlockCount(), 0);
        LOG_GENERAL(INFO,
                    "Retrieve final block from lookup node, please wait...");
      } while (m_mediator.m_lookup->cv_setTxBlockFromSeed.wait_for(
                   lock, chrono::seconds(RECOVERY_SYNC_TIMEOUT)) ==
               cv_status::timeout);

      m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);

      /// If node recovery lagging behind too much, apply re-join
      /// process instead of node recovery
      if (m_mediator.m_txBlockChain.GetBlockCount() > oldTxNum + 1) {
        LOG_GENERAL(WARNING,
                    "Node recovery lagging behind too much, apply re-join "
                    "process instead");
        return false;
      }
    }

    /// Retrieve lacked final-block state-delta from lookup nodes
    if (m_mediator.m_txBlockChain.GetBlockCount() > oldTxNum) {
      unique_lock<mutex> lock(
          m_mediator.m_lookup->m_MutexCVSetStateDeltaFromSeed);
      m_mediator.m_lookup->SetSyncType(SyncType::LOOKUP_SYNC);

      do {
        m_mediator.m_lookup->GetStateDeltaFromSeedNodes(
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum());
        LOG_GENERAL(INFO,
                    "Retrieve final block state delta from lookup node, please "
                    "wait...");
      } while (m_mediator.m_lookup->cv_setStateDeltaFromSeed.wait_for(
                   lock, chrono::seconds(RECOVERY_SYNC_TIMEOUT)) ==
               cv_status::timeout);

      m_mediator.m_lookup->SetSyncType(SyncType::NO_SYNC);
    }
  }

  /// Rejoin will be applied following below rules:
  /// 1. Non-lookup node &&
  /// 2. Not from upgrading mode &&
  /// 3. Not from re-join mode &&
  /// 4. Not from recovery-all mode &&
  /// 5. Still in first DS epoch, or in vacuous epoch
  if (!LOOKUP_NODE_MODE &&
      SyncType::NO_SYNC == m_mediator.m_lookup->GetSyncType() &&
      SyncType::RECOVERY_ALL_SYNC != syncType &&
      (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
           NUM_FINAL_BLOCK_PER_POW ||
       m_mediator.GetIsVacuousEpoch(
           m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
           1))) {
    LOG_GENERAL(WARNING,
                "Node recovery with vacuous epoch or in first DS epoch, apply "
                "re-join process instead");
    return false;
  }

  /// Save coin base for final block, from last DS epoch to current TX epoch
  if (bDS && !(RECOVERY_TRIM_INCOMPLETED_BLOCK &&
               SyncType::RECOVERY_ALL_SYNC == syncType)) {
    for (uint64_t blockNum =
             m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum();
         blockNum <=
         m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
         ++blockNum) {
      LOG_GENERAL(INFO, "Update coin base for finalblock with blockNum: "
                            << blockNum << ", reward: "
                            << m_mediator.m_txBlockChain.GetBlock(blockNum)
                                   .GetHeader()
                                   .GetRewards());
      m_mediator.m_ds->SaveCoinbase(
          m_mediator.m_txBlockChain.GetBlock(blockNum).GetB1(),
          m_mediator.m_txBlockChain.GetBlock(blockNum).GetB2(),
          CoinbaseReward::FINALBLOCK_REWARD, blockNum + 1);
      m_mediator.m_ds->m_totalTxnFees +=
          m_mediator.m_txBlockChain.GetBlock(blockNum).GetHeader().GetRewards();
    }
  }

  /// Retrieve sharding structure and setup relative variables
  if (!BlockStorage::GetBlockStorage().GetShardStructure(
          m_mediator.m_ds->m_shards)) {
    LOG_GENERAL(WARNING, "BlockStorage::GetShardStructure failed");
  }

  if (!ipMapping.empty()) {
    for (auto& shard : m_mediator.m_ds->m_shards) {
      for (auto& node : shard) {
        string pubKey;
        if (!DataConversion::SerializableToHexStr(get<SHARD_NODE_PUBKEY>(node),
                                                  pubKey)) {
          LOG_GENERAL(WARNING, "Error converting pubkey to string");
          continue;
        }

        if (ipMapping.find(pubKey) != ipMapping.end()) {
          get<SHARD_NODE_PEER>(node) = ipMapping.at(pubKey);
        }
      }
    }
  }

  bool bInShardStructure = false;

  if (bDS) {
    m_myshardId = m_mediator.m_ds->m_shards.size();
  } else {
    for (unsigned int i = 0;
         i < m_mediator.m_ds->m_shards.size() && !bInShardStructure; ++i) {
      for (const auto& shardNode : m_mediator.m_ds->m_shards.at(i)) {
        if (get<SHARD_NODE_PUBKEY>(shardNode) == m_mediator.m_selfKey.second) {
          SetMyshardId(i);
          LOG_GENERAL(
              INFO, "This node belongs to sharding structure #" << m_myshardId);
          bInShardStructure = true;
          break;
        }
      }
    }
  }

  if (LOOKUP_NODE_MODE) {
    m_mediator.m_lookup->ProcessEntireShardingStructure();
  } else {
    LoadShardingStructure(true);
    m_mediator.m_ds->ProcessShardingStructure(
        m_mediator.m_ds->m_shards, m_mediator.m_ds->m_publicKeyToshardIdMap,
        m_mediator.m_ds->m_mapNodeReputation);
  }

  if (REJOIN_NODE_NOT_IN_NETWORK && !LOOKUP_NODE_MODE && !bDS &&
      !bInShardStructure) {
    LOG_GENERAL(WARNING,
                "Node " << m_mediator.m_selfKey.second
                        << " is not in network, apply re-join process instead");
    return false;
  }

  m_mediator.m_consensusID =
      (m_mediator.m_txBlockChain.GetBlockCount()) % NUM_FINAL_BLOCK_PER_POW;

  /// Save coin base for micro block, from last DS epoch to current TX epoch
  if (bDS && !(RECOVERY_TRIM_INCOMPLETED_BLOCK &&
               SyncType::RECOVERY_ALL_SYNC == syncType)) {
    std::list<MicroBlockSharedPtr> microBlocks;
    if (BlockStorage::GetBlockStorage().GetRangeMicroBlocks(
            m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetEpochNum(),
            m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
                1,
            0, m_mediator.m_ds->m_shards.size(), microBlocks)) {
      for (const auto& microBlock : microBlocks) {
        LOG_GENERAL(INFO,
                    "Retrieve microblock with epochNum: "
                        << microBlock->GetHeader().GetEpochNum()
                        << ", shardId: " << microBlock->GetHeader().GetShardId()
                        << ", reward: " << microBlock->GetHeader().GetRewards()
                        << " from persistence, and update coin base");
        m_mediator.m_ds->SaveCoinbase(microBlock->GetB1(), microBlock->GetB2(),
                                      microBlock->GetHeader().GetShardId(),
                                      microBlock->GetHeader().GetEpochNum());
      }
    }
  }

  bool res = false;

  if (st_result && ds_result && tx_result) {
    if (m_retriever->ValidateStates()) {
      if (!LOOKUP_NODE_MODE || m_retriever->CleanExtraTxBodies()) {
        LOG_GENERAL(INFO, "RetrieveHistory Success");
        m_mediator.m_isRetrievedHistory = true;
        res = true;
      }
    }
  }

  return res;
}

void Node::GetIpMapping(unordered_map<string, Peer>& ipMapping) {
  LOG_MARKER();

  if (!boost::filesystem::exists(IP_MAPPING_FILE_NAME)) {
    LOG_GENERAL(WARNING, IP_MAPPING_FILE_NAME << " not existed!");
    return;
  }

  using boost::property_tree::ptree;
  ptree pt;
  read_xml(IP_MAPPING_FILE_NAME, pt);
  struct in_addr ip_addr;

  for (const ptree::value_type& v : pt.get_child("mapping")) {
    if (v.first == "peer") {
      inet_pton(AF_INET, v.second.get<string>("ip").c_str(), &ip_addr);
      ipMapping[v.second.get<std::string>("pubkey")] =
          Peer((uint128_t)ip_addr.s_addr, v.second.get<uint32_t>("port"));
    }
  }
}

void Node::WakeupAtDSEpoch() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(INFO, "Lookup node, wakeup immediately.");
    return;
  }

  /// If this node is DS node, run DS consensus
  if (DirectoryService::IDLE != m_mediator.m_ds->m_mode) {
    SetState(POW_SUBMISSION);
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "START OF EPOCH " << m_mediator.m_dsBlockChain.GetLastBlock()
                                           .GetHeader()
                                           .GetBlockNum() +
                                       1);
    if (BROADCAST_GOSSIP_MODE) {
      VectorOfNode peers;
      std::vector<PubKey> pubKeys;
      m_mediator.m_ds->GetEntireNetworkPeerInfo(peers, pubKeys);

      P2PComm::GetInstance().InitializeRumorManager(peers, pubKeys);
    }

    auto func = [this]() mutable -> void {
      if ((m_consensusMyID < POW_PACKET_SENDERS) ||
          (m_mediator.m_ds->m_mode == DirectoryService::PRIMARY_DS)) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Waiting " << POW_WINDOW_IN_SECONDS
                             << " seconds, accepting PoW submissions...");
        this_thread::sleep_for(chrono::seconds(POW_WINDOW_IN_SECONDS));

        // create and send POW submission packets
        auto func2 = [this]() mutable -> void {
          m_mediator.m_ds->SendPoWPacketSubmissionToOtherDSComm();
        };
        DetachedFunction(1, func2);

        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Waiting "
                      << POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                      << " seconds, accepting PoW submissions packet from "
                         "other DS member...");
        this_thread::sleep_for(
            chrono::seconds(POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
      } else {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "Waiting "
                      << POW_WINDOW_IN_SECONDS +
                             POWPACKETSUBMISSION_WINDOW_IN_SECONDS
                      << " seconds, accepting PoW submissions packets...");
        this_thread::sleep_for(chrono::seconds(
            POW_WINDOW_IN_SECONDS + POWPACKETSUBMISSION_WINDOW_IN_SECONDS));
      }

      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Starting consensus on ds block");
      m_mediator.m_ds->RunConsensusOnDSBlock();
    };
    DetachedFunction(1, func);
    return;
  }

  /// If this node is shard node, start pow
  LOG_GENERAL(INFO, "Set as shard node: "
                        << m_mediator.m_selfPeer.GetPrintableIPAddress() << ":"
                        << m_mediator.m_selfPeer.m_listenPortHost);
  uint64_t block_num =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
  uint8_t dsDifficulty =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
  uint8_t difficulty =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
  SetState(POW_SUBMISSION);

  auto func = [this, block_num, dsDifficulty, difficulty]() mutable -> void {
    LOG_GENERAL(INFO, "Shard node, wait "
                          << SHARD_DELAY_WAKEUP_IN_SECONDS -
                                 WAIT_LOOKUP_WAKEUP_IN_SECONDS
                          << " more seconds for lookup and DS nodes wakeup...");
    this_thread::sleep_for(chrono::seconds(SHARD_DELAY_WAKEUP_IN_SECONDS -
                                           WAIT_LOOKUP_WAKEUP_IN_SECONDS));
    StartPoW(block_num, dsDifficulty, difficulty, m_mediator.m_dsBlockRand,
             m_mediator.m_txBlockRand);
  };
  DetachedFunction(1, func);
}

void Node::WakeupAtTxEpoch() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    return;
  }

  lock_guard<mutex> g(m_mutexShardMember);
  if (DirectoryService::IDLE != m_mediator.m_ds->m_mode) {
    m_myShardMembers = m_mediator.m_DSCommittee;
  }

  m_consensusLeaderID =
      DataConversion::charArrTo16Bits(
          m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes()) %
      m_myShardMembers->size();

  if (DirectoryService::IDLE != m_mediator.m_ds->m_mode) {
    if (BROADCAST_GOSSIP_MODE) {
      m_mediator.m_ds->m_forceMulticast = true;
      VectorOfNode peers;
      std::vector<PubKey> pubKeys;
      m_mediator.m_ds->GetEntireNetworkPeerInfo(peers, pubKeys);

      P2PComm::GetInstance().InitializeRumorManager(peers, pubKeys);
    }
    m_mediator.m_ds->SetState(
        DirectoryService::DirState::MICROBLOCK_SUBMISSION);
    auto func = [this]() mutable -> void {
      m_mediator.m_ds->RunConsensusOnFinalBlock();
    };
    DetachedFunction(1, func);
    return;
  }

  if (BROADCAST_GOSSIP_MODE) {
    VectorOfNode peers;
    std::vector<PubKey> pubKeys;
    GetEntireNetworkPeerInfo(peers, pubKeys);

    // Initialize every start of DS Epoch
    P2PComm::GetInstance().InitializeRumorManager(peers, pubKeys);
  }

  SetState(WAITING_FINALBLOCK);
}

bool Node::GetOfflineLookups(bool endless) {
  unsigned int counter = 1;
  while (!m_mediator.m_lookup->m_fetchedOfflineLookups &&
         (counter <= FETCH_LOOKUP_MSG_MAX_RETRY || endless)) {
    m_synchronizer.FetchOfflineLookups(m_mediator.m_lookup);

    {
      unique_lock<mutex> lock(
          m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
      if (m_mediator.m_lookup->cv_offlineLookups.wait_for(
              lock, chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
          std::cv_status::timeout) {
        if (!endless) {
          LOG_GENERAL(WARNING, "FetchOfflineLookups Timeout... tried "
                                   << counter << "/"
                                   << FETCH_LOOKUP_MSG_MAX_RETRY << " times");
          counter++;
        }
      } else {
        break;
      }
    }
  }
  if (!m_mediator.m_lookup->m_fetchedOfflineLookups) {
    LOG_GENERAL(WARNING, "Fetch offline lookup nodes failed");
    return false;
  }
  m_mediator.m_lookup->m_fetchedOfflineLookups = false;
  return true;
}

void Node::StartSynchronization() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::StartSynchronization not expected to be called from "
                "LookUp node.");
    return;
  }
  LOG_MARKER();

  SetState(SYNC);
  auto func = [this]() -> void {
    if (!GetOfflineLookups()) {
      LOG_GENERAL(WARNING, "Cannot rejoin currently");
      return;
    }

    while (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
      m_mediator.m_lookup->ComposeAndSendGetDirectoryBlocksFromSeed(
          m_mediator.m_blocklinkchain.GetLatestIndex() + 1);
      m_synchronizer.FetchLatestTxBlockSeed(
          m_mediator.m_lookup,
          // m_mediator.m_txBlockChain.GetBlockCount());
          m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
              1);
      this_thread::sleep_for(chrono::seconds(m_mediator.m_lookup->m_startedPoW
                                                 ? POW_WINDOW_IN_SECONDS
                                                 : NEW_NODE_SYNC_INTERVAL));
    }
  };

  DetachedFunction(1, func);
}

uint32_t Node::CalculateShardLeaderFromDequeOfNode(
    uint16_t lastBlockHash, uint32_t sizeOfShard,
    const DequeOfNode& shardMembers) {
  LOG_MARKER();
  if (GUARD_MODE) {
    uint32_t consensusLeaderIndex = lastBlockHash % sizeOfShard;

    unsigned int iterationCount = 0;
    while (!Guard::GetInstance().IsNodeInShardGuardList(
               shardMembers.at(consensusLeaderIndex).first) &&
           (iterationCount < SHARD_LEADER_SELECT_TOL)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "consensusLeaderIndex " << consensusLeaderIndex
                                        << " is not a shard guard.");
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      sha2.Update(DataConversion::IntegerToBytes<uint16_t, sizeof(uint16_t)>(
          lastBlockHash));
      lastBlockHash = DataConversion::charArrTo16Bits(sha2.Finalize());
      consensusLeaderIndex = lastBlockHash % sizeOfShard;
      iterationCount++;
    }
    return consensusLeaderIndex;
  } else {
    return lastBlockHash % sizeOfShard;
  }
}

uint32_t Node::CalculateShardLeaderFromShard(uint16_t lastBlockHash,
                                             uint32_t sizeOfShard,
                                             const Shard& shardMembers) {
  LOG_MARKER();
  if (GUARD_MODE) {
    uint32_t consensusLeaderIndex = lastBlockHash % sizeOfShard;

    unsigned int iterationCount = 0;
    while (!Guard::GetInstance().IsNodeInShardGuardList(
               std::get<SHARD_NODE_PUBKEY>(
                   shardMembers.at(consensusLeaderIndex))) &&
           (iterationCount < SHARD_LEADER_SELECT_TOL)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "consensusLeaderIndex " << consensusLeaderIndex
                                        << " is not a shard guard.");
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      sha2.Update(DataConversion::IntegerToBytes<uint16_t, sizeof(uint16_t)>(
          lastBlockHash));
      lastBlockHash = DataConversion::charArrTo16Bits(sha2.Finalize());
      consensusLeaderIndex = lastBlockHash % sizeOfShard;
      iterationCount++;
    }
    return consensusLeaderIndex;
  } else {
    return lastBlockHash % sizeOfShard;
  }
}

bool Node::CheckState(Action action) {
  if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE &&
      action != PROCESS_MICROBLOCKCONSENSUS) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "I am a DS node. Why am I getting this message? Action: "
                  << GetActionString(action));
    return false;
  }

  static const std::multimap<NodeState, Action> ACTIONS_FOR_STATE = {
      {POW_SUBMISSION, STARTPOW},
      {POW_SUBMISSION, PROCESS_DSBLOCK},
      {WAITING_DSBLOCK, PROCESS_DSBLOCK},
      {MICROBLOCK_CONSENSUS, PROCESS_MICROBLOCKCONSENSUS},
      {WAITING_FINALBLOCK, PROCESS_FINALBLOCK},
      {FALLBACK_CONSENSUS, PROCESS_FALLBACKCONSENSUS},
      {WAITING_FALLBACKBLOCK, PROCESS_FALLBACKBLOCK}};

  bool found = false;

  for (auto pos = ACTIONS_FOR_STATE.lower_bound(m_state);
       pos != ACTIONS_FOR_STATE.upper_bound(m_state); pos++) {
    if (pos->second == action) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              GetActionString(action)
                  << " not allowed in " << GetStateString());
    return false;
  }

  return true;
}

bool GetOneGenesisAddress(Address& oAddr) {
  if (GENESIS_WALLETS.empty()) {
    LOG_GENERAL(INFO, "could not get one genensis address");
    return false;
  }

  bytes oAddrBytes;
  if (!DataConversion::HexStrToUint8Vec(GENESIS_WALLETS.front(), oAddrBytes)) {
    LOG_GENERAL(INFO, "invalid genesis key");
    return false;
  }
  oAddr = Address{oAddrBytes};
  return true;
}

/// generate transation from one to many random accounts
/*vector<Transaction> GenTransactionBulk(PrivKey& fromPrivKey, PubKey&
fromPubKey, size_t n)
{
    vector<Transaction> txns;

    // FIXME: it's a workaround to use the first genensis account
    // auto receiver = Schnorr::GetInstance().GenKeyPair();
    // auto receiverAddr = Account::GetAddressFromPublicKey(receiver.second);

    // alternative 1: use first genesis address
    // Address addr;
    // if (not GetOneGenesisAddress(addr))
    // {
    // return txns;
    // }
    // auto receiverAddr = addr;

    // alternative 2: use a fresh address throughout entire lifetime
    auto receiverAddr = GenOneReceiver();

    txns.reserve(n);
    for (auto i = 0u; i != n; i++)
    {
        auto txn = CreateValidTestingTransaction(fromPrivKey, fromPubKey,
                                                 receiverAddr, i);
        txns.emplace_back(txn);
    }

    return txns;
}*/

bool Node::ProcessSubmitMissingTxn(const bytes& message, unsigned int offset,
                                   [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessSubmitMissingTxn not expected to be called "
                "from LookUp node.");
    return true;
  }

  if (offset >= message.size()) {
    LOG_GENERAL(WARNING, "Invalid txn message, message size: "
                             << message.size()
                             << ", txn data offset: " << offset);
    // TODO: Punish the node send invalid message
    return true;
  }

  unsigned int cur_offset = offset;

  auto msgBlockNum =
      Serializable::GetNumber<uint64_t>(message, offset, sizeof(uint64_t));
  cur_offset += sizeof(uint64_t);

  if (msgBlockNum != m_mediator.m_currentEpochNum) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "untimely delivery of "
                  << "missing txns. received: " << msgBlockNum
                  << " , local: " << m_mediator.m_currentEpochNum);
  }

  if (m_mediator.GetIsVacuousEpoch(msgBlockNum)) {
    LOG_GENERAL(WARNING, "Get missing txn from vacuous epoch, why?");
    return false;
  }

  std::vector<Transaction> txns;
  if (!Messenger::GetTransactionArray(message, cur_offset, txns)) {
    LOG_GENERAL(WARNING, "Messenger::GetTransactionArray failed.");
    return false;
  }

  lock_guard<mutex> g(m_mutexCreatedTransactions);
  for (const auto& submittedTxn : txns) {
    m_createdTxns.insert(submittedTxn);
  }

  cv_MicroBlockMissingTxn.notify_all();
  return true;
}

bool Node::ProcessSubmitTransaction(const bytes& message, unsigned int offset,
                                    [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessSubmitTransaction not expected to be called "
                "from LookUp node.");
    return true;
  }
  // This message is sent by my shard peers
  // Message = [204-byte transaction]

  LOG_MARKER();

  unsigned int cur_offset = offset;

  unsigned char submitTxnType = message[cur_offset];
  cur_offset += MessageOffset::INST;

  if (submitTxnType == SUBMITTRANSACTIONTYPE::MISSINGTXN) {
    if (m_mediator.m_ds->m_mode == DirectoryService::IDLE) {
      if (m_state != MICROBLOCK_CONSENSUS) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "As a shard node not in a microblock consensus state: don't "
                  "want missing txns")
        return false;
      }
    } else {
      if (m_mediator.m_ds->m_state != DirectoryService::FINALBLOCK_CONSENSUS) {
        LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                  "As a ds node not in a finalblock consensus state: don't "
                  "want missing txns");
        return false;
      }
    }

    ProcessSubmitMissingTxn(message, cur_offset, from);
  }
  return true;
}

bool Node::ProcessTxnPacketFromLookup([[gnu::unused]] const bytes& message,
                                      [[gnu::unused]] unsigned int offset,
                                      [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessTxnPacketFromLookup not expected to "
                "be called from LookUp node.");
    return true;
  }

  // check it's at inappropriate timing
  // vacuous epoch -> reject
  // new ds epoch but didn't received ds block yet -> buffer
  // else -> process
  if (m_mediator.GetIsVacuousEpoch()) {
    LOG_GENERAL(WARNING,
                "In vacuous epoch now, shouldn't accept any Txn Packet");
    return false;
  }

  uint64_t epochNumber = 0, dsBlockNum = 0;
  uint32_t shardId = 0;
  PubKey lookupPubKey;
  vector<Transaction> transactions;
  Signature signature;

  if (!Messenger::GetNodeForwardTxnBlock(message, offset, epochNumber,
                                         dsBlockNum, shardId, lookupPubKey,
                                         transactions, signature)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeForwardTxnBlock failed.");
    return false;
  }

  if (!Lookup::VerifySenderNode(m_mediator.m_lookup->GetLookupNodesStatic(),
                                lookupPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Sender pubkey " << lookupPubKey << " not in lookup list");
    return false;
  }

  LOG_GENERAL(INFO, "Received from " << from);

  // Avoid using the original message for broadcasting in case it contains
  // excess data beyond the TxnPacket
  bytes message2 = {MessageType::NODE, NodeInstructionType::FORWARDTXNPACKET};
  if (!Messenger::SetNodeForwardTxnBlock(
          message2, MessageOffset::BODY, epochNumber, dsBlockNum, shardId,
          lookupPubKey, transactions, signature)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetNodeForwardTxnBlock failed.");
    return false;
  }

  {
    // The check here is in case the lookup send the packet
    // earlier than the node receiving DS block, need to wait the
    // node finish processing DS block and update its sharding structure

    lock_guard<mutex> g1(m_mutexDSBlock);

    // Situation 1:
    // Epoch later than genesis epoch, two sub situations:
    // a : Normal DS Block (after vacuous epoch)
    // b : DS Block after fallback
    // Situation 2:
    // Genesis Epoch 1, two sub situations:
    // a : Normal DS Block (after genesis)
    // b : Fallback happened in epoch 1 when waiting for finalblock
    if ((((m_mediator.m_currentEpochNum % NUM_FINAL_BLOCK_PER_POW == 0) ||
          m_justDidFallback) &&
         (m_mediator.m_consensusID != 0)) ||
        ((m_mediator.m_currentEpochNum == 1) &&
         ((m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() ==
           0) ||
          m_justDidFallback))) {
      lock_guard<mutex> g2(m_mutexTxnPacketBuffer);
      m_txnPacketBuffer.emplace_back(message2);
      return true;
    }
  }

  bool isLookup = m_mediator.m_lookup->IsLookupNode(from) &&
                  from.GetPrintableIPAddress() != "127.0.0.1";

  bool properState =
      (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE &&
       m_mediator.m_ds->m_state == DirectoryService::MICROBLOCK_SUBMISSION) ||
      (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE &&
       m_txn_distribute_window_open &&
       (m_state == MICROBLOCK_CONSENSUS_PREP ||
        m_state == MICROBLOCK_CONSENSUS));

  if (isLookup || !properState) {
    if ((epochNumber + (isLookup ? 0 : 1)) < m_mediator.m_currentEpochNum) {
      LOG_GENERAL(WARNING, "Txn packet from older epoch, discard");
      return false;
    }
    lock_guard<mutex> g(m_mutexTxnPacketBuffer);
    LOG_GENERAL(INFO, string(isLookup ? "Received txn from lookup"
                                      : "Received not in the prepared state") +
                          ", store to buffer");
    LOG_STATE("[TXNPKTPROC]["
              << std::setw(15) << std::left
              << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
              << m_mediator.m_currentEpochNum << "][" << shardId << "]["
              << string(lookupPubKey).substr(0, 6) << "][" << message2.size()
              << "] RECVFROMLOOKUP");
    m_txnPacketBuffer.emplace_back(message2);
  } else {
    LOG_GENERAL(INFO,
                "Packet received from a non-lookup node, "
                "should be from gossip neighbor and process it");
    return ProcessTxnPacketFromLookupCore(message2, epochNumber, dsBlockNum,
                                          shardId, lookupPubKey, transactions);
  }

  return true;
}

bool Node::ProcessTxnPacketFromLookupCore(const bytes& message,
                                          const uint64_t& epochNum,
                                          const uint64_t& dsBlockNum,
                                          const uint32_t& shardId,
                                          const PubKey& lookupPubKey,
                                          const vector<Transaction>& txns) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessTxnPacketFromLookupCore not expected to "
                "be called from LookUp node.");
    return true;
  }

  if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
    LOG_GENERAL(WARNING, "This node already started rejoin, ignore txn packet");
    return false;
  }

  if (epochNum + PACKET_EPOCH_LATE_ALLOW < m_mediator.m_currentEpochNum) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The epoch when the packet from is too late (" << epochNum
                                                             << "), reject");
    return false;
  }

  if (dsBlockNum !=
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum()) {
    LOG_GENERAL(WARNING, "Wrong DS block num ("
                             << dsBlockNum << "), expected ("
                             << m_mediator.m_dsBlockChain.GetLastBlock()
                                    .GetHeader()
                                    .GetBlockNum()
                             << ")");
    return false;
  }

  if (shardId != m_myshardId) {
    LOG_GENERAL(WARNING, "Wrong Shard (" << shardId << "), m_myshardId ("
                                         << m_myshardId << ")");
    return false;
  }

  if (BROADCAST_GOSSIP_MODE) {
    LOG_STATE("[TXNPKTPROC-CORE]["
              << std::setw(15) << std::left
              << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
              << m_mediator.m_currentEpochNum << "][" << shardId << "]["
              << string(lookupPubKey).substr(0, 6) << "][" << message.size()
              << "] BEGN");
    if (P2PComm::GetInstance().SpreadRumor(message)) {
      LOG_STATE("[TXNPKTPROC-INITIATE]["
                << std::setw(15) << std::left
                << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                << m_mediator.m_currentEpochNum << "][" << shardId << "]["
                << string(lookupPubKey).substr(0, 6) << "][" << message.size()
                << "] BEGN");
    } else {
      LOG_STATE("[TXNPKTPROC]["
                << std::setw(15) << std::left
                << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                << m_mediator.m_currentEpochNum << "][" << shardId << "]["
                << string(lookupPubKey).substr(0, 6) << "][" << message.size()
                << "] BEGN");
    }
  } else {
    vector<Peer> toSend;
    {
      lock_guard<mutex> g(m_mutexShardMember);
      for (auto& it : *m_myShardMembers) {
        toSend.push_back(it.second);
      }
    }
    LOG_GENERAL(INFO, "[Batching] Broadcast my txns to other shard members");

    P2PComm::GetInstance().SendBroadcastMessage(toSend, message);
  }

#ifdef DM_TEST_DM_LESSTXN_ONE
  uint32_t dm_test_id = (m_mediator.m_ds->GetConsensusLeaderID() + 1) %
                        m_mediator.m_DSCommittee->size();
  LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
            "Consensus ID for DM1 test is " << dm_test_id);
  if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE &&
      m_mediator.m_ds->GetConsensusMyID() == dm_test_id) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Letting one of the backups accept less txns from lookup "
              "comparing to the others (DM_TEST_DM_LESSTXN_ONE)");
    return false;
  } else {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The node triggered DM_TEST_DM_LESSTXN_ONE is "
                  << m_mediator.m_DSCommittee->at(dm_test_id).second);
  }
#endif  // DM_TEST_DM_LESSTXN_ONE

#ifdef DM_TEST_DM_LESSTXN_ALL
  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::BACKUP_DS) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Letting all of the backups accept less txns from lookup "
              "comparing to the leader (DM_TEST_DM_LESSTXN_ALL)");
    return false;
  }
#endif  // DM_TEST_DM_LESSTXN_ALL

#ifdef DM_TEST_DM_MORETXN_LEADER
  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::PRIMARY_DS) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "I the DS leader triggered DM_TEST_DM_MORETXN_LEADER");
    return false;
  }
#endif  // DM_TEST_DM_MORETXN_LEADER

#ifdef DM_TEST_DM_MORETXN_HALF
  if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE &&
      (m_mediator.m_ds->m_mode == DirectoryService::Mode::PRIMARY_DS ||
       (m_mediator.m_ds->GetConsensusMyID() % 2 == 0))) {
    if (m_mediator.m_ds->m_mode == DirectoryService::Mode::PRIMARY_DS) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "I the DS leader triggered DM_TEST_DM_MORETXN_HALF");
    } else {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "My consensus id " << m_mediator.m_ds->GetConsensusMyID()
                                   << " triggered DM_TEST_DM_MORETXN_HALF");
    }
    return false;
  }
#endif  // DM_TEST_DM_MORETXN_HALF

  // Process the txns
  unsigned int processed_count = 0;

  LOG_GENERAL(INFO, "Start check txn packet from lookup");

  std::vector<Transaction> checkedTxns;
  for (const auto& txn : txns) {
    if (m_mediator.GetIsVacuousEpoch()) {
      LOG_GENERAL(WARNING, "Already in vacuous epoch, stop proc txn");
      return false;
    }
    if (m_mediator.m_validator->CheckCreatedTransactionFromLookup(txn)) {
      checkedTxns.push_back(txn);
    } else {
      LOG_GENERAL(WARNING, "Txn is not valid.");
    }

    processed_count++;

    if (processed_count % 100 == 0) {
      LOG_GENERAL(INFO, processed_count << " txns from packet processed");
    }
  }

  {
    lock_guard<mutex> g(m_mutexCreatedTransactions);
    LOG_GENERAL(INFO,
                "TxnPool size before processing: " << m_createdTxns.size());

    for (const auto& txn : checkedTxns) {
      m_createdTxns.insert(txn);
    }

    LOG_GENERAL(INFO, "Txn processed: " << processed_count
                                        << " TxnPool size after processing: "
                                        << m_createdTxns.size());
  }

  LOG_STATE("[TXNPKTPROC][" << std::setw(15) << std::left
                            << m_mediator.m_selfPeer.GetPrintableIPAddress()
                            << "][" << m_mediator.m_currentEpochNum << "]["
                            << shardId << "]["
                            << string(lookupPubKey).substr(0, 6) << "] DONE ["
                            << processed_count << "]");
  return true;
}

bool Node::ProcessProposeGasPrice([[gnu::unused]] const bytes& message,
                                  [[gnu::unused]] unsigned int offset,
                                  [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessProposeGasPrice not expected to "
                "be called from LookUp node.");
    return true;
  }

  if (IsMessageSizeInappropriate(message.size(), offset, UINT128_SIZE)) {
    LOG_GENERAL(WARNING,
                "Message size for ProcessProposeGasPrice is too short");
    return false;
  }

  if (string(from.GetPrintableIPAddress()) != LEGAL_GAS_PRICE_IP) {
    LOG_GENERAL(WARNING, "Sender " << from << " is not from localhost");
    return false;
  }

  lock(m_mutexDSBlock, m_mutexGasPrice);
  lock_guard<mutex> g(m_mutexDSBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexGasPrice, adopt_lock);

  uint128_t gasPriceProposal =
      Serializable::GetNumber<uint128_t>(message, offset, UINT128_SIZE);
  offset += UINT128_SIZE;
  LOG_GENERAL(INFO, "Received gas price proposal: " << gasPriceProposal
                                                    << " Current GasPrice "
                                                    << m_proposedGasPrice);
  m_proposedGasPrice = max(m_proposedGasPrice, gasPriceProposal);
  LOG_GENERAL(INFO, "Newly proposed gas price: " << m_proposedGasPrice);

  return true;
}

void Node::CommitTxnPacketBuffer() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::CommitTxnPacketBuffer not expected to "
                "be called from LookUp node.");
    return;
  }

  lock_guard<mutex> g(m_mutexTxnPacketBuffer);
  for (const auto& message : m_txnPacketBuffer) {
    uint64_t epochNumber = 0, dsBlockNum = 0;
    uint32_t shardId = 0;
    PubKey lookupPubKey;
    vector<Transaction> transactions;
    Signature signature;

    if (!Messenger::GetNodeForwardTxnBlock(
            message, MessageOffset::BODY, epochNumber, dsBlockNum, shardId,
            lookupPubKey, transactions, signature)) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Messenger::GetNodeForwardTxnBlock failed.");
      return;
    }

    ProcessTxnPacketFromLookupCore(message, epochNumber, dsBlockNum, shardId,
                                   lookupPubKey, transactions);
  }
  m_txnPacketBuffer.clear();
}

// Used by Zilliqa in pow branch. This will be useful for us when doing the
// accounts and wallet in the future. bool Node::ProcessCreateAccounts(const
// bytes & message, unsigned int offset, const Peer & from)
// {
// #ifndef IS_LOOKUP_NODE
//     // Message = [117-byte account 1] ... [117-byte account n]

//     LOG_MARKER();

//     if (IsMessageSizeInappropriate(message.size(), offset, 0, ACCOUNT_SIZE))
//     {
//         return false;
//     }

//     const unsigned int numOfAccounts = (message.size() - offset) /
//     ACCOUNT_SIZE; unsigned int cur_offset = offset;

//     for (unsigned int i = 0; i < numOfAccounts; i++)
//     {
//         AccountStore::GetInstance().AddAccount(Account(message, cur_offset));
//         cur_offset += ACCOUNT_SIZE;
//     }
// #endif // IS_LOOKUP_NODE
//     // Do any post-processing on the final block
//     return true;
// }

void Node::SetState(NodeState state) {
  m_state = state;
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Node State = " << GetStateString());
}

// Set m_consensusMyID
void Node::SetConsensusMyID(uint16_t id) { m_consensusMyID = id; }

// Get m_consensusMyID
uint16_t Node::GetConsensusMyID() const { return m_consensusMyID.load(); }

// Set m_consensusLeaderID
void Node::SetConsensusLeaderID(uint16_t id) { m_consensusLeaderID = id; }

// Get m_consensusLeaderID
uint16_t Node::GetConsensusLeaderID() const {
  return m_consensusLeaderID.load();
}

void Node::AddBlock(const TxBlock& block) {
  m_mediator.m_txBlockChain.AddBlock(block);
}

void Node::RejoinAsNormal() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::RejoinAsNormal not expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();
  if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
    auto func = [this]() mutable -> void {
      while (true) {
        m_mediator.m_lookup->SetSyncType(SyncType::NORMAL_SYNC);
        this->CleanVariables();
        this->m_mediator.m_ds->CleanVariables();
        while (!this->DownloadPersistenceFromS3()) {
          LOG_GENERAL(
              WARNING,
              "Downloading persistence from S3 has failed. Will try again!");
          this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
        }
        if (!BlockStorage::GetBlockStorage().RefreshAll()) {
          LOG_GENERAL(WARNING, "BlockStorage::RefreshAll failed");
          return;
        }
        if (!AccountStore::GetInstance().RefreshDB()) {
          LOG_GENERAL(WARNING, "AccountStore::RefreshDB failed");
          return;
        }
        if (this->Install(SyncType::NORMAL_SYNC, true, true)) {
          break;
        };
        this_thread::sleep_for(chrono::seconds(RETRY_REJOINING_TIMEOUT));
      }
      this->StartSynchronization();
    };
    DetachedFunction(1, func);
  }
}

void Node::ResetRejoinFlags() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ResetRejoinFlags not expected to be called from "
                "LookUp node.");
    return;
  }

  m_doRejoinAtNextRound = false;
  m_doRejoinAtStateRoot = false;
  m_doRejoinAtFinalBlock = false;

  m_mediator.m_ds->m_doRejoinAtDSConsensus = false;
  m_mediator.m_ds->m_doRejoinAtFinalConsensus = false;
}

bool Node::CleanVariables() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::CleanVariables not expected to be called from LookUp node.");
    return true;
  }

  FallbackStop();
  AccountStore::GetInstance().InitSoft();
  {
    lock_guard<mutex> g(m_mutexShardMember);
    m_myShardMembers.reset(new DequeOfNode);
  }
  m_isPrimary = false;
  m_stillMiningPrimary = false;
  m_myshardId = 0;
  m_proposedGasPrice = PRECISION_MIN_VALUE;
  CleanCreatedTransaction();
  CleanMicroblockConsensusBuffer();
  P2PComm::GetInstance().InitializeRumorManager({}, {});
  this->ResetRejoinFlags();

  {
    std::lock_guard<mutex> lock(m_mutexConsensus);
    m_consensusObject.reset();
  }

  m_consensusBlockHash.clear();
  {
    std::lock_guard<mutex> lock(m_mutexMicroBlock);
    m_microblock.reset();
    m_gasUsedTotal = 0;
    m_txnFees = 0;
  }
  // {
  //     std::lock_guard<mutex> lock(m_mutexCommittedTransactions);
  //     m_committedTransactions.clear();
  // }
  {
    std::lock_guard<mutex> lock(m_mutexUnavailableMicroBlocks);
    m_unavailableMicroBlocks.clear();
  }
  // On Lookup
  {
    std::lock_guard<mutex> lock(
        m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
    m_mediator.m_lookup->m_fetchedOfflineLookups = false;
  }
  m_mediator.m_lookup->m_startedPoW = false;

  return true;
}

void Node::SetMyshardId(uint32_t shardId) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::SetMyshardId not expected to be called from LookUp node.");
    return;
  }
  m_myshardId = shardId;
}

void Node::CleanCreatedTransaction() {
  LOG_MARKER();
  {
    std::lock_guard<mutex> g(m_mutexCreatedTransactions);
    m_createdTxns.clear();
    t_createdTxns.clear();
  }
  {
    std::lock_guard<mutex> g(m_mutexTxnPacketBuffer);
    m_txnPacketBuffer.clear();
  }
  {
    std::lock_guard<mutex> lock(m_mutexProcessedTransactions);
    m_processedTransactions.clear();
    t_processedTransactions.clear();
  }
  {
    std::unique_lock<shared_timed_mutex> lock(m_unconfirmedTxnsMutex);
    m_unconfirmedTxns.clear();
  }
  m_TxnOrder.clear();
  m_gasUsedTotal = 0;
  m_txnFees = 0;
}

bool Node::IsShardNode(const PubKey& pubKey) {
  lock_guard<mutex> lock(m_mutexShardMember);
  return std::find_if(m_myShardMembers->begin(), m_myShardMembers->end(),
                      [&pubKey](const PairOfNode& node) {
                        return node.first == pubKey;
                      }) != m_myShardMembers->end();
}

bool Node::IsShardNode(const Peer& peerInfo) {
  lock_guard<mutex> lock(m_mutexShardMember);
  return std::find_if(m_myShardMembers->begin(), m_myShardMembers->end(),
                      [&peerInfo](const PairOfNode& node) {
                        return node.second.GetIpAddress() ==
                               peerInfo.GetIpAddress();
                      }) != m_myShardMembers->end();
}

bool Node::ProcessDoRejoin(const bytes& message, unsigned int offset,
                           [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessDoRejoin not expected to be called from "
                "LookUp node.");
    return true;
  }

  LOG_MARKER();

  if (!ENABLE_DO_REJOIN) {
    return false;
  }

  if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
    LOG_GENERAL(WARNING, "Already in rejoining!");
    return false;
  }

  unsigned int cur_offset = offset;

  if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                 MessageOffset::INST)) {
    return false;
  }

  unsigned char rejoinType = message[cur_offset];
  cur_offset += MessageOffset::INST;

  switch (rejoinType) {
    case REJOINTYPE::ATFINALBLOCK:
      m_doRejoinAtFinalBlock = true;
      break;
    case REJOINTYPE::ATNEXTROUND:
      m_doRejoinAtNextRound = true;
      break;
    case REJOINTYPE::ATSTATEROOT:
      m_doRejoinAtStateRoot = true;
      break;
    case REJOINTYPE::ATDSCONSENSUS:
      m_mediator.m_ds->m_doRejoinAtDSConsensus = true;
      break;
    case REJOINTYPE::ATFINALCONSENSUS:
      m_mediator.m_ds->m_doRejoinAtFinalConsensus = true;
      break;
    default:
      return false;
  }
  return true;
}

void Node::QueryLookupForDSGuardNetworkInfoUpdate() {
  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING,
                "Not in guard mode. Unable to query from lookup for ds guard "
                "network information update.");
    return;
  }

  LOG_MARKER();

  bytes queryLookupForDSGuardNetworkInfoUpdate = {
      MessageType::LOOKUP,
      LookupInstructionType::GETGUARDNODENETWORKINFOUPDATE};
  uint64_t dsEpochNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  LOG_GENERAL(INFO,
              "Querying the lookup for any ds guard node network info change "
              "for ds epoch "
                  << dsEpochNum);

  if (!Messenger::SetLookupGetNewDSGuardNetworkInfoFromLookup(
          queryLookupForDSGuardNetworkInfoUpdate, MessageOffset::BODY,
          m_mediator.m_selfPeer.m_listenPortHost, dsEpochNum,
          m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetLookupGetNewDSGuardNetworkInfoFromLookup failed.");
    return;
  }
  m_requestedForDSGuardNetworkInfoUpdate = true;
  m_mediator.m_lookup->SendMessageToRandomSeedNode(
      queryLookupForDSGuardNetworkInfoUpdate);
}

bool Node::ProcessDSGuardNetworkInfoUpdate(const bytes& message,
                                           unsigned int offset,
                                           [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "Node::ProcessDSGuardNetworkInfoUpdate not expected to be called from "
        "LookUp node.");
    return true;
  }

  if (!GUARD_MODE) {
    LOG_GENERAL(WARNING,
                "Not in guard mode. Unable to process from lookup for ds guard "
                "network information update.");
    return false;
  }

  if (!m_requestedForDSGuardNetworkInfoUpdate) {
    LOG_GENERAL(WARNING,
                "Did not request for DS Guard node network info update");
    return false;
  }

  LOG_MARKER();

  vector<DSGuardUpdateStruct> vecOfDSGuardUpdateStruct;
  PubKey lookupPubkey;
  if (!Messenger::SetNodeGetNewDSGuardNetworkInfo(
          message, offset, vecOfDSGuardUpdateStruct, lookupPubkey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetNodeGetNewDSGuardNetworkInfo failed.");
    return false;
  }

  if (!Lookup::VerifySenderNode(m_mediator.m_lookup->GetSeedNodes(),
                                lookupPubkey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The message sender pubkey: "
                  << lookupPubkey << " is not in my lookup node list.");
    return false;
  }

  LOG_GENERAL(INFO, "Received from lookup " << from);

  {
    lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
    for (const auto& dsguardupdate : vecOfDSGuardUpdateStruct) {
      // Remove old ds guard IP info from blacklist exclude list
      if (GUARD_MODE) {
        auto it =
            find_if(m_mediator.m_DSCommittee->begin(),
                    m_mediator.m_DSCommittee->begin() +
                        Guard::GetInstance().GetNumOfDSGuard(),
                    [&dsguardupdate](const PairOfNode& element) {
                      return element.first == dsguardupdate.m_dsGuardPubkey;
                    });

        if (it != m_mediator.m_DSCommittee->end()) {
          Blacklist::GetInstance().RemoveExclude(it->second.m_ipAddress);
          LOG_GENERAL(INFO, "Removed " << it->second.m_ipAddress
                                       << " from blacklist exclude list");
        }
      }

      // Process and update ds committee network info
      replace_if(m_mediator.m_DSCommittee->begin(),
                 m_mediator.m_DSCommittee->begin() +
                     Guard::GetInstance().GetNumOfDSGuard(),
                 [&dsguardupdate](const PairOfNode& element) {
                   return element.first == dsguardupdate.m_dsGuardPubkey;
                 },
                 make_pair(dsguardupdate.m_dsGuardPubkey,
                           dsguardupdate.m_dsGuardNewNetworkInfo));
      LOG_GENERAL(INFO, "[update ds guard] "
                            << dsguardupdate.m_dsGuardPubkey
                            << " new network info is "
                            << dsguardupdate.m_dsGuardNewNetworkInfo)
      if (GUARD_MODE) {
        Blacklist::GetInstance().Exclude(
            dsguardupdate.m_dsGuardNewNetworkInfo.m_ipAddress);
        LOG_GENERAL(INFO,
                    "Added ds guard "
                        << dsguardupdate.m_dsGuardNewNetworkInfo.m_ipAddress
                        << " to blacklist exclude list");
      }
    }
  }

  m_requestedForDSGuardNetworkInfoUpdate = false;
  return true;
}

bool Node::ToBlockMessage([[gnu::unused]] unsigned char ins_byte) {
  if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
    if (!LOOKUP_NODE_MODE) {
      if (m_mediator.m_lookup->GetSyncType() == SyncType::DS_SYNC) {
        return true;
      } else if (m_mediator.m_lookup->GetSyncType() ==
                     SyncType::GUARD_DS_SYNC &&
                 GUARD_MODE) {
        return true;
      }
      if (!m_fromNewProcess) {
        if (ins_byte != NodeInstructionType::DSBLOCK &&
            ins_byte != NodeInstructionType::FORWARDTXNPACKET) {
          return true;
        }
      } else {
        if (m_runFromLate && ins_byte != NodeInstructionType::DSBLOCK &&
            ins_byte != NodeInstructionType::FORWARDTXNPACKET) {
          return true;
        }
      }
    } else if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP &&
               ins_byte == NodeInstructionType::FINALBLOCK)  // Is seed node
    {
      return false;
    } else  // Is lookup node
    {
      return true;
    }
  }
  return false;
}

void Node::GetNodesToBroadCastUsingTreeBasedClustering(
    uint32_t cluster_size, uint32_t num_of_child_clusters, uint32_t& nodes_lo,
    uint32_t& nodes_hi) {
  // make sure cluster_size is with-in the valid range
  cluster_size = std::max(cluster_size, MIN_CLUSTER_SIZE);
  cluster_size = std::min(cluster_size, (uint32_t)m_myShardMembers->size());

  uint32_t num_of_total_clusters =
      std::ceil((double)m_myShardMembers->size() / cluster_size);

  // make sure child_cluster_size is within valid range
  num_of_child_clusters =
      std::max(num_of_child_clusters, MIN_CHILD_CLUSTER_SIZE);
  num_of_child_clusters =
      std::min(num_of_child_clusters, num_of_total_clusters - 1);

  uint32_t my_cluster_num = m_consensusMyID / cluster_size;

  LOG_GENERAL(INFO, "cluster_size :"
                        << cluster_size
                        << ", num_of_child_clusters : " << num_of_child_clusters
                        << ", num_of_total_clusters : " << num_of_total_clusters
                        << ", my_cluster_num : " << my_cluster_num);

  nodes_lo = (my_cluster_num * num_of_child_clusters + 1) * cluster_size;
  nodes_hi =
      ((my_cluster_num + 1) * num_of_child_clusters + 1) * cluster_size - 1;
}

// Tree-Based Clustering decision
//  --  Should I broadcast the message to some-one from my shard.
//  --  If yes, To whom-all should i broadcast the message.
void Node::SendBlockToOtherShardNodes(const bytes& message,
                                      uint32_t cluster_size,
                                      uint32_t num_of_child_clusters) {
  LOG_MARKER();

  uint32_t nodes_lo, nodes_hi;

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
  sha256.Update(message);  // raw_message hash
  bytes this_msg_hash = sha256.Finalize();

  lock_guard<mutex> g(m_mutexShardMember);

  GetNodesToBroadCastUsingTreeBasedClustering(
      cluster_size, num_of_child_clusters, nodes_lo, nodes_hi);

  string hashStr;
  if (!DataConversion::Uint8VecToHexStr(this_msg_hash, hashStr)) {
    return;
  }

  std::vector<Peer> shardBlockReceivers;
  if (nodes_lo >= m_myShardMembers->size()) {
    // I am at last level in tree.
    LOG_GENERAL(INFO,
                "Terminating broadcast for [" << hashStr.substr(0, 6) << "]");
    return;
  }

  // set to max valid node index, if upperbound is invalid.
  nodes_hi = std::min(nodes_hi, (uint32_t)m_myShardMembers->size() - 1);

  LOG_GENERAL(INFO, "Broadcasting [" << hashStr.substr(0, 6) << "] to "
                                     << nodes_hi - nodes_lo + 1 << " peers "
                                     << "(" << nodes_lo << "~" << nodes_hi
                                     << ")");

  for (uint32_t i = nodes_lo; i <= nodes_hi; i++) {
    const auto& kv = m_myShardMembers->at(i);
    shardBlockReceivers.emplace_back(std::get<SHARD_NODE_PEER>(kv));
    LOG_GENERAL(INFO, "[" << PAD(i, 3, ' ') << "] "
                          << std::get<SHARD_NODE_PUBKEY>(kv) << " "
                          << std::get<SHARD_NODE_PEER>(kv));
  }
  P2PComm::GetInstance().SendBroadcastMessage(shardBlockReceivers, message);
}

bool Node::Execute(const bytes& message, unsigned int offset,
                   const Peer& from) {
  // LOG_MARKER();

  bool result = true;

  typedef bool (Node::*InstructionHandler)(const bytes&, unsigned int,
                                           const Peer&);

  InstructionHandler ins_handlers[] = {
      &Node::ProcessStartPoW,
      &Node::ProcessVCDSBlocksMessage,
      &Node::ProcessSubmitTransaction,
      &Node::ProcessMicroBlockConsensus,
      &Node::ProcessFinalBlock,
      &Node::ProcessMBnForwardTransaction,
      &Node::ProcessVCBlock,
      &Node::ProcessDoRejoin,
      &Node::ProcessTxnPacketFromLookup,
      &Node::ProcessFallbackConsensus,
      &Node::ProcessFallbackBlock,
      &Node::ProcessProposeGasPrice,
      &Node::ProcessDSGuardNetworkInfoUpdate,
  };

  const unsigned char ins_byte = message.at(offset);
  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  // If the node failed and waiting for recovery, block the unwanted msg
  if (ToBlockMessage(ins_byte)) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Node not connected to network yet, ignore message");
    return false;
  }

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);
    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unknown instruction byte " << hex << (unsigned int)ins_byte
                                          << " from " << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
  }

  return result;
}

#define MAKE_LITERAL_PAIR(s) \
  { s, #s }

map<Node::NodeState, string> Node::NodeStateStrings = {
    MAKE_LITERAL_PAIR(POW_SUBMISSION),
    MAKE_LITERAL_PAIR(WAITING_DSBLOCK),
    MAKE_LITERAL_PAIR(MICROBLOCK_CONSENSUS_PREP),
    MAKE_LITERAL_PAIR(MICROBLOCK_CONSENSUS),
    MAKE_LITERAL_PAIR(WAITING_FINALBLOCK),
    MAKE_LITERAL_PAIR(WAITING_FALLBACKBLOCK),
    MAKE_LITERAL_PAIR(FALLBACK_CONSENSUS_PREP),
    MAKE_LITERAL_PAIR(FALLBACK_CONSENSUS),
    MAKE_LITERAL_PAIR(SYNC)};

string Node::GetStateString() const {
  if (NodeStateStrings.find(m_state) == NodeStateStrings.end()) {
    return "Unknown";
  } else {
    return NodeStateStrings.at(m_state);
  }
}

map<Node::Action, string> Node::ActionStrings = {
    MAKE_LITERAL_PAIR(STARTPOW),
    MAKE_LITERAL_PAIR(PROCESS_DSBLOCK),
    MAKE_LITERAL_PAIR(PROCESS_MICROBLOCKCONSENSUS),
    MAKE_LITERAL_PAIR(PROCESS_FINALBLOCK),
    MAKE_LITERAL_PAIR(PROCESS_TXNBODY),
    MAKE_LITERAL_PAIR(PROCESS_FALLBACKCONSENSUS),
    MAKE_LITERAL_PAIR(PROCESS_FALLBACKBLOCK),
    MAKE_LITERAL_PAIR(NUM_ACTIONS)};

std::string Node::GetActionString(Action action) const {
  return (ActionStrings.find(action) == ActionStrings.end())
             ? "UNKNOWN"
             : ActionStrings.at(action);
}

bool Node::GetDSLeader(const BlockLink& lastBlockLink,
                       const DSBlock& latestDSBlock,
                       const DequeOfNode& dsCommittee, PairOfNode& dsLeader) {
  const auto& blocktype = get<BlockLinkIndex::BLOCKTYPE>(lastBlockLink);
  if (blocktype == BlockType::DS) {
    uint16_t lastBlockHash = 0;
    // To cater for boostrap of blockchain. The zero and first epoch the DS
    // leader is at index0
    if (latestDSBlock.GetHeader().GetBlockNum() > 1) {
      lastBlockHash = DataConversion::charArrTo16Bits(
          latestDSBlock.GetHeader().GetHashForRandom().asBytes());
    }

    uint32_t leader_id = 0;
    if (!GUARD_MODE) {
      leader_id = lastBlockHash % dsCommittee.size();
    } else {
      leader_id = lastBlockHash % Guard::GetInstance().GetNumOfDSGuard();
    }
    dsLeader = make_pair(dsCommittee.at(leader_id).first,
                         dsCommittee.at(leader_id).second);
    LOG_GENERAL(INFO, "lastBlockHash = " << lastBlockHash);
    LOG_GENERAL(INFO, "DS leader ID  = " << leader_id);
    LOG_GENERAL(INFO, "Leader PubKey = " << dsLeader.first);
    LOG_GENERAL(INFO, "Leader Peer   = " << dsLeader.second);
  } else if (blocktype == BlockType::VC) {
    VCBlockSharedPtr VCBlockptr;
    if (!BlockStorage::GetBlockStorage().GetVCBlock(
            get<BlockLinkIndex::BLOCKHASH>(lastBlockLink), VCBlockptr)) {
      LOG_GENERAL(WARNING, "Failed to get VC block");
      return false;
    } else {
      dsLeader =
          make_pair(VCBlockptr->GetHeader().GetCandidateLeaderPubKey(),
                    VCBlockptr->GetHeader().GetCandidateLeaderNetworkInfo());
    }
  } else {
    return false;
  }
  return true;
}

void Node::GetEntireNetworkPeerInfo(VectorOfNode& peers,
                                    std::vector<PubKey>& pubKeys) {
  peers.clear();
  pubKeys.clear();

  for (const auto& i : *m_myShardMembers) {
    if (i.second.m_listenPortHost != 0) {
      peers.emplace_back(i);
      // Get the pubkeys for my shard member
      pubKeys.emplace_back(i.first);
    }
  }

  // Get the pubkeys for ds committee
  for (const auto& i : *m_mediator.m_DSCommittee) {
    pubKeys.emplace_back(i.first);
  }

  // Get the pubKeys for lookup nodes
  for (const auto& i : m_mediator.m_lookup->GetLookupNodes()) {
    pubKeys.emplace_back(i.first);
  }
}
