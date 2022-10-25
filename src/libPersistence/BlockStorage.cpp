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

#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <leveldb/db.h>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "BlockStorage.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libMessage/Messenger.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"

using namespace std;

BlockStorage& BlockStorage::GetBlockStorage(const std::string& path,
                                            bool diagnostic) {
  static BlockStorage bs(path, diagnostic);
  return bs;
}

void BlockStorage::Initialize(const std::string& path, bool diagnostic) {
  m_metadataDB = std::make_shared<LevelDB>("metadata");

  m_dsBlockchainDB = std::make_shared<LevelDB>("dsBlocks");
  m_txBlockchainDB = std::make_shared<LevelDB>("txBlocks");
  m_txBlockchainAuxDB = std::make_shared<LevelDB>("txBlocksAux");
  m_txBlockHashToNumDB = std::make_shared<LevelDB>("txBlockHashToNum");
  m_microBlockKeyDB = std::make_shared<LevelDB>("microBlockKeys");
  m_dsCommitteeDB = std::make_shared<LevelDB>("dsCommittee");
  m_VCBlockDB = std::make_shared<LevelDB>("VCBlocks");
  m_blockLinkDB = std::make_shared<LevelDB>("blockLinks");
  m_shardStructureDB = std::make_shared<LevelDB>("shardStructure");
  m_stateDeltaDB = std::make_shared<LevelDB>("stateDelta");
  m_tempStateDB = std::make_shared<LevelDB>("tempState");
  m_processedTxnTmpDB = std::make_shared<LevelDB>("processedTxnTmp");
  m_diagnosticDBNodes =
      std::make_shared<LevelDB>("diagnosticNodes", path, diagnostic);
  m_diagnosticDBCoinbase =
      std::make_shared<LevelDB>("diagnosticCoinb", path, diagnostic);
  m_stateRootDB = std::make_shared<LevelDB>("stateRoot");

  if (LOOKUP_NODE_MODE) {
    m_txBodyDBs.emplace_back(std::make_shared<LevelDB>("txBodies"));
    m_txEpochDB = std::make_shared<LevelDB>("txEpochs");
    m_txTraceDB = std::make_shared<LevelDB>("txTraces");
    m_minerInfoDSCommDB = std::make_shared<LevelDB>("minerInfoDSComm");
    m_minerInfoShardsDB = std::make_shared<LevelDB>("minerInfoShards");
    m_extSeedPubKeysDB = std::make_shared<LevelDB>("extSeedPubKeys");
  }
  m_microBlockDBs.emplace_back(std::make_shared<LevelDB>("microBlocks"));
}

bool BlockStorage::PutBlock(const uint64_t& blockNum, const zbytes& body,
                            const BlockType& blockType) {
  int ret = -1;  // according to LevelDB::Insert return value
  if (blockType == BlockType::DS) {
    unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
    ret = m_dsBlockchainDB->Insert(blockNum, body);
    LOG_GENERAL(INFO, "Stored DSBlock num = " << blockNum);
  } else if (blockType == BlockType::Tx) {
    unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    ret = m_txBlockchainDB->Insert(blockNum, body);
    LOG_GENERAL(INFO, "Stored TxBlock num = " << blockNum);
  }
  return (ret == 0);
}

bool BlockStorage::PutDSBlock(const uint64_t& blockNum, const zbytes& body) {
  LOG_GENERAL(INFO, "Stored Block " << blockNum);
  return PutBlock(blockNum, body, BlockType::DS);
}

bool BlockStorage::PutVCBlock(const BlockHash& blockhash, const zbytes& body) {
  int ret = -1;
  unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
  ret = m_VCBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutBlockLink(const uint64_t& index, const zbytes& body) {
  int ret = -1;
  unique_lock<shared_timed_mutex> g(m_mutexBlockLink);
  ret = m_blockLinkDB->Insert(index, body);
  return (ret == 0);
}

bool BlockStorage::PutTxBlock(const TxBlockHeader& blockHeader,
                              const zbytes& body) {
  const auto status = PutBlock(blockHeader.GetBlockNum(), body, BlockType::Tx);
  if (status) {
    unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    m_txBlockHashToNumDB->Insert(blockHeader.GetMyHash(),
                                 std::to_string(blockHeader.GetBlockNum()));
    m_txBlockchainAuxDB->Insert(
        leveldb::Slice(MAX_TX_BLOCK_NUM_KEY),
        leveldb::Slice(std::to_string(blockHeader.GetBlockNum())));
  }
  return status;
}

bool BlockStorage::PutTxBody(const uint64_t& epochNum, const dev::h256& key,
                             const zbytes& body) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
    return false;
  }

  zbytes epoch;
  if (!Messenger::SetTxEpoch(epoch, 0, epochNum)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxEpoch failed.");
    return false;
  }

  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txEpochDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  // Store txn hash and epoch inside txEpochs DB
  if (m_txEpochDB->Insert(keyBytes, epoch) != 0) {
    LOG_GENERAL(WARNING, "TxBody epoch insertion failed. epoch="
                             << epochNum << " key=" << key);
    return false;
  }

  // Store txn hash and body inside txBodies DB
  if (GetTxBodyDB(epochNum)->Insert(keyBytes, body) != 0) {
    LOG_GENERAL(WARNING, "TxBody insertion failed. epoch=" << epochNum
                                                           << " key=" << key);
    m_txEpochDB->DeleteKey(key);
    return false;
  }

  return true;
}

bool BlockStorage::PutProcessedTxBodyTmp(const dev::h256& key,
                                         const zbytes& body) {
  int ret;
  {
    unique_lock<shared_timed_mutex> g(m_mutexProcessTx);
    ret = m_processedTxnTmpDB->Insert(key, body);
  }
  return (ret == 0);
}

bool BlockStorage::PutMicroBlock(const BlockHash& blockHash,
                                 const uint64_t& epochNum,
                                 const uint32_t& shardID, const zbytes& body) {
  zbytes key;
  if (!Messenger::SetMicroBlockKey(key, 0, epochNum, shardID)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlockKey failed.");
    return false;
  }

  lock_guard<mutex> g(m_mutexMicroBlock);

  // Store hash and key inside microBlockKeys DB
  if (m_microBlockKeyDB->Insert(blockHash, key) != 0) {
    LOG_GENERAL(WARNING, "Microblock key insertion failed. epoch="
                             << epochNum << " shard=" << shardID);
    return false;
  }

  // Store key and body inside microBlocks DB
  if (GetMicroBlockDB(epochNum)->Insert(key, body) != 0) {
    LOG_GENERAL(WARNING, "Microblock body insertion failed. epoch="
                             << epochNum << " shard=" << shardID);
    m_microBlockKeyDB->DeleteKey(blockHash);
    return false;
  }

  return true;
}

bool BlockStorage::GetMicroBlock(const BlockHash& blockHash,
                                 MicroBlockSharedPtr& microblock) {
  string blockString;

  {
    lock_guard<mutex> g(m_mutexMicroBlock);

    // Get key from microBlockKeys DB
    const string& keyString = m_microBlockKeyDB->Lookup(blockHash);
    if (keyString.empty()) {
      return false;
    }

    zbytes keyBytes(keyString.begin(), keyString.end());
    uint64_t epochNum = 0;
    uint32_t shardID = 0;
    if (!Messenger::GetMicroBlockKey(keyBytes, 0, epochNum, shardID)) {
      LOG_GENERAL(WARNING, "Messenger::GetMicroBlockKey failed.");
      return false;
    }

    // Get body from microBlock DB
    blockString = GetMicroBlockDB(epochNum)->Lookup(keyBytes);
  }

  if (blockString.empty()) {
    return false;
  }
  microblock = make_shared<MicroBlock>(
      zbytes(blockString.begin(), blockString.end()), 0);

  return true;
}

