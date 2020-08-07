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
#include "libUtils/DataConversion.h"

using namespace std;

BlockStorage& BlockStorage::GetBlockStorage(const std::string& path,
                                            bool diagnostic) {
  static BlockStorage bs(path, diagnostic);
  return bs;
}

bool BlockStorage::PutBlock(const uint64_t& blockNum, const bytes& body,
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

bool BlockStorage::PutDSBlock(const uint64_t& blockNum, const bytes& body) {
  // return PutBlock(blockNum, body, BlockType::DS);
  bool ret = false;
  if (PutBlock(blockNum, body, BlockType::DS)) {
    if (PutMetadata(MetaType::DSINCOMPLETED, {'1'})) {
      ret = true;
    } else {
      if (!DeleteDSBlock(blockNum)) {
        LOG_GENERAL(INFO, "FAIL: Delete DSBlock" << blockNum << "Failed");
      }
    }
  }
  return ret;
}

bool BlockStorage::PutVCBlock(const BlockHash& blockhash, const bytes& body) {
  int ret = -1;
  unique_lock<shared_timed_mutex> g(m_mutexVCBlock);
  ret = m_VCBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutFallbackBlock(const BlockHash& blockhash,
                                    const bytes& body) {
  int ret = -1;
  unique_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
  ret = m_fallbackBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutBlockLink(const uint64_t& index, const bytes& body) {
  int ret = -1;
  unique_lock<shared_timed_mutex> g(m_mutexBlockLink);
  ret = m_blockLinkDB->Insert(index, body);
  return (ret == 0);
}

bool BlockStorage::PutTxBlock(const uint64_t& blockNum, const bytes& body) {
  return PutBlock(blockNum, body, BlockType::Tx);
}

bool BlockStorage::PutTxBody(const dev::h256& key, const bytes& body) {
  int ret;

  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
    return false;
  } else  // IS_LOOKUP_NODE
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxBody);
    ret = m_txBodyDB->Insert(key, body) && m_txBodyTmpDB->Insert(key, body);
  }

  return (ret == 0);
}

bool BlockStorage::PutProcessedTxBodyTmp(const dev::h256& key,
                                         const bytes& body) {
  int ret;
  {
    unique_lock<shared_timed_mutex> g(m_mutexProcessTx);
    ret = m_processedTxnTmpDB->Insert(key, body);
  }
  return (ret == 0);
}

bool BlockStorage::PutMicroBlock(const BlockHash& blockHash,
                                 const bytes& body) {
  unique_lock<shared_timed_mutex> g(m_mutexMicroBlock);
  int ret = m_microBlockDB->Insert(blockHash, body);

  return (ret == 0);
}

bool BlockStorage::InitiateHistoricalDB(const string& path) {
  // If not explicitly convert to string, calls the other constructor
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxnHistorical);
    m_txnHistoricalDB = make_shared<LevelDB>("txBodies", path, (string) "");
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexMBHistorical);
    m_MBHistoricalDB = make_shared<LevelDB>("microBlocks", path, (string) "");
  }

  return true;
}

bool BlockStorage::GetTxnFromHistoricalDB(const dev::h256& key,
                                          TxBodySharedPtr& body) {
  std::string bodyString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexTxnHistorical);
    bodyString = m_txnHistoricalDB->Lookup(key);
  }
  if (bodyString.empty()) {
    return false;
  }
  body = make_shared<TransactionWithReceipt>(
      bytes(bodyString.begin(), bodyString.end()), 0);

  return true;
}

bool BlockStorage::GetHistoricalMicroBlock(const BlockHash& blockhash,
                                           MicroBlockSharedPtr& microblock) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMBHistorical);
    blockString = m_MBHistoricalDB->Lookup(blockhash);
  }

  if (blockString.empty()) {
    return false;
  }

  microblock =
      make_shared<MicroBlock>(bytes(blockString.begin(), blockString.end()), 0);

  return true;
}

bool BlockStorage::GetMicroBlock(const BlockHash& blockHash,
                                 MicroBlockSharedPtr& microblock) {
  // LOG_MARKER();

  string blockString;

  {
    shared_lock<shared_timed_mutex> g(m_mutexMicroBlock);
    blockString = m_microBlockDB->Lookup(blockHash);
  }

  if (blockString.empty()) {
    return false;
  }
  microblock =
      make_shared<MicroBlock>(bytes(blockString.begin(), blockString.end()), 0);

  return true;
}

