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

#ifndef __TRANSACTIONRECEIPT_H__
#define __TRANSACTIONRECEIPT_H__

#include <json/json.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <unordered_map>
#include <vector>

#include "LogEntry.h"
#include "Transaction.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

class TransactionReceipt : public SerializableDataBlock {
  Json::Value m_tranReceiptObj = Json::nullValue;
  std::string m_tranReceiptStr;
  uint64_t m_cumGas = 0;

 public:
  TransactionReceipt();
  bool Serialize(std::vector<unsigned char>& dst,
                 unsigned int offset) const override;
  bool Deserialize(const std::vector<unsigned char>& src,
                   unsigned int offset) override;
  void SetResult(const bool& result);
  void SetCumGas(const uint64_t& cumGas);
  void AddEntry(const LogEntry& entry);
  const std::string& GetString() const { return m_tranReceiptStr; }
  void SetString(const std::string& tranReceiptStr);
  const uint64_t& GetCumGas() const { return m_cumGas; }
  void clear();
  const Json::Value& GetJsonValue() const { return m_tranReceiptObj; }
  void update();
};

class TransactionWithReceipt : public SerializableDataBlock {
  Transaction m_transaction;
  TransactionReceipt m_tranReceipt;

 public:
  TransactionWithReceipt() = default;
  TransactionWithReceipt(const Transaction& tran,
                         const TransactionReceipt& tranReceipt)
      : m_transaction(tran), m_tranReceipt(tranReceipt) {}
  TransactionWithReceipt(const std::vector<unsigned char>& src,
                         unsigned int offset) {
    Deserialize(src, offset);
  }

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst,
                 unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::vector<unsigned char>& src,
                   unsigned int offset) override;

  const Transaction& GetTransaction() const { return m_transaction; }
  const TransactionReceipt& GetTransactionReceipt() const {
    return m_tranReceipt;
  }

  static TxnHash ComputeTransactionReceiptsHash(
      const std::vector<TransactionWithReceipt>& txrs) {
    if (txrs.empty()) {
      LOG_GENERAL(INFO, "txrs is empty");
      return TxnHash();
    }

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    for (const auto& tr : txrs) {
      sha2.Update(DataConversion::StringToCharArray(
          tr.GetTransactionReceipt().GetString()));
    }
    return TxnHash(sha2.Finalize());
  }

  static bool ComputeTransactionReceiptsHash(
      const std::vector<TxnHash>& txnOrder,
      std::unordered_map<TxnHash, TransactionWithReceipt>& txrs,
      TxnHash& trHash) {
    std::vector<TransactionWithReceipt> vec;

    for (const auto& th : txnOrder) {
      auto it = txrs.find(th);
      if (it == txrs.end()) {
        LOG_GENERAL(WARNING, "Cannot find txnHash "
                                 << th.hex() << " from processedTransactions");
        return false;
      }
      vec.emplace_back(it->second);
    }
    trHash = ComputeTransactionReceiptsHash(vec);
    return true;
  }
};

#endif  // __TRANSACTIONRECEIPT_H__
