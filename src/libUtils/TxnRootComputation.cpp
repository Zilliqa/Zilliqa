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

#include "TxnRootComputation.h"
#include "libCrypto/Sha2.h"

using namespace dev;

namespace {
template <typename T, typename R>
const R& GetTranID(const T& item);

inline const TxnHash& GetTranID(const TxnHash& item) { return item; }

inline const TxnHash& GetTranID(const Transaction& item) {
  return item.GetTranID();
}

inline const TxnHash& GetTranID(
    const std::pair<const TxnHash, Transaction>& item) {
  return item.second.GetTranID();
}

inline const TxnHash& GetTranID(const MicroBlockHashSet& item) {
  return item.m_txRootHash;
}

inline const StateHash& GetStateID(const MicroBlockHashSet& item) {
  return item.m_stateDeltaHash;
}

inline const TxnHash& GetTranReceiptID(const MicroBlockHashSet& item) {
  return item.m_tranReceiptHash;
}
};  // namespace

template <typename... Container>
TxnHash ConcatTranAndHash(const Container&... conts) {
  LOG_MARKER();

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  (void)std::initializer_list<int>{(
      [](const auto& list, decltype(sha2)& sha2) {
        for (auto& item : list) {
          sha2.Update(GetTranID(item).asBytes());
        }
      }(conts, sha2),
      0)...};

  return TxnHash{sha2.Finalize()};
}

template <typename... Container>
StateHash ConcatStateAndHash(const Container&... conts) {
  LOG_MARKER();

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  (void)std::initializer_list<int>{(
      [](const auto& list, decltype(sha2)& sha2) {
        for (auto& item : list) {
          sha2.Update(GetStateID(item).asBytes());
        }
      }(conts, sha2),
      0)...};

  return StateHash{sha2.Finalize()};
}

template <typename... Container>
TxnHash ConcatTranReceiptAndHash(const Container&... conts) {
  LOG_MARKER();

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;

  (void)std::initializer_list<int>{(
      [](const auto& list, decltype(sha2)& sha2) {
        for (auto& item : list) {
          sha2.Update(GetTranReceiptID(item).asBytes());
        }
      }(conts, sha2),
      0)...};

  return TxnHash{sha2.Finalize()};
}

TxnHash ComputeTransactionsRoot(const std::vector<TxnHash>& transactionHashes) {
  LOG_MARKER();

  if (transactionHashes.empty()) {
    return TxnHash();
  }

  return ConcatTranAndHash(transactionHashes);
}

TxnHash ComputeTransactionsRoot(
    const std::list<Transaction>& receivedTransactions,
    const std::list<Transaction>& submittedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(receivedTransactions, submittedTransactions);
}

TxnHash ComputeTransactionsRoot(
    const std::unordered_map<TxnHash, Transaction>& processedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(processedTransactions);
}

TxnHash ComputeTransactionsRoot(
    const std::unordered_map<TxnHash, Transaction>& receivedTransactions,
    const std::unordered_map<TxnHash, Transaction>& submittedTransactions) {
  LOG_MARKER();

  return ConcatTranAndHash(receivedTransactions, submittedTransactions);
}

TxnHash ComputeTransactionsRoot(
    const std::vector<MicroBlockHashSet>& microBlockHashes) {
  LOG_MARKER();

  return ConcatTranAndHash(microBlockHashes);
}

StateHash ComputeDeltasRoot(
    const std::vector<MicroBlockHashSet>& microBlockHashes) {
  LOG_MARKER();

  return ConcatStateAndHash(microBlockHashes);
}

TxnHash ComputeTranReceiptsRoot(
    const std::vector<MicroBlockHashSet>& microBlockHashes) {
  LOG_MARKER();

  return ConcatTranReceiptAndHash(microBlockHashes);
}