bool BlockStorage::CheckMicroBlock(const BlockHash& blockHash) {
  shared_lock<shared_timed_mutex> g(m_mutexMicroBlock);
  return m_microBlockDB->Exists(blockHash);
}

bool BlockStorage::GetRangeMicroBlocks(const uint64_t lowEpochNum,
                                       const uint64_t hiEpochNum,
                                       const uint32_t loShardId,
                                       const uint32_t hiShardId,
                                       list<MicroBlockSharedPtr>& blocks) {
  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexMicroBlock);

  leveldb::Iterator* it =
      m_microBlockDB->GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one block in the chain");
      delete it;
      return false;
    }
    MicroBlockSharedPtr block = MicroBlockSharedPtr(
        new MicroBlock(bytes(blockString.begin(), blockString.end()), 0));

    if (block->GetHeader().GetEpochNum() < lowEpochNum ||
        block->GetHeader().GetEpochNum() > hiEpochNum ||
        block->GetHeader().GetShardId() < loShardId ||
        block->GetHeader().GetShardId() > hiShardId) {
      continue;
    }

    blocks.emplace_back(block);
    LOG_GENERAL(INFO, "Retrievd MicroBlock Num:" << bns);
  }

  delete it;

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
    bytes rawBytes;
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
    if (!acct.DeserializeBase(bytes(acct_string.begin(), acct_string.end()),
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
      new DSBlock(bytes(blockString.begin(), blockString.end()), 0));

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
      new VCBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::ReleaseDB() {
  {
    unique_lock<shared_timed_mutex> g(m_mutexTxBody);
    m_txBodyDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexMicroBlock);
    m_microBlockDB.reset();
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
    unique_lock<shared_timed_mutex> g(m_mutexDsBlockchain);
    m_dsBlockchainDB.reset();
  }
  {
    unique_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
    m_fallbackBlockDB.reset();
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

bool BlockStorage::GetFallbackBlock(
    const BlockHash& blockhash,
    FallbackBlockSharedPtr& fallbackblockwsharding) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
    blockString = m_fallbackBlockDB->Lookup(blockhash);
  }

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  // LOG_GENERAL(INFO, blockString.length());

  fallbackblockwsharding =
      FallbackBlockSharedPtr(new FallbackBlockWShardingStructure(
          bytes(blockString.begin(), blockString.end()), 0));

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
  if (!Messenger::GetBlockLink(bytes(blockString.begin(), blockString.end()), 0,
                               blnk)) {
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
                              TxBlockSharedPtr& block) {
  string blockString;
  {
    shared_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
    blockString = m_txBlockchainDB->Lookup(blockNum);
  }
  if (blockString.empty()) {
    return false;
  }

  block = TxBlockSharedPtr(
      new TxBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetLatestTxBlock(TxBlockSharedPtr& block) {
  uint64_t latestTxBlockNum = 0;

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

  return GetTxBlock(latestTxBlockNum, block);
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body) {
  std::string bodyString;

  {
    shared_lock<shared_timed_mutex> g(m_mutexTxBody);
    bodyString = m_txBodyDB->Lookup(key);
  }

  if (bodyString.empty()) {
    return false;
  }
  body = TxBodySharedPtr(new TransactionWithReceipt(
      bytes(bodyString.begin(), bodyString.end()), 0));

  return true;
}

bool BlockStorage::CheckTxBody(const dev::h256& key) {
  shared_lock<shared_timed_mutex> g(m_mutexTxBody);
  return m_txBodyDB->Exists(key);
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

bool BlockStorage::DeleteFallbackBlock(const BlockHash& blockhash) {
  unique_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
  int ret = m_fallbackBlockDB->DeleteKey(blockhash);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBlock(const uint64_t& blocknum) {
  LOG_GENERAL(INFO, "Delete TxBlock Num: " << blocknum);
  unique_lock<shared_timed_mutex> g(m_mutexTxBlockchain);
  int ret = m_txBlockchainDB->DeleteKey(blocknum);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBody(const dev::h256& key) {
  int ret;
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this");
    return false;
  } else {
    unique_lock<shared_timed_mutex> g(m_mutexTxBody);
    ret = m_txBodyDB->DeleteKey(key);
  }

  return (ret == 0);
}

bool BlockStorage::DeleteMicroBlock(const BlockHash& blockHash) {
  unique_lock<shared_timed_mutex> g(m_mutexMicroBlock);
  int ret = m_microBlockDB->DeleteKey(blockHash);

  return (ret == 0);
}

bool BlockStorage::DeleteStateDelta(const uint64_t& finalBlockNum) {
  unique_lock<shared_timed_mutex> g(m_mutexStateDelta);

  int ret = m_stateDeltaDB->DeleteKey(finalBlockNum);

  return (ret == 0);
}

// bool BlockStorage::PutTxBody(const string & key, const bytes
// & body)
// {
//     int ret = m_txBodyDB.Insert(key, body);
//     return (ret == 0);
// }

// void BlockStorage::GetTxBody(const string & key, TxBodySharedPtr & body)
// {
//     string bodyString = m_txBodyDB.Lookup(key);
//     const unsigned char* raw_memory = reinterpret_cast<const unsigned
//     char*>(bodyString.c_str()); body = TxBodySharedPtr( new
//     Transaction(bytes(raw_memory,
//                                             raw_memory + bodyString.size()),
//                                             0) );
// }

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
        new DSBlock(bytes(blockString.begin(), blockString.end()), 0));
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

  bytes data;
  pubK.Serialize(data, 0);
  LOG_GENERAL(INFO, "Inserting with key:" << keyStr << ", Pubkey:" << pubK);
  int ret = m_extSeedPubKeysDB->Insert(keyStr, data);
  return (ret == 0);
}

bool BlockStorage::DeleteExtSeedPubKey(const PubKey& pubK) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexExtSeedPubKeys);

  leveldb::Iterator* it =
      m_extSeedPubKeysDB->GetDB()->NewIterator(leveldb::ReadOptions());
  bytes data;
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
    PubKey pubK(bytes(pubkString.begin(), pubkString.end()), 0);
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
        new TxBlock(bytes(blockString.begin(), blockString.end()), 0));
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
        new VCBlock(bytes(blockString.begin(), blockString.end()), 0));
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

