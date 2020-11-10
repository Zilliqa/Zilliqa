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
      if (mbInfo.m_txnRootHash == TxnHash()) {
        continue;
      }
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