bool BlockStorage::GetMicroBlock(const uint64_t& epochNum,
                                 const uint32_t& shardID,
                                 MicroBlockSharedPtr& microblock) {
  zbytes key;
  if (!Messenger::SetMicroBlockKey(key, 0, epochNum, shardID)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlockKey failed.");
    return false;
  }

  string blockString;

  {
    lock_guard<mutex> g(m_mutexMicroBlock);
    blockString = GetMicroBlockDB(epochNum)->Lookup(key);
  }

  if (blockString.empty()) {
    return false;
  }
  microblock = make_shared<MicroBlock>(
      zbytes(blockString.begin(), blockString.end()), 0);

  return true;
}

bool BlockStorage::CheckMicroBlock(const BlockHash& blockHash) {
  lock_guard<mutex> g(m_mutexMicroBlock);
  // Get key from microBlockKeys DB
  string keyString = m_microBlockKeyDB->Lookup(blockHash);
  if (keyString.empty()) {
    return false;
  }
  zbytes keyBytes(keyString.begin(), keyString.end());
  uint64_t epochNum = 0;
  uint32_t shardID = 0;
  if (!Messenger::GetMicroBlockKey(keyBytes, 0, epochNum, shardID)) {
    LOG_GENERAL(WARNING, "Messenger::GetMicroBlockKey failed.");
    return false;
  }
  return GetMicroBlockDB(epochNum)->Exists(keyBytes);
}

bool BlockStorage::GetRangeMicroBlocks(const uint64_t lowEpochNum,
                                       const uint64_t hiEpochNum,
                                       const uint32_t loShardId,
                                       const uint32_t hiShardId,
                                       list<MicroBlockSharedPtr>& blocks) {
  LOG_MARKER();

  for (uint64_t epochNum = lowEpochNum; epochNum <= hiEpochNum; epochNum++) {
    for (uint32_t shardID = loShardId; shardID <= hiShardId; shardID++) {
      MicroBlockSharedPtr block;
      if (GetMicroBlock(epochNum, shardID, block)) {
        blocks.emplace_back(block);
        LOG_GENERAL(INFO, "Retrieved MicroBlock epoch=" << epochNum << " shard="
                                                        << shardID);
      }
    }
  }

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no MicroBlock matching the criteria");
    return false;
  }

  return true;
}

bool BlockStorage::PutTempState(const unordered_map<Address, Account>& states) {
  // LOG_MARKER();

  unordered_map<string, string> states_str;
  for (const auto& state : states) {
    zbytes rawBytes;
    if (!state.second.SerializeBase(rawBytes, 0)) {
      LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
      continue;
    }
    states_str.emplace(state.first.hex(),
                       DataConversion::CharArrayToString(rawBytes));
  }
  unique_lock<shared_timed_mutex> g(m_mutexTempState);
  return m_tempStateDB->BatchInsert(states_str);
}

bool BlockStorage::GetTempStateInBatch(leveldb::Iterator*& iter,
                                       vector<StateSharedPtr>& states) {
  // LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexTempState);

  if (iter == nullptr) {
    iter = m_tempStateDB->GetDB()->NewIterator(leveldb::ReadOptions());
    iter->SeekToFirst();
  }

  unsigned int counter = 0;

  for (; iter->Valid() && counter < ACCOUNT_IO_BATCH_SIZE;
       iter->Next(), counter++) {
    string addr_str = iter->key().ToString();
    string acct_string = iter->value().ToString();
    Address addr{addr_str};
    Account acct;
    if (!acct.DeserializeBase(zbytes(acct_string.begin(), acct_string.end()),
                              0)) {
      LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
      continue;
    }
    StateSharedPtr state =
        StateSharedPtr(new pair<Address, Account>(addr, acct));

    states.emplace_back(state);
  }

  return true;
}

bool BlockStorage::GetDSBlock(const uint64_t& blockNum,
                              DSBlockSharedPtr& block) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
    blockString = m_dsBlockchainDB->Lookup(blockNum);
  }

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  // LOG_GENERAL(INFO, blockString.length());
  block = DSBlockSharedPtr(
      new DSBlock(zbytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetVCBlock(const BlockHash& blockhash,
                              VCBlockSharedPtr& block) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexVCBlock);
    blockString = m_VCBlockDB->Lookup(blockhash);
  }

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  // LOG_GENERAL(INFO, blockString.length());
  block = VCBlockSharedPtr(
      new VCBlock(zbytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::ReleaseDB() {
  {
    lock_guard<mutex> g(m_mutexTxBody);
    for (auto& txBodyDB : m_txBodyDBs) {
      txBodyDB.reset();
    }
    m_txBodyDBs.clear();
    m_txEpochDB.reset();
  }
  {
    lock_guard<mutex> g(m_mutexMicroBlock);
    for (auto& microBlockDB : m_microBlockDBs) {
      microBlockDB.reset();
    }
    m_microBlockDBs.clear();
    m_microBlockKeyDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
    m_VCBlockDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    m_txBlockchainDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    m_txBlockchainAuxDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    m_txBlockHashToNumDB.reset();
  }

  {
    unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
    m_dsBlockchainDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexBlockLink);
    m_blockLinkDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
    m_minerInfoDSCommDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
    m_minerInfoShardsDB.reset();
  }
  return true;
}

bool BlockStorage::GetBlockLink(const uint64_t& index,
                                BlockLinkSharedPtr& block) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexBlockLink);
    blockString = m_blockLinkDB->Lookup(index);
  }

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  // LOG_GENERAL(INFO, blockString.length());
  BlockLink blnk;
  if (!Messenger::GetBlockLink(zbytes(blockString.begin(), blockString.end()),
                               0, blnk)) {
    LOG_GENERAL(WARNING, "Serialization of blockLink failed");
    return false;
  }

  if (get<BlockLinkIndex::VERSION>(blnk) != BLOCKLINK_VERSION) {
    LOG_CHECK_FAIL("BlockLink version", get<BlockLinkIndex::VERSION>(blnk),
                   BLOCKLINK_VERSION);
    return false;
  }

  block = make_shared<BlockLink>(blnk);
  return true;
}

bool BlockStorage::GetTxBlock(const uint64_t& blockNum,
                              TxBlockSharedPtr& block) const {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    blockString = m_txBlockchainDB->Lookup(blockNum);
  }
  if (blockString.empty()) {
    return false;
  }

  block = TxBlockSharedPtr(
      new TxBlock(zbytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetTxBlock(const BlockHash& blockhash,
                              TxBlockSharedPtr& block) const {
  const zbytes& keyBytes = blockhash.asBytes();
  std::string blockNumStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    blockNumStr = m_txBlockHashToNumDB->Lookup(keyBytes);
  }

  if (blockNumStr.empty()) {
    return false;
  }

  return GetTxBlock(std::stoull(blockNumStr), block);
}

bool BlockStorage::GetLatestTxBlock(TxBlockSharedPtr& block) {
  uint64_t latestTxBlockNum = 0;

  LOG_GENERAL(INFO, "Retrieving latest Tx block...");

  {
    shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    leveldb::Iterator* it =
        m_txBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      uint64_t blockNum = boost::lexical_cast<uint64_t>(it->key().ToString());
      if (blockNum > latestTxBlockNum) {
        latestTxBlockNum = blockNum;
      }
    }
    delete it;
  }

  LOG_GENERAL(INFO, "Latest Tx block = " << latestTxBlockNum);
  return GetTxBlock(latestTxBlockNum, block);
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body) {
  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txEpochDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string epochString = m_txEpochDB->Lookup(keyBytes);
  if (epochString.empty()) {
    return false;
  }

  zbytes epochBytes(epochString.begin(), epochString.end());
  uint64_t epochNum = 0;
  if (!Messenger::GetTxEpoch(epochBytes, 0, epochNum)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxEpoch failed.");
    return false;
  }

  string bodyString = GetTxBodyDB(epochNum)->Lookup(keyBytes);

  if (bodyString.empty()) {
    return false;
  }
  body = TxBodySharedPtr(new TransactionWithReceipt(
      zbytes(bodyString.begin(), bodyString.end()), 0));

  return true;
}

