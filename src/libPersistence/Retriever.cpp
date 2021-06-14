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

bool Retriever::RetrieveTxBlocks() {
  LOG_MARKER();

  std::vector<bytes> extraStateDeltas;
  TxBlockSharedPtr latestTxBlock;
  bool trimIncompletedBlocks = false;

  if (!BlockStorage::GetBlockStorage().GetLatestTxBlock(latestTxBlock)) {
    LOG_GENERAL(WARNING, "GetLatestTxBlock failed");
    return false;
  }

  uint64_t lastBlockNum = latestTxBlock->GetHeader().GetBlockNum();

  uint64_t extra_txblocks = (lastBlockNum + 1) % NUM_FINAL_BLOCK_PER_POW;

  for (uint64_t blockNum = lastBlockNum + 1 - extra_txblocks;
       blockNum <= lastBlockNum; blockNum++) {
    bytes stateDelta;
    if (!BlockStorage::GetBlockStorage().GetStateDelta(blockNum, stateDelta)) {
      LOG_GENERAL(INFO, "Didn't find the state-delta for txBlkNum: "
                            << blockNum << ". Try fetching it from seeds");
      unsigned int retry = 1;
      while (retry <= RETRY_GETSTATEDELTAS_COUNT) {
        // Get the state-delta for this txBlock from random seed nodes
        std::unique_lock<std::mutex> cv_lk(
            m_mediator.m_lookup->m_mutexSetStateDeltaFromSeed);
        m_mediator.m_lookup->m_skipAddStateDeltaToAccountStore = true;
        m_mediator.m_lookup->GetStateDeltaFromSeedNodes(blockNum);
        if (m_mediator.m_lookup->cv_setStateDeltaFromSeed.wait_for(
                cv_lk,
                std::chrono::seconds(GETSTATEDELTAS_TIMEOUT_IN_SECONDS)) ==
            std::cv_status::timeout) {
          LOG_GENERAL(WARNING,
                      "[Retry: " << retry
                                 << "] Didn't receive statedelta for txBlkNum: "
                                 << blockNum << "! Will try again");
          retry++;
        } else {
          break;
        }
      }
      // if state-delta is still not fetched from extra txblocks set, simple
      // skip all extra blocks
      if (retry > RETRY_GETSTATEDELTAS_COUNT) {
        extraStateDeltas.clear();
        trimIncompletedBlocks = true;
        break;
      }

      // got state-delta at last
      BlockStorage::GetBlockStorage().GetStateDelta(blockNum, stateDelta);
      BlockStorage::GetBlockStorage().DeleteStateDelta(blockNum);
    }
    // store it.
    extraStateDeltas.push_back(stateDelta);
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
    uint64_t lower_bound_txnblk =
        ((lastBlockNum - extra_txblocks + 1) >
         INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)
            ? (((lastBlockNum - extra_txblocks + 1) /
                (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW)) *
               (INCRDB_DSNUMS_WITH_STATEDELTAS * NUM_FINAL_BLOCK_PER_POW))
            : 0;
    uint64_t upper_bound_txnblk = lastBlockNum - extra_txblocks;

    LOG_GENERAL(INFO, "Will try recreating state from txnblks: "
                          << lower_bound_txnblk << " - " << upper_bound_txnblk);

    if (KEEP_HISTORICAL_STATE) {
      uint64_t earliestTrieSnapshotEpoch = std::numeric_limits<uint64_t>::max();
      bytes earliestTrieSnapshotEpochBytes;
      if (BlockStorage::GetBlockStorage().GetMetadata(
              MetaType::EARLIEST_HISTORY_STATE_EPOCH,
              earliestTrieSnapshotEpochBytes)) {
        try {
          earliestTrieSnapshotEpoch =
              std::stoull(DataConversion::CharArrayToString(
                  earliestTrieSnapshotEpochBytes));
        } catch (...) {
          LOG_GENERAL(
              WARNING,
              "EARLIEST_HISTORY_STATE_EPOCH cannot be parsed as uint64_t "
                  << DataConversion::CharArrayToString(
                         earliestTrieSnapshotEpochBytes));
          return false;
        }
      } else {
        LOG_GENERAL(INFO,
                    "No EARLIEST_HISTORY_STATE_EPOCH from local persistence");
      }
      m_mediator.m_initTrieSnapshotDSEpoch = earliestTrieSnapshotEpoch;
      BlockStorage::GetBlockStorage().PutMetadata(
          MetaType::EARLIEST_HISTORY_STATE_EPOCH,
          DataConversion::StringToCharArray(
              std::to_string(m_mediator.m_initTrieSnapshotDSEpoch)));
    }

    // clear all the state deltas from disk.
    if (!BlockStorage::GetBlockStorage().ResetDB(BlockStorage::STATE_DELTA)) {
      LOG_GENERAL(WARNING, "BlockStorage::ResetDB failed");
      return false;
    }

    std::string target = STORAGE_PATH + PERSISTENCE_PATH + "/stateDelta";
    uint64_t firstStateDeltaIndex = lower_bound_txnblk;
    for (uint64_t i = lower_bound_txnblk; i <= upper_bound_txnblk; i++) {
      // Check if StateDeltaFromS3/StateDelta_{i} exists and copy over to the
      // local persistence/stateDelta
      std::string source = STORAGE_PATH + STATEDELTAFROMS3_PATH +
                           "/stateDelta_" + std::to_string(i);
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
          for (uint64_t j = firstStateDeltaIndex; j <= i; j++) {
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

              TxBlockSharedPtr txBlockPerDelta;
              if (!BlockStorage::GetBlockStorage().GetTxBlock(
                      j, txBlockPerDelta)) {
                LOG_GENERAL(WARNING, "GetTxBlock failed for " << j);
                return false;
              }

              if (AccountStore::GetInstance().GetStateRootHash() !=
                  txBlockPerDelta->GetHeader().GetStateRootHash()) {
                LOG_GENERAL(
                    WARNING,
                    "StateRoot in TxBlock(BlockNum: "
                        << j << ") : does not match retrieved stateroot hash");
                return false;
              }
            }
          }
          // commit the state to disk
          if (!AccountStore::GetInstance().MoveUpdatesToDisk(
                  i / NUM_FINAL_BLOCK_PER_POW,
                  m_mediator.m_initTrieSnapshotDSEpoch)) {
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

  if (boost::filesystem::exists(STORAGE_PATH + STATEDELTAFROMS3_PATH)) {
    try {
      boost::filesystem::remove_all(STORAGE_PATH + STATEDELTAFROMS3_PATH);
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING, "Failed to remove " + STORAGE_PATH +
                               STATEDELTAFROMS3_PATH + " directory");
    }
  }

  if (trimIncompletedBlocks) {
    // truncate the extra final blocks at last
    for (uint64_t i = 0; i < extra_txblocks; ++i) {
      if (!BlockStorage::GetBlockStorage().DeleteTxBlock(lastBlockNum - i)) {
        LOG_GENERAL(WARNING, "BlockStorage::DeleteTxBlock " << lastBlockNum - i
                                                            << " failed");
      }
    }
  } else {
    /// Put extra state delta from last DS epoch
    uint64_t extra_delta_index = lastBlockNum - extra_txblocks + 1;
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

  m_mediator.m_node->AddBlock(*latestTxBlock);

  return true;
}

bool Retriever::RetrieveBlockLink() {
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

      auto dsBlockNumStr =
          DataConversion::CharArrayToString(latestActiveDSBlockNumVec);
      try {
        m_mediator.m_ds->m_latestActiveDSBlockNum = std::stoull(dsBlockNumStr);
      } catch (const std::exception& e) {
        LOG_GENERAL(WARNING, "Cannot convert invalid DS block number "
                                 << dsBlockNumStr << ", exception "
                                 << e.what());
        return false;
      }
    }
  } else {
    return false;
  }

  if (!BlockStorage::GetBlockStorage().ResetDB(
          BlockStorage::DBTYPE::BLOCKLINK)) {
    LOG_GENERAL(WARNING, "BlockStorage::ResetDB (BLOCKLINK) failed");
    return false;
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

  LOG_GENERAL(INFO,
              "Reconstructing DS committee from blocklinks (this may take some "
              "time)...");

  std::list<BlockLink>::iterator blocklinkItr;
  for (blocklinkItr = blocklinks.begin(); blocklinkItr != blocklinks.end();
       ++blocklinkItr) {
    const auto& blocklink = *blocklinkItr;

    if (std::get<BlockLinkIndex::BLOCKTYPE>(blocklink) == BlockType::DS) {
      DSBlockSharedPtr dsblock;
      if (!BlockStorage::GetBlockStorage().GetDSBlock(
              std::get<BlockLinkIndex::DSINDEX>(blocklink), dsblock)) {
        LOG_GENERAL(WARNING,
                    "Could not find ds block num "
                        << std::get<BlockLinkIndex::DSINDEX>(blocklink));
        return false;
      }

      m_mediator.m_node->UpdateDSCommitteeComposition(dsComm, *dsblock, false);
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
      m_mediator.m_node->UpdateRetrieveDSCommitteeCompositionAfterVC(
          *vcblock, dsComm, false);
    }

    m_mediator.m_blocklinkchain.SetBuiltDSComm(dsComm);

    m_mediator.m_blocklinkchain.AddBlockLink(
        std::get<BlockLinkIndex::INDEX>(blocklink),
        std::get<BlockLinkIndex::DSINDEX>(blocklink),
        std::get<BlockLinkIndex::BLOCKTYPE>(blocklink),
        std::get<BlockLinkIndex::BLOCKHASH>(blocklink), false);
  }

  LOG_GENERAL(INFO, "Reconstructing DS committee done");

  return true;
}

bool Retriever::RetrieveStates() {
  LOG_MARKER();
  return AccountStore::GetInstance().RetrieveFromDisk();
}

bool Retriever::RetrieveStatesOld() {
  return AccountStore::GetInstance().RetrieveFromDiskOld();
}

bool Retriever::ValidateStates() {
  LOG_MARKER();

  if (CONTRACT_STATES_MIGRATED) {
    LOG_GENERAL(INFO,
                "Data migration just applied, skip for this time, remember to "
                "disable if it's done");
    return true;
  }

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

bool Retriever::MigrateContractStates(
    bool ignore_checker, bool disambiguation,
    const std::string& contract_address_output_filename,
    const std::string& normal_address_output_filename) {
  return AccountStore::GetInstance().MigrateContractStates(
      ignore_checker, disambiguation, contract_address_output_filename,
      normal_address_output_filename);
}
