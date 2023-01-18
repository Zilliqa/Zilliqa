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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_MBNFORWARDEDTXNENTRY_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_MBNFORWARDEDTXNENTRY_H_

#include "Transaction.h"
#include "TransactionReceipt.h"
#include "libBlockchain/MicroBlock.h"

struct MBnForwardedTxnEntry {
  MicroBlock m_microBlock;
  std::vector<TransactionWithReceipt> m_transactions;

  friend std::ostream& operator<<(std::ostream& os,
                                  const MBnForwardedTxnEntry& t);
};

inline std::ostream& operator<<(std::ostream& os,
                                const MBnForwardedTxnEntry& t) {
  const auto& hdr = t.m_microBlock.GetHeader();
  os << "mbHash=" << t.m_microBlock.GetBlockHash().hex()
     << " epochNum=" << hdr.GetEpochNum() << " shardId=" << hdr.GetShardId();
  const auto& txRootHash = hdr.GetTxRootHash();
  if (txRootHash) {
    os << " txRootHash=" << txRootHash.hex();
  }
  return os;
}

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_MBNFORWARDEDTXNENTRY_H_