bool BlockStorage::CheckTxBody(const dev::h256& key) {
  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txEpochDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string epochString = m_txEpochDB->Lookup(keyBytes);
  if (epochString.empty()) {
    return false;
  }

  zbytes epochBytes(epochString.begin(), epochString.end());
  uint64_t epochNum = 0;
  if (!Messenger::GetTxEpoch(epochBytes, 0, epochNum)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxEpoch failed.");
    return false;
  }

  return GetTxBodyDB(epochNum)->Exists(keyBytes);
}

bool BlockStorage::GetTxTrace(const dev::h256& key, std::string& trace) {
  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txTraceDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  trace = m_txTraceDB->Lookup(keyBytes);

  if (trace.empty()) {
    return false;
  }
  return true;
}

bool BlockStorage::PutTxTrace(const dev::h256& key, const std::string& trace) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
    return false;
  }

  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txTraceDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  // Store txn hash and epoch inside txEpochs DB
  if (m_txTraceDB->Insert(key, trace) != 0) {
    LOG_GENERAL(WARNING, "Tx trace insertion failed. "
                             << " key=" << key);
    return false;
  }

  return true;
}

bool BlockStorage::DeleteDSBlock(const uint64_t& blocknum) {
  LOG_GENERAL(INFO, "Delete DSBlock Num: " << blocknum);
  unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
  int ret = m_dsBlockchainDB->DeleteKey(blocknum);
  return (ret == 0);
}

bool BlockStorage::DeleteVCBlock(const BlockHash& blockhash) {
  unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
  int ret = m_VCBlockDB->DeleteKey(blockhash);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBlock(const uint64_t& blocknum) {
  LOG_GENERAL(INFO, "Delete TxBlock Num: " << blocknum);
  unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
  int ret = m_txBlockchainDB->DeleteKey(blocknum);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBody(const dev::h256& key) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this");
    return false;
  }

  const zbytes& keyBytes = key.asBytes();

  lock_guard<mutex> g(m_mutexTxBody);

  if (!m_txEpochDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string epochString = m_txEpochDB->Lookup(keyBytes);
  if (epochString.empty()) {
    return false;
  }

  zbytes epochBytes(epochString.begin(), epochString.end());
  uint64_t epochNum = 0;
  if (!Messenger::GetTxEpoch(epochBytes, 0, epochNum)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxEpoch failed.");
    return false;
  }

  return (m_txEpochDB->DeleteKey(keyBytes) == 0) &&
         (GetTxBodyDB(epochNum)->DeleteKey(keyBytes) == 0);
}

bool BlockStorage::DeleteMicroBlock(const BlockHash& blockHash) {
  lock_guard<mutex> g(m_mutexMicroBlock);

  // Get key from microBlockKeys DB
  string keyString = m_microBlockKeyDB->Lookup(blockHash);
  if (keyString.empty()) {
    return false;
  }

  // Delete key
  int ret = m_microBlockKeyDB->DeleteKey(blockHash);

  // Delete body
  if (ret == 0) {
    zbytes keyBytes(keyString.begin(), keyString.end());
    uint64_t epochNum = 0;
    uint32_t shardID = 0;
    if (!Messenger::GetMicroBlockKey(keyBytes, 0, epochNum, shardID)) {
      LOG_GENERAL(WARNING, "Messenger::GetMicroBlockKey failed.");
      return false;
    }
    ret = GetMicroBlockDB(epochNum)->DeleteKey(keyString);
  }

  return (ret == 0);
}

bool BlockStorage::DeleteStateDelta(const uint64_t& finalBlockNum) {
  unique_lock<shared_timed_mutex> g(m_mutexStateDelta);

  int ret = m_stateDeltaDB->DeleteKey(finalBlockNum);

  return (ret == 0);
}

bool BlockStorage::GetAllDSBlocks(std::list<DSBlockSharedPtr>& blocks) {
  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexDsBlockchain);

  leveldb::Iterator* it =
      m_dsBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one block in the chain");
      delete it;
      return false;
    }

    DSBlockSharedPtr block = DSBlockSharedPtr(
        new DSBlock(zbytes(blockString.begin(), blockString.end()), 0));
    blocks.emplace_back(block);
    LOG_GENERAL(INFO, "Retrievd DsBlock Num:" << bns);
  }

  delete it;

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no DSBlock");
    return false;
  }

  return true;
}

bool BlockStorage::PutExtSeedPubKey(const PubKey& pubK) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);

  if (!m_extSeedPubKeysDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string keyStr = "0000000001";
  uint32_t key;
  leveldb::Iterator* it =
      m_extSeedPubKeysDB->GetDB()->NewIterator(leveldb::ReadOptions());
  it->SeekToLast();
  if (it->Valid()) {
    keyStr = it->key().ToString();
    try {
      key = stoull(keyStr);
      std::stringstream ss;
      ss << std::setw(10) << std::setfill('0') << ++key;
      keyStr = ss.str();
    } catch (...) {
      delete it;
      LOG_GENERAL(WARNING, "key is not numeric");
      return false;
    }
  }
  delete it;

  zbytes data;
  pubK.Serialize(data, 0);
  LOG_GENERAL(INFO, "Inserting with key:" << keyStr << ", Pubkey:" << pubK);
  int ret = m_extSeedPubKeysDB->Insert(keyStr, data);
  return (ret == 0);
}

bool BlockStorage::DeleteExtSeedPubKey(const PubKey& pubK) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);

  if (!m_extSeedPubKeysDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  leveldb::Iterator* it =
      m_extSeedPubKeysDB->GetDB()->NewIterator(leveldb::ReadOptions());
  zbytes data;
  pubK.Serialize(data, 0);
  string pubKStrI = DataConversion::CharArrayToString(data);
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string pns = it->key().ToString();
    string pubkString = it->value().ToString();
    if (pubkString == pubKStrI) {
      if (0 == m_extSeedPubKeysDB->DeleteKey(pns)) {
        LOG_GENERAL(
            INFO, "Deleted extseed pubkey " << pubK << " from DB successfully");
        delete it;
        return true;
      }
    }
  }
  delete it;
  return false;
}

bool BlockStorage::GetAllExtSeedPubKeys(unordered_set<PubKey>& pubKeys) {
  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);

  if (!m_extSeedPubKeysDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  leveldb::Iterator* it =
      m_extSeedPubKeysDB->GetDB()->NewIterator(leveldb::ReadOptions());
  uint64_t count = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string pubkString = it->value().ToString();
    if (pubkString.empty()) {
      LOG_GENERAL(WARNING, "Lost one extseed public key in the DB");
      delete it;
      return false;
    }
    PubKey pubK(zbytes(pubkString.begin(), pubkString.end()), 0);
    pubKeys.emplace(pubK);
    count++;
  }
  LOG_GENERAL(INFO, "Retrieved " << count << " PubKeys");

  delete it;

  if (pubKeys.empty()) {
    LOG_GENERAL(INFO, "Disk has no extseed PubKeys");
    return false;
  }

  return true;
}

