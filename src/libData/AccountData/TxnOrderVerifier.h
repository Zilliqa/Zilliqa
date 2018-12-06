/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
        (t->second < ((tolerance_in_percent * receivedTxns.size() / 100 +
                       matchedIndexes.size())))) {
      matchedIndexes.push_back(t->second);
    }
  }

  unsigned int min_ordered_txn_num =
      (100 - tolerance_in_percent) * expectedTxns.size() / 100;

  LOG_GENERAL(INFO, "Minimum in order num required: "
                        << min_ordered_txn_num << " actual in order num: "
                        << matchedIndexes.size() << " similarity: "
                        << (matchedIndexes.size() * 100 / expectedTxns.size())
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