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
#include "libUtils/FileSystem.h"
#include "libUtils/UpgradeManager.h"

using namespace std;

constexpr int EPOCH_INVALID = -1;
constexpr int PERSISTENCE_ERROR = -2;

bool RollBackDSComm(const BlockLink& lastBlockLink,
                    const DSBlock& latestDSBlock,
                    const DequeOfNode& dsCommittee,
                    const map<PubKey, Peer>& IPMapping,
                    DequeOfNode& dsCommittee_rolled_back) {
  PairOfNode dsLeader;

  if (!Node::GetDSLeader(lastBlockLink, latestDSBlock, dsCommittee, dsLeader)) {
    return false;
  }
  shared_ptr<DequeOfNode> dsCommittee_curr = make_shared<DequeOfNode>();
  uint16_t leaderID_curr{};
  if (!BlockStorage::GetBlockStorage().GetDSCommittee(dsCommittee_curr,
                                                      leaderID_curr)) {
    return false;
  }
  const auto& size = dsCommittee_curr->size();
  for (uint16_t i = 0; i < size; i++) {
    if (dsLeader.first == dsCommittee.at(i).first) {
      cout << "Leader id " << i << endl;
      leaderID_curr = i;
    }
    if (dsCommittee_curr->at(i).first == dsCommittee.at(i).first) {
      dsCommittee_rolled_back.at(i).second = dsCommittee_curr->at(i).second;
    } else if (IPMapping.find(dsCommittee_rolled_back.at(i).first) !=
               IPMapping.end()) {
      dsCommittee_rolled_back.at(i).second =
          IPMapping.at(dsCommittee_rolled_back.at(i).first);
    } else {
      cout << "Could not find IP for " << dsCommittee_rolled_back.at(i).first
           << endl;
      return false;
    }
  }
  if (!BlockStorage::GetBlockStorage().PutDSCommittee(
          make_shared<DequeOfNode>(dsCommittee_rolled_back), leaderID_curr)) {
    cout << "Failed to put ds committee" << endl;
    return false;
  }
  return true;
}

bool PutStateDeltaInLocalPersistence(uint32_t lastBlockNum,
                                     const deque<TxBlockSharedPtr>& blocks) {
  if ((lastBlockNum + 1) %
          (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW) ==
      0) {
    // we must have latest state currently. so need not recreate states
    LOG_GENERAL(INFO,
                "Current state is up-to-date until txblk :" << lastBlockNum);
  } else {
    // create states from last INCRDB_DSNUMS_WITH_STATEDELTAS *
    // NUM_FINAL_BLOCK_PER_POW txn blocks
    unsigned int lower_bound_txnblk =
        ((lastBlockNum + 1) >
         INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)
            ? (((lastBlockNum + 1) /
                (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)) *
               (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW))
            : 0;
    unsigned int upper_bound_txnblk = lastBlockNum;

    LOG_GENERAL(INFO, "Will try recreating state from txnblks: "
                          << lower_bound_txnblk << " - " << upper_bound_txnblk);

    // clear all the state deltas from disk.
    if (!BlockStorage::GetBlockStorage().ResetDB(BlockStorage::STATE_DELTA)) {
      LOG_GENERAL(WARNING, "BlockStorage::ResetDB failed");
      return false;
    }

    std::string target = "persistence/stateDelta";
    unsigned int firstStateDeltaIndex = lower_bound_txnblk;
    for (unsigned int i = lower_bound_txnblk; i <= upper_bound_txnblk; i++) {
      // Check if StateDeltaFromS3/StateDelta_{i} exists and copy over to the
      // local persistence/stateDelta
      std::string source = "StateDeltaFromS3/stateDelta_" + std::to_string(i);
      if (boost::filesystem::exists(source)) {
        try {
          recursive_copy_dir(source, target);
        } catch (std::exception& e) {
          LOG_GENERAL(FATAL, "Failed to copy over stateDelta for TxBlk:" << i);
        }

        if ((i + 1) % NUM_FINAL_BLOCK_PER_POW == 0 ||
            i == upper_bound_txnblk) {  // state-delta from vacous epoch
          // refresh state-delta after copy over
          if (!BlockStorage::GetBlockStorage().RefreshDB(
                  BlockStorage::STATE_DELTA)) {
            LOG_GENERAL(WARNING, "BlockStorage::RefreshDB failed");
            return false;
          }

          // generate state now for NUM_FINAL_BLOCK_PER_POW statedeltas
          for (unsigned int j = firstStateDeltaIndex; j <= i; j++) {
            bytes stateDelta;
            LOG_GENERAL(
                INFO,
                "Try fetching statedelta and deserializing to state for txnBlk:"
                    << j);
            if (BlockStorage::GetBlockStorage().GetStateDelta(j, stateDelta)) {
              if (!AccountStore::GetInstance().DeserializeDelta(stateDelta,
                                                                0)) {
                LOG_GENERAL(
                    WARNING,
                    "AccountStore::GetInstance().DeserializeDelta failed");
                return false;
              }

              if (AccountStore::GetInstance().GetStateRootHash() !=
                  (*std::next(blocks.begin(), j))
                      ->GetHeader()
                      .GetStateRootHash()) {
                LOG_GENERAL(
                    WARNING,
                    "StateRoot in TxBlock(BlockNum: "
                        << j << ") : does not match retrieved stateroot hash");
                return false;
              }
            }
          }
          // commit the state to disk
          uint64_t initTrie;
          if (!AccountStore::GetInstance().MoveUpdatesToDisk(0, initTrie)) {
            LOG_GENERAL(WARNING, "AccountStore::MoveUpdatesToDisk failed");
            return false;
          }
          // clear the stateDelta db
          if (!BlockStorage::GetBlockStorage().ResetDB(
                  BlockStorage::STATE_DELTA)) {
            LOG_GENERAL(WARNING, "BlockStorage::ResetDB (STATE_DELTA) failed");
            return false;
          }
          firstStateDeltaIndex = i + 1;
        }
      } else  // we rely on next statedelta that covers this missing one
      {
        LOG_GENERAL(DEBUG, "Didn't find state-delta for TxnBlk:"
                               << i << ". This can happen. Not a problem!");
        // Do nothing
      }
    }
  }

  return true;
}