bool BlockStorage::GetAllTxBlocks(std::deque<TxBlockSharedPtr>& blocks) {
  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);

  leveldb::Iterator* it =
      m_txBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
  uint64_t count = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one block in the chain");
      delete it;
      return false;
    }
    TxBlockSharedPtr block = TxBlockSharedPtr(
        new TxBlock(zbytes(blockString.begin(), blockString.end()), 0));
    blocks.emplace_back(block);
    count++;
  }
  LOG_GENERAL(INFO, "Retrievd " << count << " TxBlocks");

  delete it;

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no TxBlock");
    return false;
  }

  return true;
}

bool BlockStorage::GetAllVCBlocks(std::list<VCBlockSharedPtr>& blocks) {
  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexVCBlock);

  leveldb::Iterator* it =
      m_VCBlockDB->GetDB()->NewIterator(leveldb::ReadOptions());
  uint64_t count = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one block in the chain");
      delete it;
      return false;
    }
    VCBlockSharedPtr block = VCBlockSharedPtr(
        new VCBlock(zbytes(blockString.begin(), blockString.end()), 0));
    blocks.emplace_back(block);
    count++;
  }
  LOG_GENERAL(INFO, "Retrievd " << count << " VCBlocks");

  delete it;

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no VCBlock");
  }

  return true;
}

bool BlockStorage::GetAllBlockLink(std::list<BlockLink>& blocklinks) {
  LOG_MARKER();

  LOG_GENERAL(INFO, "Retrieving blocklinks...");

  shared_lock<shared_timed_mutex> g(m_mutexBlockLink);

  leveldb::Iterator* it =
      m_blockLinkDB->GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one blocklink in the chain");
      delete it;
      return false;
    }
    BlockLink blcklink;
    if (!Messenger::GetBlockLink(zbytes(blockString.begin(), blockString.end()),
                                 0, blcklink)) {
      LOG_GENERAL(WARNING, "Deserialization of blockLink failed " << bns);
      delete it;
      return false;
    }
    if (get<BlockLinkIndex::VERSION>(blcklink) != BLOCKLINK_VERSION) {
      LOG_CHECK_FAIL("BlockLink version",
                     get<BlockLinkIndex::VERSION>(blcklink), BLOCKLINK_VERSION);
      delete it;
      return false;
    }
    blocklinks.emplace_back(blcklink);
  }
  delete it;
  if (blocklinks.empty()) {
    LOG_GENERAL(INFO, "Disk has no blocklink");
    return false;
  }
  LOG_GENERAL(INFO, "Retrieving blocklinks done");
  return true;
}

bool BlockStorage::PutMetadata(MetaType type, const zbytes& data) {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_mutexMetadata);
  int ret = m_metadataDB->Insert(std::to_string((int)type), data);
  return (ret == 0);
}

bool BlockStorage::PutStateRoot(const zbytes& data) {
  unique_lock<shared_timed_mutex> g(m_mutexStateRoot);
  int ret = m_stateRootDB->Insert(std::to_string((int)STATEROOT), data);
  return (ret == 0);
}

bool BlockStorage::PutLatestEpochStatesUpdated(const uint64_t& epochNum) {
  unique_lock<shared_timed_mutex> g(m_mutexStateRoot);
  int ret =
      m_stateRootDB->Insert(LATEST_EPOCH_STATES_UPDATED, to_string(epochNum));
  return (ret == 0);
}

bool BlockStorage::PutEpochFin(const uint64_t& epochNum) {
  LOG_MARKER();
  return BlockStorage::GetBlockStorage().PutMetadata(
      MetaType::EPOCHFIN,
      DataConversion::StringToCharArray(to_string(epochNum)));
}

bool BlockStorage::GetMetadata(MetaType type, zbytes& data, bool muteLog) {
  if (!muteLog) {
    LOG_MARKER();
  }

  string metaString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMetadata);
    metaString = m_metadataDB->Lookup(std::to_string((int)type));
  }

  if (metaString.empty()) {
    LOG_GENERAL(INFO, "No metadata get")
    return false;
  }

  data = zbytes(metaString.begin(), metaString.end());

  return true;
}

bool BlockStorage::GetStateRoot(zbytes& data) {
  LOG_MARKER();

  string stateRoot;
  {
    shared_lock<shared_timed_mutex> g(m_mutexStateRoot);
    stateRoot = m_stateRootDB->Lookup(std::to_string((int)STATEROOT));
  }

  if (stateRoot.empty()) {
    LOG_GENERAL(INFO, "No state root found")
    return false;
  }

  data = zbytes(stateRoot.begin(), stateRoot.end());

  return true;
}

bool BlockStorage::GetLatestEpochStatesUpdated(uint64_t& epochNum) {
  LOG_MARKER();

  string epochNumStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexStateRoot);
    epochNumStr = m_stateRootDB->Lookup(LATEST_EPOCH_STATES_UPDATED);
  }

  if (epochNumStr.empty()) {
    LOG_GENERAL(INFO, "No Latest Epoch State Updated get");
    return false;
  }

  try {
    epochNum = stoull(epochNumStr);
  } catch (...) {
    LOG_GENERAL(WARNING, "epochNumStr is not numeric");
    return false;
  }
  return true;
}

bool BlockStorage::GetEpochFin(uint64_t& epochNum) {
  zbytes epochFinBytes;
  if (BlockStorage::GetBlockStorage().GetMetadata(MetaType::EPOCHFIN,
                                                  epochFinBytes, true)) {
    try {
      epochNum = std::stoull(DataConversion::CharArrayToString(epochFinBytes));
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "EPOCHFIN cannot be parsed as uint64_t "
                      << DataConversion::CharArrayToString(epochFinBytes));
      return false;
    }
  } else {
    LOG_GENERAL(WARNING, "Cannot get EPOCHFIN from DB");
    return false;
  }

  return true;
}

