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

#include "Retriever.h"

#include <stdlib.h>
#include <algorithm>
#include <exception>
#include <vector>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/FileSystem.h"

Retriever::Retriever(Mediator& mediator) : m_mediator(mediator) {}

bool Retriever::RetrieveTxBlocks(bool trimIncompletedBlocks) {
  LOG_MARKER();
  std::list<TxBlockSharedPtr> blocks;
  std::vector<bytes> extraStateDeltas;
  if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(blocks)) {
    LOG_GENERAL(WARNING, "RetrieveTxBlocks skipped or incompleted");
    return false;
  }

  blocks.sort([](const TxBlockSharedPtr& a, const TxBlockSharedPtr& b) {
    return a->GetHeader().GetBlockNum() < b->GetHeader().GetBlockNum();
  });

  unsigned int lastBlockNum = blocks.back()->GetHeader().GetBlockNum();

  unsigned int extra_txblocks = (lastBlockNum + 1) % NUM_FINAL_BLOCK_PER_POW;

  /// Retrieve final block state delta from last DS epoch to
  /// current TX epoch and buffer the statedelta for each.
  for (const auto& block : blocks) {
    if (block->GetHeader().GetBlockNum() >= lastBlockNum + 1 - extra_txblocks) {
      bytes stateDelta;
      if (!BlockStorage::GetBlockStorage().GetStateDelta(
              block->GetHeader().GetBlockNum(), stateDelta)) {
        // if any of state-delta is not fetched from extra txblocks set, simple
        // skip all extra blocks
        LOG_GENERAL(INFO, "Didn't find the state-delta for txBlkNum: "
                              << block->GetHeader().GetBlockNum()
                              << ". Will trim rest of txBlks");
        extraStateDeltas.clear();
        trimIncompletedBlocks = true;
        break;
      } else {
        extraStateDeltas.push_back(stateDelta);
      }
    }
  }

  if ((lastBlockNum - extra_txblocks + 1) %
          (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW) ==
      0) {
    // we must have latest state currently. so need not recreate states
    LOG_GENERAL(INFO, "Current state is up-to-date until txblk :"
                          << lastBlockNum - extra_txblocks);
  } else {
    // create states from last INCRDB_DSNUMS_WITH_STATEDELTAS *
    // NUM_FINAL_BLOCK_PER_POW txn blocks
    unsigned int lower_bound_txnblk =
        ((lastBlockNum - extra_txblocks + 1) >
         INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)
            ? (((lastBlockNum - extra_txblocks + 1) /
                (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)) *
               (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW))
            : 0;
    unsigned int upper_bound_txnblk = lastBlockNum - extra_txblocks;

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

        if ((i + 1) % NUM_FINAL_BLOCK_PER_POW ==
            0) {  // state-delta from vacous epoch
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
          if (!AccountStore::GetInstance().MoveUpdatesToDisk()) {
            LOG_GENERAL(WARNING, "AccountStore::MoveUpdatesToDisk failed");
            return false;
            ;
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

  if (boost::filesystem::exists("StateDeltaFromS3")) {
    try {
      boost::filesystem::remove_all("StateDeltaFromS3");
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING, "Failed to remove StateDeltaFromS3 directory");
    }
  }

  if (trimIncompletedBlocks) {
    // truncate the extra final blocks at last
    for (unsigned int i = 0; i < extra_txblocks; ++i) {
      if (!BlockStorage::GetBlockStorage().DeleteTxBlock(lastBlockNum - i)) {
        LOG_GENERAL(WARNING, "BlockStorage::DeleteTxBlock " << lastBlockNum - i
                                                            << " failed");
      }
      blocks.pop_back();
    }
  } else {
    /// Put extra state delta from last DS epoch
    unsigned int extra_delta_index = lastBlockNum - extra_txblocks + 1;
    for (const auto& stateDelta : extraStateDeltas) {
      if (!AccountStore::GetInstance().DeserializeDelta(stateDelta, 0)) {
        LOG_GENERAL(WARNING,
                    "AccountStore::GetInstance().DeserializeDelta failed");
        return false;
      }
      BlockStorage::GetBlockStorage().PutStateDelta(extra_delta_index++,
                                                    stateDelta);
    }
  }

  for (const auto& block : blocks) {
    m_mediator.m_node->AddBlock(*block);
  }

  return true;
}

bool Retriever::RetrieveBlockLink(bool trimIncompletedBlocks) {
  std::list<BlockLink> blocklinks;

  auto dsComm = m_mediator.m_blocklinkchain.GetBuiltDSComm();

  if (!BlockStorage::GetBlockStorage().GetAllBlockLink(blocklinks)) {
    LOG_GENERAL(WARNING, "RetrieveTxBlocks skipped or incompleted");
    return false;
  }
  blocklinks.sort([](const BlockLink& a, const BlockLink& b) {
    return std::get<BlockLinkIndex::INDEX>(a) <
           std::get<BlockLinkIndex::INDEX>(b);
  });

  if (!blocklinks.empty()) {
    if (m_mediator.m_ds->m_latestActiveDSBlockNum == 0) {
      bytes latestActiveDSBlockNumVec;
      if (!BlockStorage::GetBlockStorage().GetMetadata(
              MetaType::LATESTACTIVEDSBLOCKNUM, latestActiveDSBlockNumVec)) {
        LOG_GENERAL(WARNING, "Get LatestActiveDSBlockNum failed");
        return false;
      }
      m_mediator.m_ds->m_latestActiveDSBlockNum = std::stoull(
          DataConversion::CharArrayToString(latestActiveDSBlockNumVec));
    }
  } else {
    return false;
  }

  /// Check whether the termination of last running happens before the last
  /// DSEpoch properly ended.
  bytes isDSIncompleted;
  if (!BlockStorage::GetBlockStorage().GetMetadata(MetaType::DSINCOMPLETED,
                                                   isDSIncompleted)) {
    LOG_GENERAL(WARNING, "No GetMetadata or failed");
    return false;
  }

  if (!BlockStorage::GetBlockStorage().ResetDB(
          BlockStorage::DBTYPE::BLOCKLINK)) {
    LOG_GENERAL(WARNING, "BlockStorage::ResetDB (BLOCKLINK) failed");
    return false;
  }

  bool toDelete = false;

  if (isDSIncompleted[0] == '1') {
    /// Removing incompleted DS for upgrading protocol
    /// Keeping incompleted DS for node recovery
    if (trimIncompletedBlocks) {
      LOG_GENERAL(INFO, "Has incompleted DS Block, remove it");
      toDelete = true;
    }
  }

  uint64_t lastDsIndex = std::get<BlockLinkIndex::DSINDEX>(blocklinks.back());
  const BlockType lastType =
      std::get<BlockLinkIndex::BLOCKTYPE>(blocklinks.back());
  if (lastType != BlockType::DS) {
    if (lastDsIndex == 0) {
      LOG_GENERAL(WARNING, "FATAL: last ds index is 0 and blockType not DS");
      return false;
    }
    lastDsIndex--;
  }

  std::list<BlockLink>::iterator blocklinkItr;
  for (blocklinkItr = blocklinks.begin(); blocklinkItr != blocklinks.end();
       blocklinkItr++) {
    const auto& blocklink = *blocklinkItr;

    if (toDelete) {
      if ((std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::DS) &&
          (std::get<BlockLinkIndex::DSINDEX>(blocklink) == lastDsIndex)) {
        LOG_GENERAL(INFO, "Broke at DS Index " << lastDsIndex);
        break;
      }
    }

    if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::DS) {
      DSBlockSharedPtr dsblock;
      if (!BlockStorage::GetBlockStorage().GetDSBlock(
              std::get<BlockLinkIndex::DSINDEX>(blocklink), dsblock)) {
        LOG_GENERAL(WARNING,
                    "Could not find ds block num "
                        << std::get<BlockLinkIndex::DSINDEX>(blocklink));
        return false;
      }
      m_mediator.m_node->UpdateDSCommiteeComposition(dsComm, *dsblock);
      m_mediator.m_dsBlockChain.AddBlock(*dsblock);

    } else if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) ==
               BlockType::VC) {
      VCBlockSharedPtr vcblock;

      if (!BlockStorage::GetBlockStorage().GetVCBlock(
              std::get<BlockLinkIndex::BLOCKHASH>(blocklink), vcblock)) {
        LOG_GENERAL(WARNING,
                    "Could not find vc with blockHash "
                        << std::get<BlockLinkIndex::BLOCKHASH>(blocklink));
        return false;
      }
      m_mediator.m_node->UpdateRetrieveDSCommiteeCompositionAfterVC(*vcblock,
                                                                    dsComm);

    } else if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) ==
               BlockType::FB) {
      FallbackBlockSharedPtr fallbackwshardingstruct;
      if (!BlockStorage::GetBlockStorage().GetFallbackBlock(
              std::get<BlockLinkIndex::BLOCKHASH>(blocklink),
              fallbackwshardingstruct)) {
        LOG_GENERAL(WARNING,
                    "Could not find vc with blockHash "
                        << std::get<BlockLinkIndex::BLOCKHASH>(blocklink));
        return false;
      }
      uint32_t shard_id =
          fallbackwshardingstruct->m_fallbackblock.GetHeader().GetShardId();
      const PubKey& leaderPubKey =
          fallbackwshardingstruct->m_fallbackblock.GetHeader()
              .GetLeaderPubKey();
      const Peer& leaderNetworkInfo =
          fallbackwshardingstruct->m_fallbackblock.GetHeader()
              .GetLeaderNetworkInfo();
      const DequeOfShard& shards = fallbackwshardingstruct->m_shards;
      m_mediator.m_node->UpdateDSCommitteeAfterFallback(
          shard_id, leaderPubKey, leaderNetworkInfo, dsComm, shards);
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(dsComm);

    m_mediator.m_blocklinkchain.AddBlockLink(
        std::get<BlockLinkIndex::INDEX>(blocklink),
        std::get<BlockLinkIndex::DSINDEX>(blocklink),
        std::get<BlockLinkIndex::BLOCKTYPE>(blocklink),
        std::get<BlockLinkIndex::BLOCKHASH>(blocklink));
  }

  if (!toDelete) {
    return true;
  }

  for (; blocklinkItr != blocklinks.end(); blocklinkItr++) {
    const auto& blocklink = *blocklinkItr;
    if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::DS) {
      if (BlockStorage::GetBlockStorage().DeleteDSBlock(
              std::get<BlockLinkIndex::DSINDEX>(blocklink))) {
        if (!BlockStorage::GetBlockStorage().PutMetadata(
                MetaType::DSINCOMPLETED, {'0'})) {
          LOG_GENERAL(WARNING,
                      "BlockStorage::PutMetadata (DSINCOMPLETED) '0' failed");
          return false;
        }
      }
    } else if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) ==
               BlockType::VC) {
      if (!BlockStorage::GetBlockStorage().DeleteVCBlock(
              std::get<BlockLinkIndex::BLOCKHASH>(blocklink))) {
        LOG_GENERAL(WARNING, "Could not delete VC block");
      }
    } else if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) ==
               BlockType::FB) {
      if (!BlockStorage::GetBlockStorage().DeleteFallbackBlock(
              std::get<BlockLinkIndex::BLOCKHASH>(blocklink))) {
        LOG_GENERAL(WARNING, "Could not deleteLoop  FB block");
      }
    }
  }

  return true;
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
