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

#ifndef __ROOTCOMPUTATION_H__
#define __ROOTCOMPUTATION_H__

#include <list>
#include <unordered_map>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/MemoryDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"

dev::h256 ComputeRoot(const std::vector<dev::h256>& hashes);

TxnHash ComputeRoot(const std::list<Transaction>& receivedTransactions,
                    const std::list<Transaction>& submittedTransactions);

TxnHash ComputeRoot(
    const std::unordered_map<TxnHash, Transaction>& processedTransactions);

TxnHash ComputeRoot(
    const std::unordered_map<TxnHash, Transaction>& receivedTransactions,
    const std::unordered_map<TxnHash, Transaction>& submittedTransactions);

TxnHash ComputeRoot(const std::vector<TransactionWithReceipt>& transactions);

#endif  // __ROOTCOMPUTATION_H__
