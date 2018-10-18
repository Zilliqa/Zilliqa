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

#include "Retriever.h"

#include <stdlib.h>
#include <algorithm>
#include <deque>
#include <exception>
#include <vector>

#include <boost/filesystem.hpp>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"

using namespace boost::filesystem;
namespace filesys = boost::filesystem;
using namespace std;

Retriever::Retriever(Mediator& mediator) : m_mediator(mediator) {}

void Retriever::RetrieveDSBlocks(bool& result) {
  LOG_MARKER();

  std::list<DSBlockSharedPtr> blocks;
  if (!BlockStorage::GetBlockStorage().GetAllDSBlocks(blocks)) {
    LOG_GENERAL(WARNING, "RetrieveDSBlocks skipped or incompleted");
    result = false;
    return;
  }

  blocks.sort([](const DSBlockSharedPtr& a, const DSBlockSharedPtr& b) {
    return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
  });

  if (!blocks.empty()) {
    if (m_mediator.m_ds->m_latestActiveDSBlockNum == 0) {
      std::vector<unsigned char> latestActiveDSBlockNumVec;
      if (!BlockStorage::GetBlockStorage().GetMetadata(
              MetaType::LATESTACTIVEDSBLOCKNUM, latestActiveDSBlockNumVec)) {
        LOG_GENERAL(WARNING, "Get LatestActiveDSBlockNum failed");
        result = false;
        return;
      }
      m_mediator.m_ds->m_latestActiveDSBlockNum = std::stoull(
          DataConversion::CharArrayToString(latestActiveDSBlockNumVec));
    }
  }

  /// Check whether the termination of last running happens before the last
  /// DSEpoch properly ended.
  std::vector<unsigned char> isDSIncompleted;
  if (!BlockStorage::GetBlockStorage().GetMetadata(MetaType::DSINCOMPLETED,
                                                   isDSIncompleted)) {
    LOG_GENERAL(WARNING, "No GetMetadata or failed");
    result = false;
    return;
  }

  if (isDSIncompleted[0] == '1') {
    LOG_GENERAL(INFO, "Has incompleted DS Block");
    if (BlockStorage::GetBlockStorage().DeleteDSBlock(blocks.size() - 1)) {
      BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                  {'0'});
    }
    blocks.pop_back();
    hasIncompletedDS = true;
  }

  for (const auto& block : blocks) {
    m_mediator.m_dsBlockChain.AddBlock(*block);
  }

  result = true;
}

void Retriever::RetrieveDirectoryBlocks(
    bool& result, deque<pair<PubKey, Peer>>& initialDSComm) {
  result = true;

  list<BlockLink> blocklinks;

  if (!BlockStorage::GetBlockStorage().GetAllBlockLink(blocklinks)) {
    LOG_GENERAL(WARNING, "RetrieveDirectoryBlocks skipped or incompleted");
    result = false;
    return;
  }

  blocklinks.sort([](const BlockLink& a, const BlockLink& b) {
    return get<BlockLinkIndex::INDEX>(a) < get<BlockLinkIndex::INDEX>(b);
  });

  vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>
      dirBlocks;

  deque<pair<PubKey, Peer>> dsComm;

  if (BlockStorage::GetBlockStorage().GetInitialDSCommittee(dsComm)) {
    LOG_GENERAL(WARNING, "Could not get initial ds committee");
    result = false;
    return;
  }
  // initialDSComm must be empty here
  initialDSComm = dsComm;

  if (initialDSComm.size() == 0) {
    LOG_GENERAL(WARNING, "initial DS comm empty, cannot verify");
    result = false;
    return;
  }

  if (!blocklinks.empty()) {
    if (m_mediator.m_ds->m_latestActiveDSBlockNum == 0) {
      std::vector<unsigned char> latestActiveDSBlockNumVec;
      if (!BlockStorage::GetBlockStorage().GetMetadata(
              MetaType::LATESTACTIVEDSBLOCKNUM, latestActiveDSBlockNumVec)) {
        LOG_GENERAL(WARNING, "Get LatestActiveDSBlockNum failed");
        result = false;
        return;
      }
      m_mediator.m_ds->m_latestActiveDSBlockNum = std::stoull(
          DataConversion::CharArrayToString(latestActiveDSBlockNumVec));
    }
  }

  for (const auto& b : blocklinks) {
    if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::DS) {
      DSBlockSharedPtr dsblock;
      if (!BlockStorage::GetBlockStorage().GetDSBlock(
              get<BlockLinkIndex::DSINDEX>(b), dsblock)) {
        LOG_GENERAL(WARNING, "could not get ds block "
                                 << get<BlockLinkIndex::DSINDEX>(b));
        result = false;
        continue;
      }
      dirBlocks.emplace_back(*dsblock);
    } else if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::VC) {
      VCBlockSharedPtr vcblockptr;
      if (!BlockStorage::GetBlockStorage().GetVCBlock(
              get<BlockLinkIndex::BLOCKHASH>(b), vcblockptr)) {
        LOG_GENERAL(WARNING, "could not get vc block "
                                 << get<BlockLinkIndex::BLOCKHASH>(b));
        result = false;
        continue;
      }
      dirBlocks.emplace_back(*vcblockptr);
    } else if (get<BlockLinkIndex::BLOCKTYPE>(b) == BlockType::FB) {
      FallbackBlockSharedPtr fallbackwsharding;
      if (!BlockStorage::GetBlockStorage().GetFallbackBlock(
              get<BlockLinkIndex::BLOCKHASH>(b), fallbackwsharding)) {
        LOG_GENERAL(WARNING, "could not get fb block "
                                 << get<BlockLinkIndex::BLOCKHASH>(b));
        result = false;
        continue;
      }
      dirBlocks.emplace_back(*fallbackwsharding);
    }
  }

  uint64_t lastDsind = get<BlockLinkIndex::DSINDEX>(blocklinks.back());

  if (get<BlockLinkIndex::BLOCKTYPE>(blocklinks.back()) == BlockType::DS) {
    lastDsind--;
  }

  if (lastDsind == 0) {
    LOG_GENERAL(INFO, "");
    result = false;
    return;
  }

  /// Check whether the termination of last running happens before the last
  /// DSEpoch properly ended.
  std::vector<unsigned char> isDSIncompleted;
  if (!BlockStorage::GetBlockStorage().GetMetadata(MetaType::DSINCOMPLETED,
                                                   isDSIncompleted)) {
    LOG_GENERAL(WARNING, "No GetMetadata or failed");
    result = false;
    return;
  }

  if (isDSIncompleted[0] == '1') {
    LOG_GENERAL(INFO, "Has incompleted DS Block");
    if (BlockStorage::GetBlockStorage().DeleteDSBlock(lastDsind)) {
      BlockStorage::GetBlockStorage().PutMetadata(MetaType::DSINCOMPLETED,
                                                  {'0'});
    }
    hasIncompletedDS = true;
  }

  LOG_GENERAL(INFO, "Resetting DBs");

  vector<BlockStorage::DBTYPE> dbs = {
      BlockStorage::DBTYPE::DS_BLOCK, BlockStorage::DBTYPE::VC_BLOCK,
      BlockStorage::DBTYPE::FB_BLOCK, BlockStorage::DBTYPE::BLOCKLINK};

  for (const auto& db : dbs) {
    if (!BlockStorage::GetBlockStorage().ResetDB(db)) {
      LOG_GENERAL(WARNING, "Could not reset db  " << db);
      continue;
    }
  }

  m_mediator.m_validator->CheckDirBlocks(dirBlocks, initialDSComm, 0, dsComm);

  //[ToDo]check built with given ds comm
}