bool BlockStorage::GetAllTxBodiesTmp(std::list<TxnHash>& txnHashes) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "BlockStorage::GetAllTxBodiesTmp not expected to be called "
                "from other than the LookUp node.");
    return true;
  }

  LOG_MARKER();

  shared_lock<shared_timed_mutex> g(m_mutexTxBodyTmp);

  leveldb::Iterator* it =
      m_txBodyTmpDB->GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string hashString = it->key().ToString();
    if (hashString.empty()) {
      LOG_GENERAL(WARNING, "Lost one Tmp txBody Hash");
      delete it;
      return false;
    }
    TxnHash txnHash(hashString);
    txnHashes.emplace_back(txnHash);
  }

  delete it;
  return true;
}

bool BlockStorage::GetAllBlockLink(std::list<BlockLink>& blocklinks) {
  LOG_MARKER();

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
    if (!Messenger::GetBlockLink(bytes(blockString.begin(), blockString.end()),
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
    LOG_GENERAL(INFO, "Retrievd BlockLink Num:" << bns);
  }
  delete it;
  if (blocklinks.empty()) {
    LOG_GENERAL(INFO, "Disk has no blocklink");
    return false;
  }
  return true;
}

bool BlockStorage::PutMetadata(MetaType type, const bytes& data) {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_mutexMetadata);
  int ret = m_metadataDB->Insert(std::to_string((int)type), data);
  return (ret == 0);
}

bool BlockStorage::PutStateRoot(const bytes& data) {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_mutexStateRoot);
  int ret = m_stateRootDB->Insert(std::to_string((int)STATEROOT), data);
  return (ret == 0);
}

