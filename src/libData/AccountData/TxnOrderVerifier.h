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

  std::vector<unsigned int> matchedIndex;

  for (const auto& th : expectedTxns) {
    auto t = m_tranHashMap.find(th);
    if (t != m_tranHashMap.end() &&
        (matchedIndex.empty() || t->second > matchedIndex.back()) &&
        (t->second <= ((tolerance_in_percent * receivedTxns.size() / 100 +
                        matchedIndex.size())))) {
      matchedIndex.push_back(t->second);
    }
  }

  unsigned int min_ordered_txn_num =
      (100 - tolerance_in_percent) * receivedTxns.size() / 100;

  LOG_GENERAL(INFO, "Minimum in order num required: "
                        << min_ordered_txn_num << " actual in order num: "
                        << matchedIndex.size() << " similarity: "
                        << (matchedIndex.size() * 100 / receivedTxns.size())
                        << "% "
                        << "tolerance: " << tolerance_in_percent << "%");

  if (matchedIndex.size() >= min_ordered_txn_num) {
    return true;
  }

  LOG_GENERAL(INFO, "Txns in order:");
  for (const auto& index : matchedIndex) {
    LOG_GENERAL(INFO, receivedTxns.at(index).hex());
  }

  return false;
}

#endif  // __TXNORDERVERIFIER_H__