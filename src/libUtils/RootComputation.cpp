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
