/*
 * Copyright (C) 2020 Zilliqa
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

#include <cstdlib>
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/Retriever.h"
#include "libUtils/UpgradeManager.h"

/// Should be run from a location with constants.xml and persistence folder

using namespace std;

int main(int argc, const char* argv[]) {
  TxBlockSharedPtr txBlockPtr;
  if (!BlockStorage::GetBlockStorage().GetLatestTxBlock(txBlockPtr)) {
    LOG_GENERAL(WARNING, "BlockStorage::GetLatestTxBlock failed");
    return -1;
  }
  const uint64_t fromBlock = argc > 1 ? strtol(argv[1], NULL, 10) : 0;
  const uint64_t toBlock = argc > 2 ? strtol(argv[2], NULL, 10)
                                    : txBlockPtr->GetHeader().GetBlockNum();

  if (fromBlock != 0) {
    // DSBlock 0
    DSBlockSharedPtr dsBlock0;
    if (!BlockStorage::GetBlockStorage().GetDSBlock(0, dsBlock0)) {
      LOG_GENERAL(WARNING, "Missing DS Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, *dsBlock0);
    uint64_t origTimestamp = dsBlock0->GetTimestamp();

    DSBlockSharedPtr dsBlock1;
    if (!BlockStorage::GetBlockStorage().GetDSBlock(1, dsBlock1)) {
      LOG_GENERAL(WARNING, "Missing DS Block 1");
      return -1;
    }
    LOG_GENERAL(INFO, "PrevHash In DS Block 1 = "
                          << dsBlock1->GetHeader().GetPrevHash());
    LOG_GENERAL(INFO,
                "DS Block 0 Hash = " << dsBlock0->GetHeader().GetMyHash());

    DSBlock genDSBlock = Synchronizer::ConstructGenesisDSBlock();
    genDSBlock.SetTimestamp(origTimestamp);
    bytes serializedDSBlock;
    genDSBlock.Serialize(serializedDSBlock, 0);
    if (!BlockStorage::GetBlockStorage().PutDSBlock(
            genDSBlock.GetHeader().GetBlockNum(), serializedDSBlock)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutDSBlock failed");
      return -1;
    }

    if (!BlockStorage::GetBlockStorage().GetDSBlock(0, dsBlock0)) {
      LOG_GENERAL(WARNING, "Missing DS Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, *dsBlock0);
    LOG_GENERAL(INFO,
                "New DS Block 0 Hash = " << dsBlock0->GetHeader().GetMyHash());

    // DirBlock 0
    BlockLinkSharedPtr dirBlock0;
    if (!BlockStorage::GetBlockStorage().GetBlockLink(0, dirBlock0)) {
      LOG_GENERAL(WARNING, "Missing Dir Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, "Dir Block 0 Hash = "
                          << std::get<BlockLinkIndex::BLOCKHASH>(*dirBlock0));

    bytes serializedDirBlock;
    if (!Messenger::SetBlockLink(
            serializedDirBlock, 0,
            std::make_tuple(std::get<BlockLinkIndex::VERSION>(*dirBlock0),
                            std::get<BlockLinkIndex::INDEX>(*dirBlock0),
                            std::get<BlockLinkIndex::DSINDEX>(*dirBlock0),
                            std::get<BlockLinkIndex::BLOCKTYPE>(*dirBlock0),
                            dsBlock0->GetHeader().GetMyHash()))) {
      LOG_GENERAL(WARNING, "Messenger::SetBlockLink failed");
      return -1;
    }
    if (!BlockStorage::GetBlockStorage().PutBlockLink(
            std::get<BlockLinkIndex::INDEX>(*dirBlock0), serializedDirBlock)) {
      LOG_GENERAL(WARNING, "PutBlockLink failed");
      return -1;
    }

    if (!BlockStorage::GetBlockStorage().GetBlockLink(0, dirBlock0)) {
      LOG_GENERAL(WARNING, "Missing Dir Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, "New Dir Block 0 Hash = "
                          << std::get<BlockLinkIndex::BLOCKHASH>(*dirBlock0));

    // TxBlock 0
    TxBlockSharedPtr txBlock0;
    if (!BlockStorage::GetBlockStorage().GetTxBlock(0, txBlock0)) {
      LOG_GENERAL(WARNING, "Missing Tx Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, *txBlock0);
    origTimestamp = txBlock0->GetTimestamp();

    TxBlockSharedPtr txBlock1;
    if (!BlockStorage::GetBlockStorage().GetTxBlock(1, txBlock1)) {
      LOG_GENERAL(WARNING, "Missing Tx Block 1");
      return -1;
    }
    LOG_GENERAL(INFO, "PrevHash In Tx Block 1 = "
                          << txBlock1->GetHeader().GetPrevHash());
    LOG_GENERAL(INFO,
                "Tx Block 0 Hash = " << txBlock0->GetHeader().GetMyHash());

    TxBlock genTxBlock = Synchronizer::ConstructGenesisTxBlock();
    genTxBlock.SetTimestamp(origTimestamp);
    bytes serializedTxBlock;
    genTxBlock.Serialize(serializedTxBlock, 0);
    if (!BlockStorage::GetBlockStorage().PutTxBlock(
            genTxBlock.GetHeader().GetBlockNum(), serializedTxBlock)) {
      LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed");
      return -1;
    }

    if (!BlockStorage::GetBlockStorage().GetTxBlock(0, txBlock0)) {
      LOG_GENERAL(WARNING, "Missing Tx Block 0");
      return -1;
    }
    LOG_GENERAL(INFO, *txBlock0);
    LOG_GENERAL(INFO,
                "New Tx Block 0 Hash = " << txBlock0->GetHeader().GetMyHash());
  }

  LOG_GENERAL(INFO, "Migrating from TxBlock=" << fromBlock
                                              << " to TxBlock=" << toBlock);
  cout << "Migrating from TxBlock=" << fromBlock << " to TxBlock=" << toBlock
       << endl;
  for (uint64_t txBlockNum = fromBlock; txBlockNum <= toBlock; txBlockNum++) {
    if ((txBlockNum % 1000) == 0) {
      cout << "At TxBlock " << txBlockNum << endl;
    }
    if (!BlockStorage::GetBlockStorage().GetTxBlock(txBlockNum, txBlockPtr)) {
      LOG_GENERAL(FATAL, "Failed to get TxBlock " << txBlockNum);
      return -1;
    }
    auto microBlockInfos = txBlockPtr->GetMicroBlockInfos();
    for (auto const& mbInfo : microBlockInfos) {
      MicroBlockSharedPtr mbptr;
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(
              mbInfo.m_microBlockHash, mbptr)) {
        LOG_GENERAL(WARNING, "Missing MB " << mbInfo.m_microBlockHash
                                           << " for TxBlock " << txBlockNum);
        continue;
      }
      bytes body;
      mbptr->Serialize(body, 0);
      if (!BlockStorage::GetBlockStorage().PutMicroBlock(
              mbInfo.m_microBlockHash, mbptr->GetHeader().GetEpochNum(),
              mbptr->GetHeader().GetShardId(), body)) {
        LOG_GENERAL(FATAL, "Failed to write MB " << mbInfo.m_microBlockHash
                                                 << " for TxBlock "
                                                 << txBlockNum);
        return -1;
      }
      const std::vector<TxnHash>& tranHashes = mbptr->GetTranHashes();
      const uint64_t& epochNum = mbptr->GetHeader().GetEpochNum();
      bytes epoch;
      if (!Messenger::SetTxEpoch(epoch, 0, epochNum)) {
        LOG_GENERAL(WARNING, "Messenger::SetTxEpoch failed.");
        return -1;
      }
      if (tranHashes.size() > 0) {
        for (const auto& tranHash : tranHashes) {
          TxBodySharedPtr txBody;
          if (!BlockStorage::GetBlockStorage().GetTxBody(tranHash, txBody)) {
            LOG_GENERAL(WARNING, "Missing Tx " << tranHash << " for MB "
                                               << mbInfo.m_microBlockHash
                                               << " TxBlock " << txBlockNum);
            continue;
          }
          bytes serializedTxBody;
          txBody->Serialize(serializedTxBody, 0);
          if (!BlockStorage::GetBlockStorage().PutTxBody(
                  epoch, epochNum, tranHash, serializedTxBody)) {
            LOG_GENERAL(FATAL, "Failed to write Tx " << tranHash << " for MB "
                                                     << mbInfo.m_microBlockHash
                                                     << " TxBlock "
                                                     << txBlockNum);
            return -1;
          }
        }
      }
    }
  }

  return 0;
}