int main(int argc, char* argv[]) {
  PairOfKey key;  // Dummy to initate mediator
  Peer peer;
  map<PubKey, Peer> IPMapping;

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
  // cout << dsBlock.GetHeader().GetBlockNum() << endl;
  // AccountStore::GetInstance().Init();
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

  std::deque<TxBlockSharedPtr> txblocks;
  if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(txblocks)) {
    cout << "Failed to get TxBlocks" << endl;
    return PERSISTENCE_ERROR;
  }

  sort(txblocks.begin(), txblocks.end(),
       [](const TxBlockSharedPtr& a, const TxBlockSharedPtr& b) {
         return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
       });

  const auto latestTxBlockNum = txblocks.back()->GetHeader().GetBlockNum();
  const auto& latestDSIndex = txblocks.back()->GetHeader().GetDSBlockNum();
  cout << "latestTxBlockNum: " << latestTxBlockNum << endl;

  if (latestTxBlockNum < epoch) {
    cout << "epoch is not still reached " << endl;
    return EPOCH_INVALID;
  }

  if (!BlockStorage::GetBlockStorage().ResetDB(
          BlockStorage::DBTYPE::BLOCKLINK)) {
    cout << "Failed to reset BlockLinkDB" << endl;
    return PERSISTENCE_ERROR;
  }
  const auto& dsBlock = mediator.m_dsBlockChain.GetBlock(0);
  mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                         dsBlock.GetBlockHash());

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
        for (const auto& dswinner : dsblock->GetHeader().GetDSPoWWinners()) {
          IPMapping.emplace(dswinner.first, dswinner.second);
        }
        mediator.m_node->UpdateDSCommitteeComposition(dsComm, *dsblock);
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
  ret.RetrieveStates();
  if ((latestDSIndex == latestDSIndexPruned)) {
    ret.RetrieveTxBlocks();
  } else {
    PutStateDeltaInLocalPersistence(latestTxBlockNumPruned, txblocks);
  }
  auto dsCommittee_rolled_back = dsComm;
  if (!RollBackDSComm(mediator.m_blocklinkchain.GetLatestBlockLink(),
                      lastDSblock, dsComm, IPMapping,
                      dsCommittee_rolled_back)) {
    cout << "Could not roll back ds comm" << endl;
    return PERSISTENCE_ERROR;
  }
}
