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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONRECEIPT_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONRECEIPT_H_

#include <json/json.h>

#include <unordered_map>

#include "LogEntry.h"
#include "Transaction.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

enum ReceiptError : unsigned int {
  CHECKER_FAILED = 0,
  RUNNER_FAILED = 1,
  BALANCE_TRANSFER_FAILED = 2,
  EXECUTE_CMD_FAILED = 3,
  EXECUTE_CMD_TIMEOUT = 4,
  NO_GAS_REMAINING_FOUND = 5,
  NO_ACCEPTED_FOUND = 6,
  CALL_CONTRACT_FAILED = 7,
  CREATE_CONTRACT_FAILED = 8,
  JSON_OUTPUT_CORRUPTED = 9,
  CONTRACT_NOT_EXIST = 10,
  STATE_CORRUPTED = 11,
  LOG_ENTRY_INSTALL_FAILED = 12,
  MESSAGE_CORRUPTED = 13,
  RECEIPT_IS_NULL = 14,
  MAX_EDGES_REACHED = 15,
  CHAIN_CALL_DIFF_SHARD = 16,
  PREPARATION_FAILED = 17,
  NO_OUTPUT = 18,
  OUTPUT_ILLEGAL = 19,
  MAP_DEPTH_MISSING = 20,
  GAS_NOT_SUFFICIENT = 21,
  INTERNAL_ERROR = 22,
  LIBRARY_AS_RECIPIENT = 23,
  VERSION_INCONSISTENT = 24,
  LIBRARY_EXTRACTION_FAILED = 25
};

class TransactionReceipt : public SerializableDataBlock {
  Json::Value m_tranReceiptObj = Json::nullValue;
  std::string m_tranReceiptStr;
  uint64_t m_cumGas = 0;
  unsigned int m_edge = 0;
  Json::Value m_errorObj;

 public:
  TransactionReceipt();
  bool Serialize(bytes& dst, unsigned int offset) const override;
  bool Deserialize(const bytes& src, unsigned int offset) override;
  bool Deserialize(const std::string& src, unsigned int offset) override;
  void SetResult(const bool& result);
  void AddError(const unsigned int& errCode);
  void AddException(const Json::Value& jsonException);
  void AddEdge();
  void InstallError();
  void SetCumGas(const uint64_t& cumGas);
  void SetEpochNum(const uint64_t& epochNum);
  void AddEntry(const LogEntry& entry);
  void AddTransition(const Address& addr, const Json::Value& transition,
                     uint32_t tree_depth);
  void AddAccepted(bool accepted);
  bool AddAcceptedForLastTransition(bool accepted);
  void RemoveAllTransitions();
  void CleanEntry();
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
  TransactionWithReceipt(const bytes& src, unsigned int offset) {
    Deserialize(src, offset);
  }

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

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

    SHA2<HashType::HASH_VARIANT_256> sha2;
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
        LOG_GENERAL(WARNING, "Missing txnHash " << th);
        return false;
      }
      vec.emplace_back(it->second);
    }
    trHash = ComputeTransactionReceiptsHash(vec);
    return true;
  }
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONRECEIPT_H_
