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
#include <iostream>
#include <sstream>
#include <string>

#include <leveldb/db.h>
#include <boost/filesystem.hpp>

#include "BlockStorage.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"

using namespace std;

BlockStorage& BlockStorage::GetBlockStorage() {
  static BlockStorage bs;
  return bs;
}

bool BlockStorage::PutBlock(const uint64_t& blockNum, const bytes& body,
                            const BlockType& blockType) {
  int ret = -1;  // according to LevelDB::Insert return value
  if (blockType == BlockType::DS) {
    ret = m_dsBlockchainDB->Insert(blockNum, body);
    LOG_GENERAL(INFO, "Stored DsBlock  Num:" << blockNum);
  } else if (blockType == BlockType::Tx) {
    ret = m_txBlockchainDB->Insert(blockNum, body);
    LOG_GENERAL(INFO, "Stored TxBlock  Num:" << blockNum);
  }
  return (ret == 0);
}

bool BlockStorage::PutDSBlock(const uint64_t& blockNum, const bytes& body) {
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
  ret = m_VCBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutFallbackBlock(const BlockHash& blockhash,
                                    const bytes& body) {
  int ret = -1;
  ret = m_fallbackBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutBlockLink(const uint64_t& index, const bytes& body) {
  int ret = -1;
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
    ret = m_txBodyDB->Insert(key, body) && m_txBodyTmpDB->Insert(key, body);
  }

  return (ret == 0);
}

bool BlockStorage::PutMicroBlock(const BlockHash& blockHash,
                                 const bytes& body) {
  int ret = m_microBlockDB->Insert(blockHash, body);

  return (ret == 0);
}

bool BlockStorage::InitiateHistoricalDB(const string& path) {
  m_historicalDB = make_shared<LevelDB>("txBodies", path, "");

  return true;
}

bool BlockStorage::GetTxnFromHistoricalDB(const dev::h256& key,
                                          TxBodySharedPtr& body) {
  std::string bodyString;

  bodyString = m_txBodyDB->Lookup(key);

  if (bodyString.empty()) {
    return false;
  }
  body = make_shared<TransactionWithReceipt>(
      bytes(bodyString.begin(), bodyString.end()), 0);

  return true;
}

bool BlockStorage::GetMicroBlock(const BlockHash& blockHash,
                                 MicroBlockSharedPtr& microblock) {
  LOG_MARKER();

  string blockString = m_microBlockDB->Lookup(blockHash);

  if (blockString.empty()) {
    return false;
  }
  microblock =
      make_shared<MicroBlock>(bytes(blockString.begin(), blockString.end()), 0);

  return true;
}

bool BlockStorage::GetRangeMicroBlocks(const uint64_t lowEpochNum,
                                       const uint64_t hiEpochNum,
                                       const uint32_t loShardId,
                                       const uint32_t hiShardId,
                                       list<MicroBlockSharedPtr>& blocks) {
  LOG_MARKER();

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

bool BlockStorage::GetDSBlock(const uint64_t& blockNum,
                              DSBlockSharedPtr& block) {
  string blockString = m_dsBlockchainDB->Lookup(blockNum);

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  LOG_GENERAL(INFO, blockString.length());
  block = DSBlockSharedPtr(
      new DSBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetVCBlock(const BlockHash& blockhash,
                              VCBlockSharedPtr& block) {
  string blockString = m_VCBlockDB->Lookup(blockhash);

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  LOG_GENERAL(INFO, blockString.length());
  block = VCBlockSharedPtr(
      new VCBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::ReleaseDB() {
  m_txBodyDB.reset();
  m_microBlockDB.reset();
  m_VCBlockDB.reset();
  m_txBlockchainDB.reset();
  m_dsBlockchainDB.reset();
  m_fallbackBlockDB.reset();
  m_blockLinkDB.reset();
  return true;
}

bool BlockStorage::GetFallbackBlock(
    const BlockHash& blockhash,
    FallbackBlockSharedPtr& fallbackblockwsharding) {
  string blockString = m_fallbackBlockDB->Lookup(blockhash);

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  LOG_GENERAL(INFO, blockString.length());

  fallbackblockwsharding =
      FallbackBlockSharedPtr(new FallbackBlockWShardingStructure(
          bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetBlockLink(const uint64_t& index,
                                BlockLinkSharedPtr& block) {
  string blockString = m_blockLinkDB->Lookup(index);

  if (blockString.empty()) {
    return false;
  }

  // LOG_GENERAL(INFO, blockString);
  LOG_GENERAL(INFO, blockString.length());
  BlockLink blnk;
  if (!Messenger::GetBlockLink(bytes(blockString.begin(), blockString.end()), 0,
                               blnk)) {
    LOG_GENERAL(WARNING, "Serialization of blockLink failed");
    return false;
  }
  block = make_shared<BlockLink>(blnk);
  return true;
}

bool BlockStorage::GetTxBlock(const uint64_t& blockNum,
                              TxBlockSharedPtr& block) {
  string blockString = m_txBlockchainDB->Lookup(blockNum);

  if (blockString.empty()) {
    return false;
  }

  block = TxBlockSharedPtr(
      new TxBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body) {
  std::string bodyString;

  bodyString = m_txBodyDB->Lookup(key);

  if (bodyString.empty()) {
    return false;
  }
  body = TxBodySharedPtr(new TransactionWithReceipt(
      bytes(bodyString.begin(), bodyString.end()), 0));

  return true;
}

bool BlockStorage::DeleteDSBlock(const uint64_t& blocknum) {
  LOG_GENERAL(INFO, "Delete DSBlock Num: " << blocknum);
  int ret = m_dsBlockchainDB->DeleteKey(blocknum);
  return (ret == 0);
}

bool BlockStorage::DeleteVCBlock(const BlockHash& blockhash) {
  int ret = m_VCBlockDB->DeleteKey(blockhash);
  return (ret == 0);
}

bool BlockStorage::DeleteFallbackBlock(const BlockHash& blockhash) {
  int ret = m_fallbackBlockDB->DeleteKey(blockhash);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBlock(const uint64_t& blocknum) {
  LOG_GENERAL(INFO, "Delete TxBlock Num: " << blocknum);
  int ret = m_txBlockchainDB->DeleteKey(blocknum);
  return (ret == 0);
}

bool BlockStorage::DeleteTxBody(const dev::h256& key) {
  int ret;
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this");
    return false;
  } else {
    ret = m_txBodyDB->DeleteKey(key);
  }

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

bool BlockStorage::GetAllTxBlocks(std::list<TxBlockSharedPtr>& blocks) {
  LOG_MARKER();

  leveldb::Iterator* it =
      m_txBlockchainDB->GetDB()->NewIterator(leveldb::ReadOptions());
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
    LOG_GENERAL(INFO, "Retrievd TxBlock Num:" << bns);
  }

  delete it;

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no TxBlock");
    return false;
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
  int ret = m_metadataDB->Insert(std::to_string((int)type), data);
  return (ret == 0);
}

bool BlockStorage::GetMetadata(MetaType type, bytes& data) {
  LOG_MARKER();
  string metaString = m_metadataDB->Lookup(std::to_string((int)type));

  if (metaString.empty()) {
    LOG_GENERAL(INFO, "No metadata get")
    return false;
  }

  data = bytes(metaString.begin(), metaString.end());

  return true;
}

bool BlockStorage::PutDSCommittee(const shared_ptr<DequeOfDSNode>& dsCommittee,
                                  const uint16_t& consensusLeaderID) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexDsCommittee);
  m_dsCommitteeDB->ResetDB();
  unsigned int index = 0;
  string leaderId = to_string(consensusLeaderID);

  if (0 != m_dsCommitteeDB->Insert(index++,
                                   bytes(leaderId.begin(), leaderId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store DS leader ID:" << consensusLeaderID);
    return false;
  }

  LOG_GENERAL(INFO, "Stored DS leader ID:" << consensusLeaderID);

  bytes data;

  for (const auto& ds : *dsCommittee) {
    int pubKeySize = ds.first.Serialize(data, 0);
    ds.second.Serialize(data, pubKeySize);

    /// Store index as key, to guarantee the sequence of DS committee after
    /// retrieval Because first DS committee is DS leader
    if (0 != m_dsCommitteeDB->Insert(index++, data)) {
      LOG_GENERAL(WARNING, "Failed to store DS committee:" << ds.first << ", "
                                                           << ds.second);
      return false;
    }

    LOG_GENERAL(INFO, "Stored DS committee:" << ds.first << ", " << ds.second);
  }

  return true;
}

bool BlockStorage::GetDSCommittee(
    shared_ptr<deque<pair<PubKey, Peer>>>& dsCommittee,
    uint16_t& consensusLeaderID) {
  LOG_MARKER();

  unsigned int index = 0;
  lock_guard<mutex> g(m_mutexDsCommittee);
  string strConsensusLeaderID = m_dsCommitteeDB->Lookup(index++);

  if (strConsensusLeaderID.empty()) {
    LOG_GENERAL(WARNING, "Cannot retrieve DS committee!");
    return false;
  }

  consensusLeaderID = stoul(strConsensusLeaderID);
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

  lock_guard<mutex> g(m_mutexShardStructure);
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

  if (!Messenger::ShardStructureToArray(shardStructure, 0, shards)) {
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
    lock_guard<mutex> g(m_mutexShardStructure);
    dataStr = m_shardStructureDB->Lookup(index++);
  }

  Messenger::ArrayToShardStructure(bytes(dataStr.begin(), dataStr.end()), 0,
                                   shards);
  LOG_GENERAL(INFO, "Retrieved sharding structure");
  return true;
}

bool BlockStorage::PutStateDelta(const uint64_t& finalBlockNum,
                                 const bytes& stateDelta) {
  LOG_MARKER();

  if (0 != m_stateDeltaDB->Insert(finalBlockNum, stateDelta)) {
    LOG_PAYLOAD(WARNING,
                "Failed to store state delta of final block " << finalBlockNum,
                stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
    return false;
  }

  LOG_PAYLOAD(INFO, "Stored state delta of final block " << finalBlockNum,
              stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
  return true;
}

bool BlockStorage::GetStateDelta(const uint64_t& finalBlockNum,
                                 bytes& stateDelta) {
  LOG_MARKER();

  string dataStr = m_stateDeltaDB->Lookup(finalBlockNum);
  stateDelta = bytes(dataStr.begin(), dataStr.end());
  LOG_PAYLOAD(INFO, "Retrieved state delta of final block " << finalBlockNum,
              stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
  return true;
}

bool BlockStorage::PutDiagnosticData(const uint64_t& dsBlockNum,
                                     const DequeOfShard& shards,
                                     const DequeOfDSNode& dsCommittee) {
  LOG_MARKER();

  bytes data;

  if (!Messenger::SetDiagnosticData(data, 0, shards, dsCommittee)) {
    LOG_GENERAL(WARNING, "Messenger::SetDiagnosticData failed");
    return false;
  }

  lock_guard<mutex> g(m_mutexDiagnostic);

  if (0 != m_diagnosticDB->Insert(dsBlockNum, data)) {
    LOG_GENERAL(WARNING, "Failed to store diagnostic data");
    return false;
  }

  m_diagnosticDBCounter++;

  return true;
}

bool BlockStorage::GetDiagnosticData(const uint64_t& dsBlockNum,
                                     DequeOfShard& shards,
                                     DequeOfDSNode& dsCommittee) {
  LOG_MARKER();

  string dataStr;

  {
    lock_guard<mutex> g(m_mutexDiagnostic);
    dataStr = m_diagnosticDB->Lookup(dsBlockNum);
  }

  if (dataStr.empty()) {
    LOG_GENERAL(WARNING,
                "Failed to retrieve diagnostic data for DS block number "
                    << dsBlockNum);
    return false;
  }

  bytes data(dataStr.begin(), dataStr.end());

  if (!Messenger::GetDiagnosticData(data, 0, shards, dsCommittee)) {
    LOG_GENERAL(WARNING, "Messenger::GetDiagnosticData failed");
    return false;
  }

  return true;
}

void BlockStorage::GetDiagnosticData(
    map<uint64_t, DiagnosticData>& diagnosticDataMap) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexDiagnostic);

  leveldb::Iterator* it =
      m_diagnosticDB->GetDB()->NewIterator(leveldb::ReadOptions());

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
      dsBlockNum = stoul(dsBlockNumStr);
    } catch (...) {
      LOG_GENERAL(WARNING,
                  "Non-numeric key " << dsBlockNumStr << " at index " << index);
      continue;
    }

    bytes data(dataStr.begin(), dataStr.end());

    DiagnosticData entry;

    if (!Messenger::GetDiagnosticData(data, 0, entry.shards,
                                      entry.dsCommittee)) {
      LOG_GENERAL(WARNING,
                  "Messenger::GetDiagnosticData failed for DS block number "
                      << dsBlockNumStr << " at index " << index);
      continue;
    }

    diagnosticDataMap.emplace(make_pair(dsBlockNum, entry));

    index++;
  }
}

unsigned int BlockStorage::GetDiagnosticDataCount() {
  lock_guard<mutex> g(m_mutexDiagnostic);
  return m_diagnosticDBCounter;
}

bool BlockStorage::DeleteDiagnosticData(const uint64_t& dsBlockNum) {
  lock_guard<mutex> g(m_mutexDiagnostic);
  bool result = (0 == m_diagnosticDB->DeleteKey(dsBlockNum));
  if (result) {
    m_diagnosticDBCounter--;
  }
  return result;
}

bool BlockStorage::ResetDB(DBTYPE type) {
  bool ret = false;
  switch (type) {
    case META: {
      lock_guard<mutex> g(m_mutexMetadata);
      ret = m_metadataDB->ResetDB();
      break;
    }
    case DS_BLOCK: {
      lock_guard<mutex> g(m_mutexDsBlockchain);
      ret = m_dsBlockchainDB->ResetDB();
      break;
    }
    case TX_BLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockchain);
      ret = m_txBlockchainDB->ResetDB();
      break;
    }
    case TX_BODY: {
      lock_guard<mutex> g(m_mutexTxBody);
      ret = m_txBodyDB->ResetDB();
      break;
    }
    case TX_BODY_TMP: {
      lock_guard<mutex> g(m_mutexTxBodyTmp);
      ret = m_txBodyTmpDB->ResetDB();
      break;
    }
    case MICROBLOCK: {
      lock_guard<mutex> g(m_mutexMicroBlock);
      ret = m_microBlockDB->ResetDB();
      break;
    }
    case DS_COMMITTEE: {
      lock_guard<mutex> g(m_mutexDsCommittee);
      ret = m_dsCommitteeDB->ResetDB();
      break;
    }
    case VC_BLOCK: {
      lock_guard<mutex> g(m_mutexVCBlock);
      ret = m_VCBlockDB->ResetDB();
      break;
    }
    case FB_BLOCK: {
      lock_guard<mutex> g(m_mutexFallbackBlock);
      ret = m_fallbackBlockDB->ResetDB();
      break;
    }
    case BLOCKLINK: {
      lock_guard<mutex> g(m_mutexBlockLink);
      ret = m_blockLinkDB->ResetDB();
      break;
    }
    case SHARD_STRUCTURE: {
      lock_guard<mutex> g(m_mutexShardStructure);
      ret = m_shardStructureDB->ResetDB();
      break;
    }
    case STATE_DELTA: {
      lock_guard<mutex> g(m_mutexStateDelta);
      ret = m_stateDeltaDB->ResetDB();
      break;
    }
    case DIAGNOSTIC: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret = m_diagnosticDB->ResetDB();
      if (ret) {
        m_diagnosticDBCounter = 0;
      }
      break;
    }
  }
  if (!ret) {
    LOG_GENERAL(INFO, "FAIL: Reset DB " << type << " failed");
  }
  return ret;
}

std::vector<std::string> BlockStorage::GetDBName(DBTYPE type) {
  std::vector<std::string> ret;
  switch (type) {
    case META: {
      lock_guard<mutex> g(m_mutexMetadata);
      ret.push_back(m_metadataDB->GetDBName());
      break;
    }
    case DS_BLOCK: {
      lock_guard<mutex> g(m_mutexDsBlockchain);
      ret.push_back(m_dsBlockchainDB->GetDBName());
      break;
    }
    case TX_BLOCK: {
      lock_guard<mutex> g(m_mutexTxBlockchain);
      ret.push_back(m_txBlockchainDB->GetDBName());
      break;
    }
    case TX_BODY: {
      lock_guard<mutex> g(m_mutexTxBody);
      ret.push_back(m_txBodyDB->GetDBName());
      break;
    }
    case TX_BODY_TMP: {
      lock_guard<mutex> g(m_mutexTxBodyTmp);
      ret.push_back(m_txBodyTmpDB->GetDBName());
      break;
    }
    case MICROBLOCK: {
      lock_guard<mutex> g(m_mutexMicroBlock);
      ret.push_back(m_microBlockDB->GetDBName());
      break;
    }
    case DS_COMMITTEE: {
      lock_guard<mutex> g(m_mutexDsCommittee);
      ret.push_back(m_dsCommitteeDB->GetDBName());
      break;
    }
    case VC_BLOCK: {
      lock_guard<mutex> g(m_mutexVCBlock);
      ret.push_back(m_VCBlockDB->GetDBName());
      break;
    }
    case FB_BLOCK: {
      lock_guard<mutex> g(m_mutexFallbackBlock);
      ret.push_back(m_fallbackBlockDB->GetDBName());
      break;
    }
    case BLOCKLINK: {
      lock_guard<mutex> g(m_mutexBlockLink);
      ret.push_back(m_blockLinkDB->GetDBName());
      break;
    }
    case SHARD_STRUCTURE: {
      lock_guard<mutex> g(m_mutexShardStructure);
      ret.push_back(m_shardStructureDB->GetDBName());
      break;
    }
    case STATE_DELTA: {
      lock_guard<mutex> g(m_mutexStateDelta);
      ret.push_back(m_stateDeltaDB->GetDBName());
      break;
    }
    case DIAGNOSTIC: {
      lock_guard<mutex> g(m_mutexDiagnostic);
      ret.push_back(m_diagnosticDB->GetDBName());
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
           ResetDB(STATE_DELTA);
  } else  // IS_LOOKUP_NODE
  {
    return ResetDB(META) & ResetDB(DS_BLOCK) & ResetDB(TX_BLOCK) &
           ResetDB(TX_BODY) & ResetDB(TX_BODY_TMP) & ResetDB(MICROBLOCK) &
           ResetDB(DS_COMMITTEE) & ResetDB(VC_BLOCK) & ResetDB(FB_BLOCK) &
           ResetDB(BLOCKLINK) & ResetDB(SHARD_STRUCTURE) &
           ResetDB(STATE_DELTA) & ResetDB(DIAGNOSTIC);
  }
}
