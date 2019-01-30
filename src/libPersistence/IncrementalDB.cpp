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

#include "IncrementalDB.h"
#include <list>
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libMessage/Messenger.h"

using namespace std;

void IncrementalDB::ChangeDBPointer(const uint64_t& currentDSEpoch,
                                    const string& dbName) {
  if (m_DBPointer.at(dbName).first != currentDSEpoch) {
    m_DBPointer.at(dbName).first = currentDSEpoch;
    m_DBPointer.at(dbName).second =
        make_shared<LevelDB>(dbName, m_path, to_string(currentDSEpoch));
  }
}

bool IncrementalDB::PutTxBody(const dev::h256& txID, const bytes& body,
                              const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_txBodyDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_txBodyDBName).second->Insert(txID, body);

  return (ret == 0);
}

bool IncrementalDB::PutMicroBlock(const BlockHash& blockHash, const bytes& body,
                                  const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_microBlockDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_microBlockDBName).second->Insert(blockHash, body);

  return (ret == 0);
}

bool IncrementalDB::PutTxBlock(const uint64_t& blockNum, const bytes& body,
                               const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_TxBlockDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_TxBlockDBName).second->Insert(blockNum, body);

  return (ret == 0);
}

bool IncrementalDB::PutDSBlock(const uint64_t& blockNum, const bytes& body,
                               const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_DSBlockDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_DSBlockDBName).second->Insert(blockNum, body);

  return (ret == 0);
}

bool IncrementalDB::PutFallbackBlock(const BlockHash& blockHash,
                                     const bytes& body,
                                     const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_FallbackBlockDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_FallbackBlockDBName).second->Insert(blockHash, body);

  return (ret == 0);
}

bool IncrementalDB::PutVCBlock(const BlockHash& blockHash, const bytes& body,
                               const uint64_t& dsEpoch) {
  ChangeDBPointer(dsEpoch, m_VCBlockDBName);

  int ret = -1;

  ret = m_DBPointer.at(m_VCBlockDBName).second->Insert(blockHash, body);

  return (ret == 0);
}

bool IncrementalDB::PutBlockLink(const uint64_t& index, const bytes& body) {
  int ret = -1;
  ret = m_blockLinkDB->Insert(index, body);
  return (ret == 0);
}

void IncrementalDB::Init() {
  const std::string path_abs = "./" + m_path;
  if (!boost::filesystem::exists(path_abs)) {
    boost::filesystem::create_directories(path_abs);
  }

  string emptyString;
  emptyString.clear();
  m_blockLinkDB = make_shared<LevelDB>(m_blockLinkDBName, m_path, emptyString);

  for (auto const& dbName :
       {m_txBodyDBName, m_microBlockDBName, m_VCBlockDBName, m_DSBlockDBName,
        m_FallbackBlockDBName, m_TxBlockDBName}) {
    m_DBPointer.emplace(
        dbName,
        make_pair(0, make_shared<LevelDB>(dbName, m_path, (string) "0")));
  }
}

bool IncrementalDB::GetAllBlockLink(list<BlockLink>& blocklinks) {
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
bool IncrementalDB::GetLatestDSEpochStorage(uint64_t& lastDSEpoch) {
  lastDSEpoch = 0;
  uint64_t temp = -1;
  const string path_abs = "./" + m_path;

  for (boost::filesystem::directory_iterator itr(path_abs);
       itr != boost::filesystem::directory_iterator(); ++itr) {
    if (boost::filesystem::is_directory(itr->status())) {
      try {
        temp = boost::lexical_cast<uint64_t>(itr->path().filename());
        if (temp > lastDSEpoch) {
          lastDSEpoch = temp;
        }
      } catch (exception& e) {
        LOG_GENERAL(INFO, itr->path().filename()
                              << " not a number " << e.what());
      }
    }
  }

  if ((uint64_t)-1 ==
      temp)  // Means that temp did not set a valid value at some point
  {
    LOG_GENERAL(WARNING, "Failed to get Latest Epoch");
    return false;
  }

  return true;
}

bool IncrementalDB::GetAllTxBlocksEpoch(std::list<TxBlock>& blocks,
                                        const uint64_t& dsEpoch) {
  LOG_MARKER();

  ChangeDBPointer(dsEpoch, m_TxBlockDBName);

  leveldb::Iterator* it = m_DBPointer.at(m_TxBlockDBName)
                              .second->GetDB()
                              ->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    string bns = it->key().ToString();
    string blockString = it->value().ToString();
    if (blockString.empty()) {
      LOG_GENERAL(WARNING, "Lost one block in the chain");
      delete it;
      return false;
    }
    blocks.emplace_back(
        TxBlock(bytes(blockString.begin(), blockString.end()), 0));
  }

  delete it;

  if (blocks.empty()) {
    LOG_GENERAL(INFO, "Disk has no TxBlock");
    return false;
  }

  return true;
}

