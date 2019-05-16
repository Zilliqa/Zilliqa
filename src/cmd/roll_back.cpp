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

/// Should be run from a folder with dsnodes.xml and constants.xml and a folder
/// named "persistence" consisting of the persistence

#include "libMediator/Mediator.h"
#include "libNetwork/Guard.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/Retriever.h"
#include "libUtils/UpgradeManager.h"

using namespace std;

constexpr int SUCCESSFUL = 0;
constexpr int EPOCH_INVALID = -1;
constexpr int PERSISTENCE_ERROR = -2;

bool RollBackDSComm(const BlockLink& lastBlockLink,
                    const DSBlock& latestDSBlock,
                    const DequeOfNode& dsCommittee,
                    DequeOfNode& dsCommittee_rolled_back) {
  PairOfNode dsLeader;

  if (!Node::GetDSLeader(lastBlockLink, latestDSBlock, dsCommittee, dsLeader)) {
    return false;
  }
  shared_ptr<DequeOfNode> dsCommittee_curr;
  uint16_t leaderID_curr{};
  if (!BlockStorage::GetBlockStorage().GetDSCommittee(dsCommittee_curr,
                                                      leaderID_curr)) {
    return false;
  }
  const auto& size = dsCommittee_curr->size();
  for (uint16_t i = 0; i < size; i++) {
    if (dsCommittee_curr->at(i).first == dsCommittee.at(i).first) {
      dsCommittee_rolled_back.at(i).second = dsCommittee_curr->at(i).second;
    } else {
      dsCommittee_rolled_back.at(i).second = Peer(-1, 0);
      // Put invalid IP and port
    }
  }
  return true;
}