bool BlockStorage::PutDSCommittee(const shared_ptr<DequeOfNode>& dsCommittee,
                                  const uint16_t& consensusLeaderID) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexDsCommittee);
  m_dsCommitteeDB->ResetDB();
  unsigned int index = 0;
  string leaderId = to_string(consensusLeaderID);

  if (0 != m_dsCommitteeDB->Insert(index++,
                                   zbytes(leaderId.begin(), leaderId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store DS leader ID:" << consensusLeaderID);
    return false;
  }

  LOG_GENERAL(INFO, "DS leader: " << consensusLeaderID);

  zbytes data;

  unsigned int ds_index = 0;
  for (const auto& ds : *dsCommittee) {
    ds.first.Serialize(data, 0);
    ds.second.Serialize(data, data.size());

    /// Store index as key, to guarantee the sequence of DS committee after
    /// retrieval Because first DS committee is DS leader
    if (0 != m_dsCommitteeDB->Insert(index++, data)) {
      LOG_GENERAL(WARNING, "Failed to store DS committee:" << ds.first << ", "
                                                           << ds.second);
      return false;
    }

    LOG_GENERAL(INFO, "[" << PAD(ds_index++, 3, ' ') << "] " << ds.first << " "
                          << ds.second);

    data.clear();
  }

  return true;
}

bool BlockStorage::GetDSCommittee(shared_ptr<DequeOfNode>& dsCommittee,
                                  uint16_t& consensusLeaderID) {
  LOG_MARKER();

  unsigned int index = 0;
  shared_lock<shared_timed_mutex> g(m_mutexDsCommittee);
  string strConsensusLeaderID = m_dsCommitteeDB->Lookup(index++);

  if (strConsensusLeaderID.empty()) {
    LOG_GENERAL(WARNING, "Cannot retrieve DS committee!");
    return false;
  }

  try {
    consensusLeaderID = stoul(strConsensusLeaderID);
  } catch (...) {
    LOG_GENERAL(WARNING, "strConsensusID is not numeric");
    return false;
  }
  LOG_GENERAL(INFO, "Retrieved DS leader ID: " << consensusLeaderID);
  string dataStr;

  while (true) {
    dataStr = m_dsCommitteeDB->Lookup(index++);

    if (dataStr.empty()) {
      break;
    }

    dsCommittee->emplace_back(
        PubKey(zbytes(dataStr.begin(), dataStr.begin() + PUB_KEY_SIZE), 0),
        Peer(zbytes(dataStr.begin() + PUB_KEY_SIZE, dataStr.end()), 0));
    LOG_GENERAL(INFO, "Retrieved DS committee: " << dsCommittee->back().first
                                                 << ", "
                                                 << dsCommittee->back().second);
  }

  return true;
}

bool BlockStorage::PutShardStructure(const DequeOfShard& shards,
                                     const uint32_t myshardId) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexShardStructure);
  m_shardStructureDB->ResetDB();
  unsigned int index = 0;
  string shardId = to_string(myshardId);

  if (0 != m_shardStructureDB->Insert(index++,
                                      zbytes(shardId.begin(), shardId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store shard ID:" << myshardId);
    return false;
  }

  LOG_GENERAL(INFO, "Stored shard ID:" << myshardId);

  zbytes shardStructure;

  if (!Messenger::ShardStructureToArray(shardStructure, 0,
                                        SHARDINGSTRUCTURE_VERSION, shards)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure");
    return false;
  }

  if (0 != m_shardStructureDB->Insert(index++, shardStructure)) {
    LOG_GENERAL(WARNING, "Failed to store sharding structure");
    return false;
  }

  LOG_GENERAL(INFO, "Stored sharding structure");
  return true;
}

bool BlockStorage::GetShardStructure(DequeOfShard& shards) {
  LOG_MARKER();

  unsigned int index = 1;
  string dataStr;

  {
    shared_lock<shared_timed_mutex> g(m_mutexShardStructure);
    dataStr = m_shardStructureDB->Lookup(index++);
  }

  uint32_t version = 0;
  Messenger::ArrayToShardStructure(zbytes(dataStr.begin(), dataStr.end()), 0,
                                   version, shards);

  if (version != SHARDINGSTRUCTURE_VERSION) {
    LOG_CHECK_FAIL("Sharding structure version", version,
                   SHARDINGSTRUCTURE_VERSION);
    return false;
  }

  LOG_GENERAL(INFO, "Retrieved sharding structure");
  return true;
}

bool BlockStorage::PutStateDelta(const uint64_t& finalBlockNum,
                                 const zbytes& stateDelta) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexStateDelta);

  if (0 != m_stateDeltaDB->Insert(finalBlockNum, stateDelta)) {
    LOG_PAYLOAD(WARNING,
                "Failed to store state delta of final block " << finalBlockNum,
                stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
    return false;
  }

  LOG_PAYLOAD(INFO, "FinalBlock " << finalBlockNum << " state delta",
              stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
  return true;
}

bool BlockStorage::GetStateDelta(const uint64_t& finalBlockNum,
                                 zbytes& stateDelta) {
  LOG_MARKER();
  bool found = false;

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexStateDelta);
    dataStr = m_stateDeltaDB->Lookup(finalBlockNum, found);
  }
  if (found) {
    stateDelta = zbytes(dataStr.begin(), dataStr.end());
    LOG_PAYLOAD(INFO, "Retrieved state delta of final block " << finalBlockNum,
                stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
  } else {
    LOG_GENERAL(INFO,
                "Didn't find state delta of final block " << finalBlockNum);
  }

  return found;
}

bool BlockStorage::PutDiagnosticDataNodes(const uint64_t& dsBlockNum,
                                          const DequeOfShard& shards,
                                          const DequeOfNode& dsCommittee) {
  LOG_MARKER();

  zbytes data;

  if (!Messenger::SetDiagnosticDataNodes(data, 0, SHARDINGSTRUCTURE_VERSION,
                                         shards, DSCOMMITTEE_VERSION,
                                         dsCommittee)) {
    LOG_GENERAL(WARNING, "Messenger::SetDiagnosticDataNodes failed");
    return false;
  }

  lock_guard<mutex> g(m_mutexDiagnostic);

  if (0 != m_diagnosticDBNodes->Insert(dsBlockNum, data)) {
    LOG_GENERAL(WARNING, "Failed to store diagnostic data");
    return false;
  }

  m_diagnosticDBNodesCounter++;

  return true;
}

bool BlockStorage::PutDiagnosticDataCoinbase(
    const uint64_t& dsBlockNum, const DiagnosticDataCoinbase& entry) {
  LOG_MARKER();

  zbytes data;

  if (!Messenger::SetDiagnosticDataCoinbase(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::SetDiagnosticDataCoinbase failed");
    return false;
  }

  lock_guard<mutex> g(m_mutexDiagnostic);

  if (0 != m_diagnosticDBCoinbase->Insert(dsBlockNum, data)) {
    LOG_GENERAL(WARNING, "Failed to store diagnostic data");
    return false;
  }

  m_diagnosticDBCoinbaseCounter++;

  return true;
}

bool BlockStorage::GetDiagnosticDataNodes(const uint64_t& dsBlockNum,
                                          DequeOfShard& shards,
                                          DequeOfNode& dsCommittee) {
  LOG_MARKER();

  string dataStr;

  {
    lock_guard<mutex> g(m_mutexDiagnostic);
    dataStr = m_diagnosticDBNodes->Lookup(dsBlockNum);
  }

  if (dataStr.empty()) {
    LOG_GENERAL(WARNING,
                "Failed to retrieve diagnostic data for DS block number "
                    << dsBlockNum);
    return false;
  }

  zbytes data(dataStr.begin(), dataStr.end());

  uint32_t shardingStructureVersion = 0;
  uint32_t dsCommitteeVersion = 0;
  if (!Messenger::GetDiagnosticDataNodes(data, 0, shardingStructureVersion,
                                         shards, dsCommitteeVersion,
                                         dsCommittee)) {
    LOG_GENERAL(WARNING, "Messenger::GetDiagnosticDataNodes failed");
    return false;
  }

  if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
    LOG_CHECK_FAIL("Sharding structure version", shardingStructureVersion,
                   SHARDINGSTRUCTURE_VERSION);
    return false;
  }

  if (dsCommitteeVersion != DSCOMMITTEE_VERSION) {
    LOG_CHECK_FAIL("DS committee version", dsCommitteeVersion,
                   DSCOMMITTEE_VERSION);
    return false;
  }

  return true;
}