void Retriever::RetrieveTxBlocks(bool& result) {
  LOG_MARKER();
  std::list<TxBlockSharedPtr> blocks;
  if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(blocks)) {
    LOG_GENERAL(WARNING, "RetrieveTxBlocks skipped or incompleted");
    result = false;
    return;
  }

  blocks.sort([](const TxBlockSharedPtr& a, const TxBlockSharedPtr& b) {
    return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
  });

  // truncate the extra final blocks at last
  int totalSize = blocks.size();
  int extra_txblocks = totalSize % NUM_FINAL_BLOCK_PER_POW;
  for (int i = 0; i < extra_txblocks; ++i) {
    BlockStorage::GetBlockStorage().DeleteTxBlock(totalSize - 1 - i);
    blocks.pop_back();
  }

  for (const auto& block : blocks) {
    m_mediator.m_node->AddBlock(*block);
  }

  result = true;
}

bool Retriever::CleanExtraTxBodies() {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Retriever::CleanExtraTxBodies not expected to be called "
                "from other than LookUp node.");
    return true;
  }

  LOG_MARKER();
  std::list<TxnHash> txnHashes;
  if (BlockStorage::GetBlockStorage().GetAllTxBodiesTmp(txnHashes)) {
    for (auto i : txnHashes) {
      if (!BlockStorage::GetBlockStorage().DeleteTxBody(i)) {
        LOG_GENERAL(WARNING, "FAIL: To delete TxHash in TxBodiesTmpDB");
        // return false;
      }
    }
  }
  return BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TX_BODY_TMP);
}

bool Retriever::RetrieveStates() {
  LOG_MARKER();
  return AccountStore::GetInstance().RetrieveFromDisk();
}

bool Retriever::ValidateStates() {
  LOG_MARKER();

  if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetStateRootHash() ==
      AccountStore::GetInstance().GetStateRootHash()) {
    LOG_GENERAL(INFO, "ValidateStates passed.");
    AccountStore::GetInstance().RepopulateStateTrie();
    return true;
  } else {
    LOG_GENERAL(WARNING, "ValidateStates failed.");
    LOG_GENERAL(INFO, "StateRoot in FinalBlock(BlockNum: "
                          << m_mediator.m_txBlockChain.GetLastBlock()
                                 .GetHeader()
                                 .GetBlockNum()
                          << "): "
                          << m_mediator.m_txBlockChain.GetLastBlock()
                                 .GetHeader()
                                 .GetStateRootHash()
                          << '\n'
                          << "Retrieved StateRoot: "
                          << AccountStore::GetInstance().GetStateRootHash());
    return false;
  }
}

void Retriever::CleanAll() {
  if (BlockStorage::GetBlockStorage().ResetAll()) {
    LOG_GENERAL(INFO, "Reset DB Succeed");
  } else {
    LOG_GENERAL(WARNING, "FAIL: Reset DB Failed");
  }
}
