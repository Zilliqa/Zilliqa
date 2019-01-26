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

#ifndef __MBNFORWARDEDTXNENTRY_H__
#define __MBNFORWARDEDTXNENTRY_H__

#include "AccountStore.h"
#include "Transaction.h"
#include "TransactionReceipt.h"
#include "libData/BlockData/Block/MicroBlock.h"

struct MBnForwardedTxnEntry {
  MicroBlock m_microBlock;
  std::vector<TransactionWithReceipt> m_transactions;

  friend std::ostream& operator<<(std::ostream& os,
                                  const MBnForwardedTxnEntry& t);
};

inline std::ostream& operator<<(std::ostream& os,
                                const MBnForwardedTxnEntry& t) {
  os << "<MBnForwardedTxnEntry>" << std::endl
     << " mbHash     = " << t.m_microBlock.GetBlockHash().hex() << std::endl
     << " txRootHash = " << t.m_microBlock.GetHeader().GetTxRootHash().hex()
     << std::endl
     << " epochNum   = " << t.m_microBlock.GetHeader().GetEpochNum()
     << std::endl
     << " shardId    = " << t.m_microBlock.GetHeader().GetShardId();
  return os;
}

#endif  // __MBNFORWARDEDTXNENTRY_H__
