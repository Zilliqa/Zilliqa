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

#include <unordered_map>

#include "LogEntry.h"
#include "Transaction.h"

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
  LIBRARY_EXTRACTION_FAILED = 25,
  MAX_LEN
};

// This is a perfect case for magic enum, however g3log messed up some macros
// disallowing magic enum to work...

namespace TransactionReceiptStr {

#define MAKE_RECEIPT_ERR_AS_STR(s) #s

constexpr std::array<std::string_view, static_cast<uint8_t>(MAX_LEN)>
    TransactionReceiptErrorStr = {
        MAKE_RECEIPT_ERR_AS_STR(CHECKER_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(RUNNER_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(BALANCE_TRANSFER_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(EXECUTE_CMD_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(EXECUTE_CMD_TIMEOUT),
        MAKE_RECEIPT_ERR_AS_STR(NO_GAS_REMAINING_FOUND),
        MAKE_RECEIPT_ERR_AS_STR(NO_ACCEPTED_FOUND),
        MAKE_RECEIPT_ERR_AS_STR(CALL_CONTRACT_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(CREATE_CONTRACT_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(JSON_OUTPUT_CORRUPTED),
        MAKE_RECEIPT_ERR_AS_STR(CONTRACT_NOT_EXIST),
        MAKE_RECEIPT_ERR_AS_STR(STATE_CORRUPTED),
        MAKE_RECEIPT_ERR_AS_STR(LOG_ENTRY_INSTALL_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(MESSAGE_CORRUPTED),
        MAKE_RECEIPT_ERR_AS_STR(RECEIPT_IS_NULL),
        MAKE_RECEIPT_ERR_AS_STR(MAX_EDGES_REACHED),
        MAKE_RECEIPT_ERR_AS_STR(CHAIN_CALL_DIFF_SHARD),
        MAKE_RECEIPT_ERR_AS_STR(PREPARATION_FAILED),
        MAKE_RECEIPT_ERR_AS_STR(NO_OUTPUT),
        MAKE_RECEIPT_ERR_AS_STR(OUTPUT_ILLEGAL),
        MAKE_RECEIPT_ERR_AS_STR(MAP_DEPTH_MISSING),
        MAKE_RECEIPT_ERR_AS_STR(GAS_NOT_SUFFICIENT),
        MAKE_RECEIPT_ERR_AS_STR(INTERNAL_ERROR),
        MAKE_RECEIPT_ERR_AS_STR(LIBRARY_AS_RECIPIENT),
        MAKE_RECEIPT_ERR_AS_STR(VERSION_INCONSISTENT),
        MAKE_RECEIPT_ERR_AS_STR(LIBRARY_EXTRACTION_FAILED)};

}  // namespace TransactionReceiptStr

class TransactionReceipt : public SerializableDataBlock {
  Json::Value m_tranReceiptObj = Json::nullValue;
  std::string m_tranReceiptStr;
  uint64_t m_cumGas = 0;
  unsigned int m_edge = 0;
  Json::Value m_errorObj;

 public:
  TransactionReceipt();
  bool Serialize(zbytes& dst, unsigned int offset) const override;
  bool Deserialize(const zbytes& src, unsigned int offset) override;
  bool Deserialize(const std::string& src, unsigned int offset) override;
  void SetResult(const bool& result);
  void AddError(const unsigned int& errCode);
  void AddException(const Json::Value& jsonException);
  void AddEdge();
  void InstallError();
  void SetCumGas(const uint64_t& cumGas);
  void SetEpochNum(const uint64_t& epochNum);
  void AddLogEntry(const LogEntry& entry);
  void AddJsonEntry(const Json::Value& obj);
  void AppendJsonEntry(const Json::Value& obj);
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
  TransactionWithReceipt(const zbytes& src, unsigned int offset) {
    Deserialize(src, offset);
  }

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

  const Transaction& GetTransaction() const { return m_transaction; }
  const TransactionReceipt& GetTransactionReceipt() const {
    return m_tranReceipt;
  }

  static TxnHash ComputeTransactionReceiptsHash(
      const std::vector<TransactionWithReceipt>& txrs);

  static bool ComputeTransactionReceiptsHash(
      const std::vector<TxnHash>& txnOrder,
      std::unordered_map<TxnHash, TransactionWithReceipt>& txrs,
      TxnHash& trHash);
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONRECEIPT_H_
