/*
 * Copyright (C) 2024 Zilliqa
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

#include <libBlockchain/TxBlock.h>
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/Logger.h"

using namespace std;
int main(int argc, char* argv[]) {
  INIT_STDOUT_LOGGER();
  uint64_t maxTxBlockNum = 10;
  TxBlockSharedPtr txBlock;
  dev::h256 txnHashh256(
      "425b600e982da68ab6c3daa1c6e45d1a941d8e89391a9f8d131dfc22662f2f33");

  for (uint64_t blockNum = 0; blockNum <= maxTxBlockNum; ++blockNum) {
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, txBlock)) {
      LOG_GENERAL(INFO, "GetTxBlock failed for " << blockNum);
      continue;
    }
    uint32_t numTransactions = txBlock->GetHeader().GetNumTxs();
    LOG_GENERAL(INFO, "blockNum = " << blockNum << " numTransactions = "
                                    << numTransactions);
    auto microBlockInfos = txBlock->GetMicroBlockInfos();
    for (auto const& mbInfo : microBlockInfos) {
      MicroBlockSharedPtr mbptr;
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(
              mbInfo.m_microBlockHash, mbptr)) {
        LOG_GENERAL(INFO, "Error for block hash = " << mbInfo.m_microBlockHash);
      }
      if (!mbptr) {
        LOG_GENERAL(INFO, "No microblock present for hash = "
                              << mbInfo.m_microBlockHash);
        continue;
      }
      const std::vector<TxnHash>& tranHashes = mbptr->GetTranHashes();
      for (const auto& txnhash : tranHashes) {
        LOG_GENERAL(INFO,
                    "txn hash = " << txnhash << " block num = " << blockNum);
        if (txnhash == txnHashh256) {
          LOG_GENERAL(INFO,
                      "txn hash = " << txnhash
                                    << " present in block num = " << blockNum);
        }
        TxBodySharedPtr tptr;
        bool isPresent =
            BlockStorage::GetBlockStorage().GetTxBody(txnhash, tptr);
        if (!isPresent) {
          LOG_GENERAL(INFO,
                      "Txn body is not present for txn has = " << txnhash);
          continue;
        }
        const Transaction& txn = tptr->GetTransaction();
        LOG_GENERAL(INFO, "txn id = " << txn.GetTranID().hex()
                                      << "nonce = " << txn.GetNonce());
      }
    }
  }
  return 0;
}