int main(int argc, char* argv[]) {
  PairOfKey key;  // Dummy to initate mediator
  Peer peer;

  if (argc != 2) {
    cout << "Please give argument as the epoch number" << endl;
    return EPOCH_INVALID;
  }
  uint64_t epoch{};
  try {
    epoch = stoull(argv[1]);
  } catch (const exception& e) {
    cout << "Could not convert epochnum " << e.what() << endl;
    return EPOCH_INVALID;
  }

  Mediator mediator(key, peer);
  Node node(mediator, 0, false);
  shared_ptr<Validator> vd;
  vd = make_shared<Validator>(mediator);
  Synchronizer sync;

  mediator.m_dsBlockChain.Reset();
  mediator.m_txBlockChain.Reset();

  sync.InitializeGenesisBlocks(mediator.m_dsBlockChain,
                               mediator.m_txBlockChain);
  const auto& dsBlock = mediator.m_dsBlockChain.GetBlock(0);
  // cout << dsBlock.GetHeader().GetBlockNum() << endl;
  {
    lock_guard<mutex> lock(mediator.m_mutexInitialDSCommittee);
    if (!UpgradeManager::GetInstance().LoadInitialDS(
            *mediator.m_initialDSCommittee)) {
      LOG_GENERAL(WARNING, "Unable to load initial DS comm");
    }
  }
  {
    DequeOfNode buildDSComm;
    lock_guard<mutex> lock(mediator.m_mutexInitialDSCommittee);
    if (mediator.m_initialDSCommittee->size() != 0) {
      for (const auto& initDSCommKey : *mediator.m_initialDSCommittee) {
        buildDSComm.emplace_back(initDSCommKey, Peer());
        // Set initial ds committee with null peer
      }
    } else {
      LOG_GENERAL(WARNING, "Initial DS comm size 0 ");
    }

    mediator.m_blocklinkchain.SetBuiltDSComm(buildDSComm);
  }
  mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                         dsBlock.GetBlockHash());

  if (GUARD_MODE) {
    Guard::GetInstance().Init();
  }
  mediator.RegisterColleagues(nullptr, &node, nullptr, vd.get());

  list<BlockLink> blocklinks;

  if (!BlockStorage::GetBlockStorage().GetAllBlockLink(blocklinks)) {
    cout << "Failed to get blocklinks " << endl;
    return PERSISTENCE_ERROR;
  }

  blocklinks.sort([](const BlockLink& a, const BlockLink& b) {
    return std::get<BlockLinkIndex::INDEX>(a) <
           std::get<BlockLinkIndex::INDEX>(b);
  });

  std::list<TxBlockSharedPtr> txblocks;
  if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(txblocks)) {
    cout << "Failed to get TxBlocks" << endl;
    return PERSISTENCE_ERROR;
  }

  txblocks.sort([](const TxBlockSharedPtr& a, const TxBlockSharedPtr& b) {
    return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
  });

  const auto latestTxBlockNum = txblocks.back()->GetHeader().GetBlockNum();

  txblocks.clear();  // no longer need the vector

  if (latestTxBlockNum < epoch) {
    cout << "epoch is not still reached " << endl;
    return EPOCH_INVALID;
  }

  if (!BlockStorage::GetBlockStorage().ResetDB(
          BlockStorage::DBTYPE::BLOCKLINK)) {
    cout << "Failed to reset BlockLinkDB" << endl;
    return PERSISTENCE_ERROR;
  }
  TxBlockSharedPtr latestTxBlockPruned;
  if (!BlockStorage::GetBlockStorage().GetTxBlock(epoch, latestTxBlockPruned)) {
    cout << "Could not get epoch tx block " << epoch << endl;
    return PERSISTENCE_ERROR;
  }

  const auto& latestTxBlockNumPruned =
      latestTxBlockPruned->GetHeader().GetBlockNum();
  const auto& latestDSIndexPruned =
      latestTxBlockPruned->GetHeader().GetDSBlockNum();
  auto dsComm = mediator.m_blocklinkchain.GetBuiltDSComm();
  DSBlock lastDSblock{};
  for (const auto& blocklink : blocklinks) {
    const auto& currDSIndex = get<BlockLinkIndex::DSINDEX>(blocklink);
    const auto& type = get<BlockLinkIndex::BLOCKTYPE>(blocklink);
    const auto& blockHash = get<BlockLinkIndex::BLOCKHASH>(blocklink);
    if (currDSIndex == latestDSIndexPruned &&
        get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::VC) {
      // check if vc happen afterwards or before
      VCBlockSharedPtr vcblock;
      if (!BlockStorage::GetBlockStorage().GetVCBlock(blockHash, vcblock)) {
        return PERSISTENCE_ERROR;
      }
      if (latestTxBlockNumPruned <=
          vcblock->GetHeader().GetViewChangeEpochNo()) {
        if (!BlockStorage::GetBlockStorage().DeleteVCBlock(blockHash)) {
          cout << "Failed to delete VC blocks " << blockHash << endl;
          return PERSISTENCE_ERROR;
        }
      }
      continue;
    } else if (currDSIndex <= latestDSIndexPruned) {
      DSBlockSharedPtr dsblock;
      if (type == BlockType::DS) {
        if (!BlockStorage::GetBlockStorage().GetDSBlock(currDSIndex, dsblock)) {
          cout << "Failed to get DS Block " << endl;
          return PERSISTENCE_ERROR;
        }
        lastDSblock = *dsblock;
        mediator.m_node->UpdateDSCommiteeComposition(dsComm, *dsblock);
        //[TODO] Add condition for VC
      }
      mediator.m_blocklinkchain.AddBlockLink(
          std::get<BlockLinkIndex::INDEX>(blocklink),
          std::get<BlockLinkIndex::DSINDEX>(blocklink),
          std::get<BlockLinkIndex::BLOCKTYPE>(blocklink),
          std::get<BlockLinkIndex::BLOCKHASH>(blocklink));
      continue;
    }

    if (type == BlockType::DS) {
      if (!BlockStorage::GetBlockStorage().DeleteDSBlock(currDSIndex)) {
        cout << "Failed to delete DS block " << currDSIndex << endl;
        return PERSISTENCE_ERROR;
      }
    } else if (type == BlockType::VC) {
      if (!BlockStorage::GetBlockStorage().DeleteVCBlock(blockHash)) {
        cout << "Failed to delete VC blocks " << blockHash << endl;
        return PERSISTENCE_ERROR;
      }
    }
  }

  for (uint64_t i = latestTxBlockNumPruned + 1; i <= latestTxBlockNum; i++) {
    TxBlockSharedPtr currTxBlock;
    if (!BlockStorage::GetBlockStorage().GetTxBlock(i, currTxBlock)) {
      cout << "Could not get tx block " << i << endl;
      return PERSISTENCE_ERROR;
    }
    if (!BlockStorage::GetBlockStorage().DeleteTxBlock(i)) {
      cout << "Failed to delete tx block " << i << endl;
      return PERSISTENCE_ERROR;
    }
    if (!BlockStorage::GetBlockStorage().DeleteStateDelta(i)) {
      cout << "Failed to delete State delta " << i << endl;
      return PERSISTENCE_ERROR;
    }
    const auto& microblockInfos = currTxBlock->GetMicroBlockInfos();
    for (const auto& mbInfo : microblockInfos) {
      if (mbInfo.m_txnRootHash == TxnHash()) {
        continue;
      }
      MicroBlockSharedPtr mbptr;
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(
              mbInfo.m_microBlockHash, mbptr)) {
        cout << "Could not get MicroBlock " << mbInfo.m_microBlockHash << endl;
        return PERSISTENCE_ERROR;
      }
      if (!BlockStorage::GetBlockStorage().DeleteMicroBlock(
              mbInfo.m_microBlockHash)) {
        cout << "Could not delete MicroBlock " << mbInfo.m_microBlockHash
             << endl;
        return PERSISTENCE_ERROR;
      }
      for (const auto& tranHash : mbptr->GetTranHashes()) {
        if (!BlockStorage::GetBlockStorage().DeleteTxBody(tranHash)) {
          cout << "Could not delete transaction hash " << tranHash << endl;
          return PERSISTENCE_ERROR;
        }
      }
    }
  }

  Retriever ret(mediator);
  // To construct base state
  ret.RetrieveTxBlocks(true);
  auto dsCommittee_rolled_back = dsComm;
  if (!RollBackDSComm(mediator.m_blocklinkchain.GetLatestBlockLink(),
                      lastDSblock, dsComm, dsCommittee_rolled_back)) {
    cout << "Could not roll back ds comm" << endl;
    return PERSISTENCE_ERROR;
  }
}
