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

#include "RootComputation.h"
#include "libCrypto/Sha2.h"

using namespace std;
using namespace dev;

namespace {
template <typename T, typename R>
const R& GetHash(const T& item);

inline const TxnHash& GetHash(const TxnHash& item) { return item; }

inline const TxnHash& GetHash(const Transaction& item) {
  return item.GetTranID();
}

inline const TxnHash& GetHash(
    const std::pair<const TxnHash, Transaction>& item) {
  return item.second.GetTranID();
}

inline const TxnHash& GetHash(const TransactionWithReceipt& item) {
  return item.GetTransaction().GetTranID();
}
};  // namespace

template <typename... Container>
TxnHash ConcatTranAndHash(const Container&... conts) {
  LOG_MARKER();

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  bool hasValue = false;

  (void)std::initializer_list<int>{(
      [](const auto& list, decltype(sha2)& sha2, bool& hasValue) {
        if (list.empty()) {
          return;
        }
        hasValue = true;

        for (auto& item : list) {
          sha2.Update(GetHash(item).asBytes());
        }
      }(conts, sha2, hasValue),
      0)...};

  return hasValue ? TxnHash{sha2.Finalize()} : TxnHash();
}

h256 ComputeRoot(const vector<h256>& hashes) {
  LOG_MARKER();

  return ConcatTranAndHash(hashes);
}

TxnHash ComputeRoot(const list<Transaction>& receivedTransactions,
                    const list<Transaction>& submittedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(receivedTransactions, submittedTransactions);
}

TxnHash ComputeRoot(
    const unordered_map<TxnHash, Transaction>& processedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(processedTransactions);
}

TxnHash ComputeRoot(
    const unordered_map<TxnHash, Transaction>& receivedTransactions,
    const unordered_map<TxnHash, Transaction>& submittedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(receivedTransactions, submittedTransactions);
}

TxnHash ComputeRoot(const vector<TransactionWithReceipt>& transactions) {
  LOG_MARKER();

  return ConcatTranAndHash(transactions);
}