bool BlockStorage::GetDiagnosticDataCoinbase(const uint64_t& dsBlockNum,
                                             DiagnosticDataCoinbase& entry) {
  LOG_MARKER();

  string dataStr;

  {
    lock_guard<mutex> g(m_mutexDiagnostic);
    dataStr = m_diagnosticDBCoinbase->Lookup(dsBlockNum);
  }

  if (dataStr.empty()) {
    LOG_GENERAL(WARNING,
                "Failed to retrieve diagnostic data for DS block number "
                    << dsBlockNum);
    return false;
  }

  zbytes data(dataStr.begin(), dataStr.end());

  if (!Messenger::GetDiagnosticDataCoinbase(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::GetDiagnosticDataCoinbase failed");
    return false;
  }

  return true;
}

void BlockStorage::GetDiagnosticDataNodes(
    map<uint64_t, DiagnosticDataNodes>& diagnosticDataMap) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexDiagnostic);

  leveldb::Iterator* it =
      m_diagnosticDBNodes->GetDB()->NewIterator(leveldb::ReadOptions());

  unsigned int index = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string dsBlockNumStr = it->key().ToString();
    string dataStr = it->value().ToString();

    if (dsBlockNumStr.empty() || dataStr.empty()) {
      LOG_GENERAL(WARNING,
                  "Failed to retrieve diagnostic data at index " << index);
      continue;
    }

    uint64_t dsBlockNum = 0;
    try {
      dsBlockNum = stoull(dsBlockNumStr);
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "Non-numeric key " << dsBlockNumStr << " at index " << index);
      continue;
    }

    zbytes data(dataStr.begin(), dataStr.end());

    DiagnosticDataNodes entry;
    uint32_t shardingStructureVersion = 0;
    uint32_t dsCommitteeVersion = 0;

    if (!Messenger::GetDiagnosticDataNodes(data, 0, shardingStructureVersion,
                                           entry.shards, dsCommitteeVersion,
                                           entry.dsCommittee)) {
      LOG_GENERAL(
          WARNING,
          "Messenger::GetDiagnosticDataNodes failed for DS block number "
              << dsBlockNumStr << " at index " << index);
      continue;
    }

    if (shardingStructureVersion != SHARDINGSTRUCTURE_VERSION) {
      LOG_CHECK_FAIL("Sharding structure version", shardingStructureVersion,
                     SHARDINGSTRUCTURE_VERSION)
      continue;
    }

    if (dsCommitteeVersion != DSCOMMITTEE_VERSION) {
      LOG_CHECK_FAIL("DS committee version", dsCommitteeVersion,
                     DSCOMMITTEE_VERSION);
      continue;
    }

    diagnosticDataMap.emplace(make_pair(dsBlockNum, entry));

    index++;
  }
  delete it;
}

void BlockStorage::GetDiagnosticDataCoinbase(
    map<uint64_t, DiagnosticDataCoinbase>& diagnosticDataMap) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexDiagnostic);

  leveldb::Iterator* it =
      m_diagnosticDBCoinbase->GetDB()->NewIterator(leveldb::ReadOptions());

  unsigned int index = 0;
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string dsBlockNumStr = it->key().ToString();
    string dataStr = it->value().ToString();

    if (dsBlockNumStr.empty() || dataStr.empty()) {
      LOG_GENERAL(WARNING,
                  "Failed to retrieve diagnostic data at index " << index);
      continue;
    }

    uint64_t dsBlockNum = 0;
    try {
      dsBlockNum = stoull(dsBlockNumStr);
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "Non-numeric key " << dsBlockNumStr << " at index " << index);
      continue;
    }

    zbytes data(dataStr.begin(), dataStr.end());

    DiagnosticDataCoinbase entry;

    if (!Messenger::GetDiagnosticDataCoinbase(data, 0, entry)) {
      LOG_GENERAL(
          WARNING,
          "Messenger::GetDiagnosticDataCoinbase failed for DS block number "
              << dsBlockNumStr << " at index " << index);
      continue;
    }

    diagnosticDataMap.emplace(make_pair(dsBlockNum, entry));

    index++;
  }
  delete it;
}

unsigned int BlockStorage::GetDiagnosticDataNodesCount() {
  lock_guard<mutex> g(m_mutexDiagnostic);
  return m_diagnosticDBNodesCounter;
}

unsigned int BlockStorage::GetDiagnosticDataCoinbaseCount() {
  lock_guard<mutex> g(m_mutexDiagnostic);
  return m_diagnosticDBCoinbaseCounter;
}

bool BlockStorage::DeleteDiagnosticDataNodes(const uint64_t& dsBlockNum) {
  lock_guard<mutex> g(m_mutexDiagnostic);
  bool result = (0 == m_diagnosticDBNodes->DeleteKey(dsBlockNum));
  if (result) {
    m_diagnosticDBNodesCounter--;
  }
  return result;
}

bool BlockStorage::DeleteDiagnosticDataCoinbase(const uint64_t& dsBlockNum) {
  lock_guard<mutex> g(m_mutexDiagnostic);
  bool result = (0 == m_diagnosticDBCoinbase->DeleteKey(dsBlockNum));
  if (result) {
    m_diagnosticDBCoinbaseCounter--;
  }
  return result;
}

bool BlockStorage::PutMinerInfoDSComm(const uint64_t& dsBlockNum,
                                      const MinerInfoDSComm& entry) {
  LOG_MARKER();

  zbytes data;

  if (!Messenger::SetMinerInfoDSComm(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::SetMinerInfoDSComm failed");
    return false;
  }

  unique_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);

  if (!m_minerInfoDSCommDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  if (0 != m_minerInfoDSCommDB->Insert(dsBlockNum, data)) {
    LOG_GENERAL(WARNING, "Failed to store miner info");
    return false;
  }

  return true;
}

bool BlockStorage::GetMinerInfoDSComm(const uint64_t& dsBlockNum,
                                      MinerInfoDSComm& entry) {
  LOG_MARKER();
  bool found = false;

  if (!m_minerInfoDSCommDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
    dataStr = m_minerInfoDSCommDB->Lookup(dsBlockNum, found);
  }
  if (found) {
    if (!Messenger::GetMinerInfoDSComm(zbytes(dataStr.begin(), dataStr.end()),
                                       0, entry)) {
      LOG_GENERAL(WARNING, "Messenger::GetMinerInfoDSComm failed");
      found = false;
    }
  }

  return found;
}

bool BlockStorage::PutMinerInfoShards(const uint64_t& dsBlockNum,
                                      const MinerInfoShards& entry) {
  LOG_MARKER();

  zbytes data;

  if (!Messenger::SetMinerInfoShards(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::SetMinerInfoShards failed");
    return false;
  }

  unique_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);

  if (!m_minerInfoShardsDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  if (0 != m_minerInfoShardsDB->Insert(dsBlockNum, data)) {
    LOG_GENERAL(WARNING, "Failed to store miner info");
    return false;
  }

  return true;
}

bool BlockStorage::GetMinerInfoShards(const uint64_t& dsBlockNum,
                                      MinerInfoShards& entry) {
  LOG_MARKER();
  bool found = false;

  if (!m_minerInfoShardsDB) {
    LOG_GENERAL(
        WARNING,
        "Attempt to access non initialized DB! Are you in lookup mode? ");
    return false;
  }

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
    dataStr = m_minerInfoShardsDB->Lookup(dsBlockNum, found);
  }
  if (found) {
    if (!Messenger::GetMinerInfoShards(zbytes(dataStr.begin(), dataStr.end()),
                                       0, entry)) {
      LOG_GENERAL(WARNING, "Messenger::GetMinerInfoShards failed");
      found = false;
    }
  }

  return found;
}

bool BlockStorage::ResetDB(DBTYPE type) {
  LOG_MARKER();
  bool ret = false;
  switch (type) {
    case META: {
      unique_lock<shared_timed_mutex> g(m_mutexMetadata);
      ret = m_metadataDB->ResetDB();
      break;
    }
    case DS_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
      ret = m_dsBlockchainDB->ResetDB();
      break;
    }
    case TX_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockchainDB->ResetDB();
      break;
    }
    case TX_BLOCK_AUX: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockchainAuxDB->ResetDB();
      break;
    }
    case TX_BLOCK_HASH_TO_NUM: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockHashToNumDB->ResetDB();
      break;
    }

    case TX_BODY: {
      lock_guard<mutex> g(m_mutexTxBody);
      ret = m_txEpochDB->ResetDB();
      for (auto& txBodyDB : m_txBodyDBs) {
        ret &= txBodyDB->ResetDB();
      }
      break;
    }
    case MICROBLOCK: {
      lock_guard<mutex> g(m_mutexMicroBlock);
      ret = m_microBlockKeyDB->ResetDB();
      for (auto& microBlockDB : m_microBlockDBs) {
        ret &= microBlockDB->ResetDB();
      }
      break;
    }
    case DS_COMMITTEE: {
      unique_lock<shared_timed_mutex> g(m_mutexDsCommittee);
      ret = m_dsCommitteeDB->ResetDB();
      break;
    }
    case VC_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
      ret = m_VCBlockDB->ResetDB();
      break;
    }
    case BLOCKLINK: {
      unique_lock<shared_timed_mutex> g(m_mutexBlockLink);
      ret = m_blockLinkDB->ResetDB();
      break;
    }
    case SHARD_STRUCTURE: {
      unique_lock<shared_timed_mutex> g(m_mutexShardStructure);
      ret = m_shardStructureDB->ResetDB();
      break;
    }
    case STATE_DELTA: {
      unique_lock<shared_timed_mutex> g(m_mutexStateDelta);
      ret = m_stateDeltaDB->ResetDB();
      break;
    }
    case TEMP_STATE: {
      unique_lock<shared_timed_mutex> g(m_mutexTempState);
      ret = m_tempStateDB->ResetDB();
      break;
    }
    case DIAGNOSTIC_NODES: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret = m_diagnosticDBNodes->ResetDB();
      if (ret) {
        m_diagnosticDBNodesCounter = 0;
      }
      break;
    }
    case DIAGNOSTIC_COINBASE: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret = m_diagnosticDBCoinbase->ResetDB();
      if (ret) {
        m_diagnosticDBCoinbaseCounter = 0;
      }
      break;
    }
    case STATE_ROOT: {
      unique_lock<shared_timed_mutex> g(m_mutexStateRoot);
      ret = m_stateRootDB->ResetDB();
      break;
    }
    case PROCESSED_TEMP: {
      unique_lock<shared_timed_mutex> g(m_mutexProcessTx);
      ret = m_processedTxnTmpDB->ResetDB();
      break;
    }
    case MINER_INFO_DSCOMM: {
      unique_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
      ret = m_minerInfoDSCommDB->ResetDB();
      break;
    }
    case MINER_INFO_SHARDS: {
      unique_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
      ret = m_minerInfoShardsDB->ResetDB();
      break;
    }
    case EXTSEED_PUBKEYS: {
      unique_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);
      ret = m_extSeedPubKeysDB->ResetDB();
      break;
    }
  }
  if (!ret) {
    LOG_GENERAL(INFO, "FAIL: Reset DB " << type << " failed");
  }
  return ret;
}