bool BlockStorage::PutLatestEpochStatesUpdated(const uint64_t& epochNum) {
  LOG_MARKER();
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

bool BlockStorage::GetMetadata(MetaType type, bytes& data, bool muteLog) {
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

  data = bytes(metaString.begin(), metaString.end());

  return true;
}

bool BlockStorage::GetStateRoot(bytes& data) {
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

  data = bytes(stateRoot.begin(), stateRoot.end());

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
  bytes epochFinBytes;
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
                                   bytes(leaderId.begin(), leaderId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store DS leader ID:" << consensusLeaderID);
    return false;
  }

  LOG_GENERAL(INFO, "DS leader: " << consensusLeaderID);

  bytes data;

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
        PubKey(bytes(dataStr.begin(), dataStr.begin() + PUB_KEY_SIZE), 0),
        Peer(bytes(dataStr.begin() + PUB_KEY_SIZE, dataStr.end()), 0));
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
                                      bytes(shardId.begin(), shardId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store shard ID:" << myshardId);
    return false;
  }

  LOG_GENERAL(INFO, "Stored shard ID:" << myshardId);

  bytes shardStructure;

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
  Messenger::ArrayToShardStructure(bytes(dataStr.begin(), dataStr.end()), 0,
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
                                 const bytes& stateDelta) {
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
                                 bytes& stateDelta) {
  LOG_MARKER();
  bool found = false;

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexStateDelta);
    dataStr = m_stateDeltaDB->Lookup(finalBlockNum, found);
  }
  if (found) {
    stateDelta = bytes(dataStr.begin(), dataStr.end());
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

  bytes data;

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

  bytes data;

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

  bytes data(dataStr.begin(), dataStr.end());

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

  bytes data(dataStr.begin(), dataStr.end());

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

    bytes data(dataStr.begin(), dataStr.end());

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

    bytes data(dataStr.begin(), dataStr.end());

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

  bytes data;

  if (!Messenger::SetMinerInfoDSComm(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::SetMinerInfoDSComm failed");
    return false;
  }

  unique_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);

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

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMinerInfoDSComm);
    dataStr = m_minerInfoDSCommDB->Lookup(dsBlockNum, found);
  }
  if (found) {
    if (!Messenger::GetMinerInfoDSComm(bytes(dataStr.begin(), dataStr.end()), 0,
                                       entry)) {
      LOG_GENERAL(WARNING, "Messenger::GetMinerInfoDSComm failed");
      found = false;
    }
  }

  return found;
}