bool IncrementalDB::GetDSBlock(uint64_t& blocknum, DSBlockSharedPtr& block) {
  ChangeDBPointer(blocknum, m_DSBlockDBName);
  string blockString = m_DBPointer.at(m_DSBlockDBName).second->Lookup(blocknum);

  if (blockString.empty()) {
    return false;
  }

  block = DSBlockSharedPtr(
      new DSBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool IncrementalDB::GetVCBlock(uint64_t& dsEpochNum, BlockHash& blockhash,
                               VCBlockSharedPtr& block) {
  ChangeDBPointer(dsEpochNum, m_VCBlockDBName);
  string blockString =
      m_DBPointer.at(m_VCBlockDBName).second->Lookup(blockhash);

  if (blockString.empty()) {
    return false;
  }
  block = VCBlockSharedPtr(
      new VCBlock(bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

bool IncrementalDB::GetFallbackBlock(
    uint64_t& dsEpochNum, BlockHash& blockhash,
    FallbackBlockSharedPtr& fallbackblockwsharding) {
  ChangeDBPointer(dsEpochNum, m_FallbackBlockDBName);
  string blockString =
      m_DBPointer.at(m_FallbackBlockDBName).second->Lookup(blockhash);

  if (blockString.empty()) {
    return false;
  }

  fallbackblockwsharding =
      FallbackBlockSharedPtr(new FallbackBlockWShardingStructure(
          bytes(blockString.begin(), blockString.end()), 0));

  return true;
}

/*bool IncrementalDB::VerifyAll()
{

  DequeOfNode dsComm;

  for (const auto& dsKey : *m_mediator.m_initialDSCommittee) {
    dsComm.emplace_back(dsKey, Peer());
  }

  list<BlockLink> blocklinks;

  if(!GetAllBlockLink(blocklinks) || blocklinks.empty())
  {
    LOG_GENERAL(WARNING,"Failed to get all blocklinks");
    return false;
  }

   blocklinks.sort([](const BlockLink& a, const BlockLink& b) {
    return std::get<BlockLinkIndex::INDEX>(a) <
           std::get<BlockLinkIndex::INDEX>(b);
  });

  uint64_t latestDSEpoch = 0;
  if(!GetLatestDSEpochStorage(latestDSEpoch))
  {
    return false;
  }

  list<TxBlock> txblocks;
  for(uint64_t i = latestDSEpoch ; i>=0 ; i--)
  {
    txblocks.clear();
    if(GetAllTxBlocksEpoch(txblocks, i))
    {
      LOG_GENERAL(INFO, "Succeeded to get finalblocks of epoch "<<i);
      latestDSEpoch = i;
      break;
    }

  }

  if(txblocks.empty())
  {
    LOG_GENERAL(WARNING,"Could not find any tx block");
    return false;
  }

  txblocks.sort([](const TxBlock& a, const TxBlock& b)
  {
    return a.GetHeader().GetBlockNum() < b.GetHeader().GetBlockNum();
  }

    );

  const auto& latestTxBlockNum = txblocks.back()->GetHeader().GetBlockNum();


  vector<boost::variant<DSBlock, VCBlock,
                                FallbackBlockWShardingStructure>>
directoryBlocks;

  for(const auto& blocklink : blocklinks)
  {
    uint64_t& dsEpoch = get<BlockLinkIndex::DSINDEX>(blocklink);
    BlockHash& blockhash = get<BlockLinkIndex::BLOCKHASH>(blockLink);

    if(get<BlockLinkIndex::TYPE>(blocklink) == BlockType::DS)
    {
      DSBlockSharedPtr dsblock
      if(!GetDSBlock(dsEpoch, dsblock))
      {
        LOG_GENERAL(WARNING, "Could not get ds blocknum "<<dsEpoch);
        return false;
      }
      if (latestTxBlockNum <= dsblock->GetHeader().GetEpochNum()) {
        LOG_GENERAL(INFO, "Break off at "
                              << latestTxBlockNum << " " << latestDSIndex << " "
                              << dsblock->GetHeader().GetBlockNum() << " "
                              << dsblock->GetHeader().GetEpochNum());
        break;
      }
      directoryBlocks.emplace_back(*dsblock);
    }
    else if(get<BlockLinkIndex::TYPE>(blocklink) == BlockType::VC)
    {
      VCBlockSharedPtr vcblock;

      if(!GetVCBlock(dsEpoch, blockhash, vcblock))
      {
        LOG_GENERAL(WARNING, "Could not get vc blockhash "<<blockhash<<"
"<<dsEpoch); return false;
      }
      directoryBlocks.emplace_back(*vcblock);
    }
    else if(get<BlockLinkIndex::TYPE>(blocklink) == BlockType::FB)
    {
      FallbackBlockSharedPtr fallbackblockwsharding;
      if(!GetFallbackBlock(dsEpoch, blockhash, fallbackblockwsharding))
      {
        LOG_GENERAL(WARNING, "Could not get fallback blockhash "<<blockhash<<"
"<<dsEpoch); return false;
      }
      directoryBlocks.emplace_back(*fallbackblockwsharding);
    }
  }

  if (!m_mediator.m_validator->CheckDirBlocks(diectoryBlocks, dsComm, 0,
dsComm)) { LOG_GENERAL(WARNING, "Failed to verify Dir Blocks"); return false;
  }

  if (m_mediator.m_validator->CheckTxBlocks(
          txblocks, dsComm, m_mediator.m_blocklinkchain.GetLatestBlockLink()) !=
      ValidatorBase::TxBlockValidationMsg::VALID) {
    LOG_GENERAL(WARNING, "Failed to verify TxBlocks");
    return false;
  }

  //If this check passes means that the latest block is valid, hence can just
use previous hash to verify others if(latestDSEpoch <= 1)
  {
    LOG_GENERAL(INFO, "All tx blocks verified");
    return true;
  }
  BlockHash prevHash = txblocks.at(0).GetHeader().GetPrevHash();
  vector<TxBlocks> prevTxBlocks;
  for(uint64_t i = latestDSEpoch-1; i>=0; i++)
  {
    if(!GetAllTxBlocksEpoch(prevTxBlocks,i))
    {
      LOG_GENERAL(WARNING, "Failed to get blocks for "<<i);
      return false;
    }

  }

}*/