bool BlockStorage::RefreshDB(DBTYPE type) {
  bool ret = false;
  switch (type) {
    case META: {
      unique_lock<shared_timed_mutex> g(m_mutexMetadata);
      ret = m_metadataDB->RefreshDB();
      break;
    }
    case DS_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
      ret = m_dsBlockchainDB->RefreshDB();
      break;
    }
    case TX_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockchainDB->RefreshDB();
      break;
    }
    case TX_BLOCK_AUX: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockchainAuxDB->RefreshDB();
      break;
    }
    case TX_BLOCK_HASH_TO_NUM: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockHashToNumDB->RefreshDB();
      break;
    }
    case TX_BODY: {
      lock_guard<mutex> g(m_mutexTxBody);
      ret = m_txEpochDB->RefreshDB();
      for (auto& txBodyDB : m_txBodyDBs) {
        ret &= txBodyDB->RefreshDB();
      }
      break;
    }
    case MICROBLOCK: {
      lock_guard<mutex> g(m_mutexMicroBlock);
      ret = m_microBlockKeyDB->RefreshDB();
      for (auto& microBlockDB : m_microBlockDBs) {
        ret &= microBlockDB->RefreshDB();
      }
      break;
    }
    case DS_COMMITTEE: {
      unique_lock<shared_timed_mutex> g(m_mutexDsCommittee);
      ret = m_dsCommitteeDB->RefreshDB();
      break;
    }
    case VC_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
      ret = m_VCBlockDB->RefreshDB();
      break;
    }
    case BLOCKLINK: {
      unique_lock<shared_timed_mutex> g(m_mutexBlockLink);
      ret = m_blockLinkDB->RefreshDB();
      break;
    }
    case SHARD_STRUCTURE: {
      unique_lock<shared_timed_mutex> g(m_mutexShardStructure);
      ret = m_shardStructureDB->RefreshDB();
      break;
    }
    case STATE_DELTA: {
      unique_lock<shared_timed_mutex> g(m_mutexStateDelta);
      ret = m_stateDeltaDB->RefreshDB();
      break;
    }
    case DIAGNOSTIC_NODES: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret = m_diagnosticDBNodes->RefreshDB();
      if (ret) {
        m_diagnosticDBNodesCounter = 0;
      }
      break;
    }
    case DIAGNOSTIC_COINBASE: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret = m_diagnosticDBCoinbase->RefreshDB();
      if (ret) {
        m_diagnosticDBCoinbaseCounter = 0;
      }
      break;
    }
    case STATE_ROOT: {
      unique_lock<shared_timed_mutex> g(m_mutexStateRoot);
      ret = m_stateRootDB->RefreshDB();
      break;
    }
    case TEMP_STATE: {
      unique_lock<shared_timed_mutex> g(m_mutexTempState);
      ret = m_tempStateDB->RefreshDB();
      break;
    }
    case PROCESSED_TEMP: {
      unique_lock<shared_timed_mutex> g(m_mutexProcessTx);
      ret = m_processedTxnTmpDB->RefreshDB();
      break;
    }
    case MINER_INFO_DSCOMM: {
      unique_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
      ret = m_minerInfoDSCommDB->RefreshDB();
      break;
    }
    case MINER_INFO_SHARDS: {
      unique_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
      ret = m_minerInfoShardsDB->RefreshDB();
      break;
    }
    case EXTSEED_PUBKEYS: {
      unique_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);
      ret = m_extSeedPubKeysDB->RefreshDB();
      break;
    }
  }
  if (!ret) {
    LOG_GENERAL(INFO, "FAIL: Refresh DB " << type << " failed");
  }
  return ret;
}

