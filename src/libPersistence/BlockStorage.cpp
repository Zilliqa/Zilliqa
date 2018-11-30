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

bool BlockStorage::PutBlock(const uint64_t& blockNum,
                            const vector<unsigned char>& body,
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

bool BlockStorage::PutDSBlock(const uint64_t& blockNum,
                              const vector<unsigned char>& body) {
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

bool BlockStorage::PutVCBlock(const BlockHash& blockhash,
                              const vector<unsigned char>& body) {
  int ret = -1;
  ret = m_VCBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutFallbackBlock(const BlockHash& blockhash,
                                    const vector<unsigned char>& body) {
  int ret = -1;
  ret = m_fallbackBlockDB->Insert(blockhash, body);
  return (ret == 0);
}

bool BlockStorage::PutBlockLink(const uint64_t& index,
                                const vector<unsigned char>& body) {
  int ret = -1;
  ret = m_blockLinkDB->Insert(index, body);
  return (ret == 0);
}

bool BlockStorage::PutTxBlock(const uint64_t& blockNum,
                              const vector<unsigned char>& body) {
  return PutBlock(blockNum, body, BlockType::Tx);
}

bool BlockStorage::PutTxBody(const dev::h256& key,
                             const vector<unsigned char>& body) {
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
                                 const vector<unsigned char>& body) {
  int ret = m_microBlockDB->Insert(blockHash, body);

  return (ret == 0);
}

bool BlockStorage::GetMicroBlock(const BlockHash& blockHash,
                                 MicroBlockSharedPtr& microblock) {
  LOG_MARKER();

  string blockString = m_microBlockDB->Lookup(blockHash);

  if (blockString.empty()) {
    return false;
  }
  microblock = make_shared<MicroBlock>(
      vector<unsigned char>(blockString.begin(), blockString.end()), 0);

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
    MicroBlockSharedPtr block = MicroBlockSharedPtr(new MicroBlock(
        std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

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
  block = DSBlockSharedPtr(new DSBlock(
      std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

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
  block = VCBlockSharedPtr(new VCBlock(
      std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

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
          std::vector<unsigned char>(blockString.begin(), blockString.end()),
          0));

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
  if (!Messenger::GetBlockLink(
          vector<unsigned char>(blockString.begin(), blockString.end()), 0,
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

  block = TxBlockSharedPtr(new TxBlock(
      std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));

  return true;
}

bool BlockStorage::GetTxBody(const dev::h256& key, TxBodySharedPtr& body) {
  std::string bodyString;
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "Non lookup node should not trigger this.");
    return false;
  } else  // IS_LOOKUP_NODE
  {
    bodyString = m_txBodyDB->Lookup(key);
  }

  if (bodyString.empty()) {
    return false;
  }
  body = TxBodySharedPtr(new TransactionWithReceipt(
      std::vector<unsigned char>(bodyString.begin(), bodyString.end()), 0));

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

// bool BlockStorage::PutTxBody(const string & key, const vector<unsigned char>
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
//     Transaction(std::vector<unsigned char>(raw_memory,
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

    DSBlockSharedPtr block = DSBlockSharedPtr(new DSBlock(
        std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));
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
    TxBlockSharedPtr block = TxBlockSharedPtr(new TxBlock(
        std::vector<unsigned char>(blockString.begin(), blockString.end()), 0));
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
    if (!Messenger::GetBlockLink(
            vector<unsigned char>(blockString.begin(), blockString.end()), 0,
            blcklink)) {
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

bool BlockStorage::PutMetadata(MetaType type,
                               const std::vector<unsigned char>& data) {
  LOG_MARKER();
  int ret = m_metadataDB->Insert(std::to_string((int)type), data);
  return (ret == 0);
}

bool BlockStorage::GetMetadata(MetaType type,
                               std::vector<unsigned char>& data) {
  LOG_MARKER();
  string metaString = m_metadataDB->Lookup(std::to_string((int)type));

  if (metaString.empty()) {
    LOG_GENERAL(INFO, "No metadata get")
    return false;
  }

  data = std::vector<unsigned char>(metaString.begin(), metaString.end());

  return true;
}

bool BlockStorage::PutDSCommittee(
    const shared_ptr<deque<pair<PubKey, Peer>>>& dsCommittee,
    const uint16_t& consensusLeaderID) {
  LOG_MARKER();
  m_dsCommitteeDB->ResetDB();
  unsigned int index = 0;
  string leaderId = to_string(consensusLeaderID);

  if (0 !=
      m_dsCommitteeDB->Insert(
          index++, vector<unsigned char>(leaderId.begin(), leaderId.end()))) {
    LOG_GENERAL(WARNING, "Failed to store DS leader ID:" << consensusLeaderID);
    return false;
  }

  LOG_GENERAL(INFO, "Stored DS leader ID:" << consensusLeaderID);

  vector<unsigned char> data;

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
        PubKey(vector<unsigned char>(dataStr.begin(),
                                     dataStr.begin() + PUB_KEY_SIZE),
               0),
        Peer(vector<unsigned char>(dataStr.begin() + PUB_KEY_SIZE,
                                   dataStr.end()),
             0));
    LOG_GENERAL(INFO, "Retrieved DS committee: " << dsCommittee->back().first
                                                 << ", "
                                                 << dsCommittee->back().second);
  }

  return true;
}

bool BlockStorage::PutShardStructure(const DequeOfShard& shards) {
  LOG_MARKER();

  m_shardStructureDB->ResetDB();
  unsigned int index = 0;
  vector<unsigned char> shardStructure;

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

  unsigned int index = 0;
  string dataStr = m_shardStructureDB->Lookup(index++);
  Messenger::ArrayToShardStructure(
      vector<unsigned char>(dataStr.begin(), dataStr.end()), 0, shards);
  LOG_GENERAL(INFO, "Retrieved sharding structure");
  return true;
}

bool BlockStorage::PutStateDelta(const uint64_t& finalBlockNum,
                                 const std::vector<unsigned char>& stateDelta) {
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
                                 std::vector<unsigned char>& stateDelta) {
  LOG_MARKER();

  string dataStr = m_stateDeltaDB->Lookup(finalBlockNum);
  stateDelta = vector<unsigned char>(dataStr.begin(), dataStr.end());
  LOG_PAYLOAD(INFO, "Retrieved state delta of final block " << finalBlockNum,
              stateDelta, Logger::MAX_BYTES_TO_DISPLAY);
  return true;
}

bool BlockStorage::ResetDB(DBTYPE type) {
  bool ret = false;
  switch (type) {
    case META:
      ret = m_metadataDB->ResetDB();
      break;
    case DS_BLOCK:
      ret = m_dsBlockchainDB->ResetDB();
      break;
    case TX_BLOCK:
      ret = m_txBlockchainDB->ResetDB();
      break;
    case TX_BODY:
      ret = m_txBodyDB->ResetDB();
      break;
    case TX_BODY_TMP:
      ret = m_txBodyTmpDB->ResetDB();
      break;
    case MICROBLOCK:
      ret = m_microBlockDB->ResetDB();
      break;
    case DS_COMMITTEE:
      ret = m_dsCommitteeDB->ResetDB();
      break;
    case VC_BLOCK:
      ret = m_VCBlockDB->ResetDB();
      break;
    case FB_BLOCK:
      ret = m_fallbackBlockDB->ResetDB();
      break;
    case BLOCKLINK:
      ret = m_blockLinkDB->ResetDB();
      break;
    case SHARD_STRUCTURE:
      ret = m_shardStructureDB->ResetDB();
      break;
    case STATE_DELTA:
      ret = m_stateDeltaDB->ResetDB();
      break;
  }
  if (!ret) {
    LOG_GENERAL(INFO, "FAIL: Reset DB " << type << " failed");
  }
  return ret;
}

std::vector<std::string> BlockStorage::GetDBName(DBTYPE type) {
  std::vector<std::string> ret;
  switch (type) {
    case META:
      ret.push_back(m_metadataDB->GetDBName());
      break;
    case DS_BLOCK:
      ret.push_back(m_dsBlockchainDB->GetDBName());
      break;
    case TX_BLOCK:
      ret.push_back(m_txBlockchainDB->GetDBName());
      break;
    case TX_BODY:
      ret.push_back(m_txBodyDB->GetDBName());
      break;
    case TX_BODY_TMP:
      ret.push_back(m_txBodyTmpDB->GetDBName());
      break;
    case MICROBLOCK:
      ret.push_back(m_microBlockDB->GetDBName());
      break;
    case DS_COMMITTEE:
      ret.push_back(m_dsCommitteeDB->GetDBName());
      break;
    case VC_BLOCK:
      ret.push_back(m_VCBlockDB->GetDBName());
      break;
    case FB_BLOCK:
      ret.push_back(m_fallbackBlockDB->GetDBName());
      break;
    case BLOCKLINK:
      ret.push_back(m_blockLinkDB->GetDBName());
      break;
    case SHARD_STRUCTURE:
      ret.push_back(m_shardStructureDB->GetDBName());
      break;
    case STATE_DELTA:
      ret.push_back(m_stateDeltaDB->GetDBName());
      break;
  }

  return ret;
}

bool BlockStorage::ResetAll() {
  if (!LOOKUP_NODE_MODE) {
    return ResetDB(META) && ResetDB(DS_BLOCK) && ResetDB(TX_BLOCK) &&
           ResetDB(MICROBLOCK) && ResetDB(DS_COMMITTEE) && ResetDB(VC_BLOCK) &&
           ResetDB(FB_BLOCK) && ResetDB(BLOCKLINK) &&
           ResetDB(SHARD_STRUCTURE) && ResetDB(STATE_DELTA);
  } else  // IS_LOOKUP_NODE
  {
    return ResetDB(META) && ResetDB(DS_BLOCK) && ResetDB(TX_BLOCK) &&
           ResetDB(TX_BODY) && ResetDB(TX_BODY_TMP) && ResetDB(MICROBLOCK) &&
           ResetDB(DS_COMMITTEE) && ResetDB(VC_BLOCK) && ResetDB(FB_BLOCK) &&
           ResetDB(BLOCKLINK) && ResetDB(SHARD_STRUCTURE) &&
           ResetDB(STATE_DELTA);
  }
}
