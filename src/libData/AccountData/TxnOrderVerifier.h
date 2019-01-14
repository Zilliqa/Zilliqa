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

#ifndef __TXNORDERVERIFIER_H__
#define __TXNORDERVERIFIER_H__

#include <unordered_map>
#include <vector>

#include "common/Constants.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/Logger.h"

bool VerifyTxnOrderWTolerance(const std::vector<TxnHash>& expectedTxns,
                              const std::vector<TxnHash>& receivedTxns,
                              unsigned int tolerance_in_percent) {
  LOG_MARKER();

  if (expectedTxns.empty() && receivedTxns.empty()) {
    return true;
  }

  if (expectedTxns.empty()) {
    return false;
  }

  std::unordered_map<TxnHash, unsigned int> m_tranHashMap;

  for (unsigned int i = 0; i < receivedTxns.size(); i++) {
    m_tranHashMap.insert({receivedTxns.at(i), i});
  }

  std::vector<unsigned int> matchedIndexes;

  for (const auto& th : expectedTxns) {
    auto t = m_tranHashMap.find(th);
    // if txn was found in map &&
    // (matchedIndexs empty || the txn has larger index than previous element
    //    (to ensure txn has incremental index in the order)) &&
    // if the index of this txn was in the potion where can guarantee the
    // successive of the verification,
    //    which can be used to avoid too big index being inserted in the
    //    matchedIndexes too early causing all the txns left was not able to be
    //    inserted. e.g. to verify 10 txns with tolerance of 20%, ordered by 0 9
    //    1 3 2 4 5 6 7 8 10, the expected order is 0 1 2 3 4 5 6 7 8 9 10 11 12
    //    when it comes to 9, which is out of the 20% of the size of the
    //    received order(2) + sizeof matchedIndexes(1), discard when it comes to
    //    3, which is in the the range (2 + sizeof{0,1}) = 4, accept finally the
    //    indexed items are 0 1 3 4 5 6 7 8 10, size(9), which is more than 80%
    //    of the size in the expecting order (12*0.8 = 9), thus return true if
    //    the expectig order is 0 1 2 3 4 5 6 7 8 9 10 11 12 13, then the min
    //    size will be (10), it will return false
    if (t != m_tranHashMap.end() &&
        (matchedIndexes.empty() || t->second > matchedIndexes.back()) &&
        (t->second <
         ((tolerance_in_percent * receivedTxns.size() / ONE_HUNDRED_PERCENT +
           matchedIndexes.size())))) {
      matchedIndexes.push_back(t->second);
    }
  }

  unsigned int min_ordered_txn_num = (unsigned int)(ceil(
      (double)((ONE_HUNDRED_PERCENT - tolerance_in_percent) *
               expectedTxns.size()) /
      (double)ONE_HUNDRED_PERCENT));

  LOG_GENERAL(INFO, "Minimum in order num required: "
                        << min_ordered_txn_num << " actual in order num: "
                        << matchedIndexes.size() << " similarity: "
                        << (matchedIndexes.size() * ONE_HUNDRED_PERCENT /
                            expectedTxns.size())
                        << "% "
                        << "tolerance: " << tolerance_in_percent << "%");

  if (matchedIndexes.size() >= min_ordered_txn_num) {
    return true;
  }

  LOG_GENERAL(INFO, "Txns not in order, ordered txns:");
  for (const auto& index : matchedIndexes) {
    LOG_GENERAL(INFO, receivedTxns.at(index).hex());
  }

  return false;
}

#endif  // __TXNORDERVERIFIER_H__