std::vector<std::string> BlockStorage::GetDBName(DBTYPE type) {
  std::vector<std::string> ret;
  switch (type) {
    case META: {
      shared_lock<shared_timed_mutex> g(m_mutexMetadata);
      ret.push_back(m_metadataDB->GetDBName());
      break;
    }
    case DS_BLOCK: {
      shared_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
      ret.push_back(m_dsBlockchainDB->GetDBName());
      break;
    }
    case TX_BLOCK: {
      shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret.push_back(m_txBlockchainDB->GetDBName());
      break;
    }
    case TX_BLOCK_AUX: {
      shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret.push_back(m_txBlockchainAuxDB->GetDBName());
      break;
    }
    case TX_BLOCK_HASH_TO_NUM: {
      shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
      ret.push_back(m_txBlockHashToNumDB->GetDBName());
      break;
    }
    case TX_BODY: {
      lock_guard<mutex> g(m_mutexTxBody);
      ret.push_back(m_txBodyDBs.at(0)->GetDBName());
      break;
    }
    case MICROBLOCK: {
      lock_guard<mutex> g(m_mutexMicroBlock);
      ret.push_back(m_microBlockDBs.at(0)->GetDBName());
      break;
    }
    case DS_COMMITTEE: {
      shared_lock<shared_timed_mutex> g(m_mutexDsCommittee);
      ret.push_back(m_dsCommitteeDB->GetDBName());
      break;
    }
    case VC_BLOCK: {
      shared_lock<shared_timed_mutex> g(m_mutexVCBlock);
      ret.push_back(m_VCBlockDB->GetDBName());
      break;
    }
    case BLOCKLINK: {
      shared_lock<shared_timed_mutex> g(m_mutexBlockLink);
      ret.push_back(m_blockLinkDB->GetDBName());
      break;
    }
    case SHARD_STRUCTURE: {
      shared_lock<shared_timed_mutex> g(m_mutexShardStructure);
      ret.push_back(m_shardStructureDB->GetDBName());
      break;
    }
    case STATE_DELTA: {
      shared_lock<shared_timed_mutex> g(m_mutexStateDelta);
      ret.push_back(m_stateDeltaDB->GetDBName());
      break;
    }
    case TEMP_STATE: {
      shared_lock<shared_timed_mutex> g(m_mutexTempState);
      ret.push_back(m_tempStateDB->GetDBName());
      break;
    }
    case DIAGNOSTIC_NODES: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret.push_back(m_diagnosticDBNodes->GetDBName());
      break;
    }
    case DIAGNOSTIC_COINBASE: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret.push_back(m_diagnosticDBCoinbase->GetDBName());
      break;
    }
    case STATE_ROOT: {
      shared_lock<shared_timed_mutex> g(m_mutexStateRoot);
      ret.push_back(m_stateRootDB->GetDBName());
      break;
    }
    case PROCESSED_TEMP: {
      shared_lock<shared_timed_mutex> g(m_mutexProcessTx);
      ret.push_back(m_processedTxnTmpDB->GetDBName());
      break;
    }
    case MINER_INFO_DSCOMM: {
      shared_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
      ret.push_back(m_minerInfoDSCommDB->GetDBName());
      break;
    }
    case MINER_INFO_SHARDS: {
      shared_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
      ret.push_back(m_minerInfoShardsDB->GetDBName());
      break;
    }
    case EXTSEED_PUBKEYS: {
      shared_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);
      ret.push_back(m_extSeedPubKeysDB->GetDBName());
      break;
    }
  }

  return ret;
}

// Don't use short-circuit logical AND (&&) here so that we attempt to reset all
// databases
bool BlockStorage::ResetAll() {
  std::vector<DBTYPE> dbs;
  if (!LOOKUP_NODE_MODE) {
    dbs = {META,
           DS_BLOCK,
           TX_BLOCK,
           TX_BLOCK_HASH_TO_NUM,
           MICROBLOCK,
           DS_COMMITTEE,
           VC_BLOCK,
           BLOCKLINK,
           SHARD_STRUCTURE,
           STATE_DELTA,
           TEMP_STATE,
           DIAGNOSTIC_NODES,
           DIAGNOSTIC_COINBASE,
           STATE_ROOT,
           PROCESSED_TEMP};
  } else  // IS_LOOKUP_NODE
  {
    dbs = {META,
           DS_BLOCK,
           TX_BLOCK,
           TX_BLOCK_HASH_TO_NUM,
           TX_BODY,
           MICROBLOCK,
           DS_COMMITTEE,
           VC_BLOCK,
           BLOCKLINK,
           SHARD_STRUCTURE,
           STATE_DELTA,
           TEMP_STATE,
           DIAGNOSTIC_NODES,
           DIAGNOSTIC_COINBASE,
           STATE_ROOT,
           PROCESSED_TEMP,
           MINER_INFO_DSCOMM,
           MINER_INFO_SHARDS,
           EXTSEED_PUBKEYS};
  }

  auto result = true;
  for (auto db : dbs) {
    result = ResetDB(db) && result;
  }

  return result;
}

// Don't use short-circuit logical AND (&&) here so that we attempt to refresh
// all databases
bool BlockStorage::RefreshAll() {
  std::vector<DBTYPE> dbs;
  if (!LOOKUP_NODE_MODE) {
    dbs = {META,
           DS_BLOCK,
           TX_BLOCK,
           TX_BLOCK_HASH_TO_NUM,
           MICROBLOCK,
           DS_COMMITTEE,
           VC_BLOCK,
           BLOCKLINK,
           SHARD_STRUCTURE,
           STATE_DELTA,
           TEMP_STATE,
           DIAGNOSTIC_NODES,
           DIAGNOSTIC_COINBASE,
           STATE_ROOT,
           PROCESSED_TEMP};
  } else  // IS_LOOKUP_NODE
  {
    dbs = {META,
           DS_BLOCK,
           TX_BLOCK,
           TX_BLOCK_HASH_TO_NUM,
           TX_BODY,
           MICROBLOCK,
           DS_COMMITTEE,
           VC_BLOCK,
           BLOCKLINK,
           SHARD_STRUCTURE,
           STATE_DELTA,
           TEMP_STATE,
           DIAGNOSTIC_NODES,
           DIAGNOSTIC_COINBASE,
           STATE_ROOT,
           PROCESSED_TEMP,
           MINER_INFO_DSCOMM,
           MINER_INFO_SHARDS,
           EXTSEED_PUBKEYS};
  }

  auto result = true;
  for (auto db : dbs) {
    result = RefreshDB(db) && result;
  }

  result =
      Contract::ContractStorage::GetContractStorage().RefreshAll() && result;

  BuildHashToNumberMappingForTxBlocks();
  return result;
}

shared_ptr<LevelDB> BlockStorage::GetMicroBlockDB(const uint64_t& epochNum) {
  const unsigned int dbindex = epochNum / NUM_EPOCHS_PER_PERSISTENT_DB;
  while (m_microBlockDBs.size() <= dbindex) {
    m_microBlockDBs.emplace_back(std::make_shared<LevelDB>(
        string("microBlocks_") + to_string(m_microBlockDBs.size())));
  }
  return m_microBlockDBs.at(dbindex);
}

shared_ptr<LevelDB> BlockStorage::GetTxBodyDB(const uint64_t& epochNum) {
  const unsigned int dbindex = epochNum / NUM_EPOCHS_PER_PERSISTENT_DB;
  while (m_txBodyDBs.size() <= dbindex) {
    m_txBodyDBs.emplace_back(std::make_shared<LevelDB>(
        string("txBodies_") + to_string(m_txBodyDBs.size())));
  }
  return m_txBodyDBs.at(dbindex);
}

void BlockStorage::BuildHashToNumberMappingForTxBlocks() {
  LOG_MARKER();

  std::unique_lock<shared_timed_mutex> lock{m_mutexTxBlockchain};

  const auto maxKnownBlockNumStr =
      m_txBlockchainAuxDB->Lookup(MAX_TX_BLOCK_NUM_KEY);
  // buildTxBlockHashesToNums should be run first to build relevant mapping and
  // storing last known block num in Aux DB.
  if (maxKnownBlockNumStr.empty()) {
    LOG_GENERAL(WARNING,
                "TxBlockAuxiliary databased doesn't contain max known txBlock "
                "number, Eth-api will be malfunctioning");
    return;
  }

  const auto maxKnownBlock = stoull(maxKnownBlockNumStr);
  // Iterate over a range of (maxKnownBlock + 1, maxTxBlockMined) and fill
  // missing gap if needed. Block Numbers are guaranteed to be increasing
  // linearly
  uint64_t currBlock = maxKnownBlock + 1;
  for (;; currBlock++) {
    const auto blockContent = m_txBlockchainDB->Lookup(currBlock);
    if (blockContent.empty()) {
      // There's nothing more to do at this point
      break;
    }
    const TxBlock block{{blockContent.begin(), blockContent.end()}, 0};

    m_txBlockHashToNumDB->Insert(block.GetBlockHash(),
                                 std::to_string(currBlock));
  }

  // Update max known block number if there was anything to process
  currBlock -= 1;
  if (currBlock > maxKnownBlock) {
    m_txBlockchainAuxDB->Insert(leveldb::Slice(MAX_TX_BLOCK_NUM_KEY),
                                leveldb::Slice(std::to_string(currBlock)));
  }
}
