/*
 * Copyright (C) 2022 Zilliqa
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

#include "libUtils/BlockTransactionsHelper.h"
#include "libData/BlockChainData/BlockChain.h"

#include "common/BaseType.h"

boost::optional<uint32_t> BlockTransactionsHelper::GetTransactionIndexInBlock(
    TxBlockChain& blockChain, const std::string& txHashStr,
    const std::string& txBlockHashStr) {
  const BlockHash blockHash{txBlockHashStr};
  const auto txBlock = blockChain.GetBlockByHash(blockHash);

  MicroBlockSharedPtr microBlockPtr;
  uint32_t indexInBlock = 0;

  const TxnHash inputTxHash{txHashStr};

  const auto& microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const& mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto& currTranHashes = microBlockPtr->GetTranHashes();

    for (const auto& currHash : currTranHashes) {
      if (currHash == inputTxHash) {
        return indexInBlock;
      }
      indexInBlock++;
    }
  }
  return {};
}