bool BlockStorage::PutMinerInfoShards(const uint64_t& dsBlockNum,
                                      const MinerInfoShards& entry) {
  LOG_MARKER();

  bytes data;

  if (!Messenger::SetMinerInfoShards(data, 0, entry)) {
    LOG_GENERAL(WARNING, "Messenger::SetMinerInfoShards failed");
    return false;
  }

  unique_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);

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

  string dataStr;
  {
    shared_lock<shared_timed_mutex> g(m_mutexMinerInfoShards);
    dataStr = m_minerInfoShardsDB->Lookup(dsBlockNum, found);
  }
  if (found) {
    if (!Messenger::GetMinerInfoShards(bytes(dataStr.begin(), dataStr.end()), 0,
                                       entry)) {
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
    case TX_BODY: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBody);
      ret = m_txBodyDB->ResetDB();
      break;
    }
    case TX_BODY_TMP: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBodyTmp);
      ret = m_txBodyTmpDB->ResetDB();
      break;
    }
    case MICROBLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexMicroBlock);
      ret = m_microBlockDB->ResetDB();
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
    case FB_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
      ret = m_fallbackBlockDB->ResetDB();
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
  LOG_MARKER();
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
    case TX_BODY: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBody);
      ret = m_txBodyDB->RefreshDB();
      break;
    }
    case TX_BODY_TMP: {
      unique_lock<shared_timed_mutex> g(m_mutexTxBodyTmp);
      ret = m_txBodyTmpDB->RefreshDB();
      break;
    }
    case MICROBLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexMicroBlock);
      ret = m_microBlockDB->RefreshDB();
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
    case FB_BLOCK: {
      unique_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
      ret = m_fallbackBlockDB->RefreshDB();
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
    case TX_BODY: {
      shared_lock<shared_timed_mutex> g(m_mutexTxBody);
      ret.push_back(m_txBodyDB->GetDBName());
      break;
    }
    case TX_BODY_TMP: {
      shared_lock<shared_timed_mutex> g(m_mutexTxBodyTmp);
      ret.push_back(m_txBodyTmpDB->GetDBName());
      break;
    }
    case MICROBLOCK: {
      shared_lock<shared_timed_mutex> g(m_mutexMicroBlock);
      ret.push_back(m_microBlockDB->GetDBName());
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
    case FB_BLOCK: {
      shared_lock<shared_timed_mutex> g(m_mutexFallbackBlock);
      ret.push_back(m_fallbackBlockDB->GetDBName());
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
  if (!LOOKUP_NODE_MODE) {
    return ResetDB(META) & ResetDB(DS_BLOCK) & ResetDB(TX_BLOCK) &
           ResetDB(MICROBLOCK) & ResetDB(DS_COMMITTEE) & ResetDB(VC_BLOCK) &
           ResetDB(FB_BLOCK) & ResetDB(BLOCKLINK) & ResetDB(SHARD_STRUCTURE) &
           ResetDB(STATE_DELTA) & ResetDB(TEMP_STATE) &
           ResetDB(DIAGNOSTIC_NODES) & ResetDB(DIAGNOSTIC_COINBASE) &
           ResetDB(STATE_ROOT) & ResetDB(PROCESSED_TEMP);
  } else  // IS_LOOKUP_NODE
  {
    return ResetDB(META) & ResetDB(DS_BLOCK) & ResetDB(TX_BLOCK) &
           ResetDB(TX_BODY) & ResetDB(TX_BODY_TMP) & ResetDB(MICROBLOCK) &
           ResetDB(DS_COMMITTEE) & ResetDB(VC_BLOCK) & ResetDB(FB_BLOCK) &
           ResetDB(BLOCKLINK) & ResetDB(SHARD_STRUCTURE) &
           ResetDB(STATE_DELTA) & ResetDB(TEMP_STATE) &
           ResetDB(DIAGNOSTIC_NODES) & ResetDB(DIAGNOSTIC_COINBASE) &
           ResetDB(STATE_ROOT) & ResetDB(PROCESSED_TEMP) &
           ResetDB(MINER_INFO_DSCOMM) & ResetDB(MINER_INFO_SHARDS) &
           ResetDB(EXTSEED_PUBKEYS);
  }
}

// Don't use short-circuit logical AND (&&) here so that we attempt to refresh
// all databases
bool BlockStorage::RefreshAll() {
  if (!LOOKUP_NODE_MODE) {
    return RefreshDB(META) & RefreshDB(DS_BLOCK) & RefreshDB(TX_BLOCK) &
           RefreshDB(MICROBLOCK) & RefreshDB(DS_COMMITTEE) &
           RefreshDB(VC_BLOCK) & RefreshDB(FB_BLOCK) & RefreshDB(BLOCKLINK) &
           RefreshDB(SHARD_STRUCTURE) & RefreshDB(STATE_DELTA) &
           RefreshDB(TEMP_STATE) & RefreshDB(DIAGNOSTIC_NODES) &
           RefreshDB(DIAGNOSTIC_COINBASE) & RefreshDB(STATE_ROOT) &
           RefreshDB(PROCESSED_TEMP) &
           Contract::ContractStorage2::GetContractStorage().RefreshAll();
  } else  // IS_LOOKUP_NODE
  {
    return RefreshDB(META) & RefreshDB(DS_BLOCK) & RefreshDB(TX_BLOCK) &
           RefreshDB(TX_BODY) & RefreshDB(TX_BODY_TMP) & RefreshDB(MICROBLOCK) &
           RefreshDB(DS_COMMITTEE) & RefreshDB(VC_BLOCK) & RefreshDB(FB_BLOCK) &
           RefreshDB(BLOCKLINK) & RefreshDB(SHARD_STRUCTURE) &
           RefreshDB(STATE_DELTA) & RefreshDB(TEMP_STATE) &
           RefreshDB(DIAGNOSTIC_NODES) & RefreshDB(DIAGNOSTIC_COINBASE) &
           RefreshDB(STATE_ROOT) & RefreshDB(PROCESSED_TEMP) &
           RefreshDB(MINER_INFO_DSCOMM) & RefreshDB(MINER_INFO_SHARDS) &
           RefreshDB(EXTSEED_PUBKEYS) &
           Contract::ContractStorage2::GetContractStorage().RefreshAll();
  }
}