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

#ifndef __TRANSACTIONRECEIPT_H__
#define __TRANSACTIONRECEIPT_H__

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
  RUNNER_FAILED,
  BALANCE_TRANSFER_FAILED,
  EXECUTE_CMD_FAILED,
  EXECUTE_CMD_TIMEOUT,
  NO_GAS_REMAINING_FOUND,
  NO_ACCEPTED_FOUND,
  CALL_CONTRACT_FAILED,
  CREATE_CONTRACT_FAILED,
  JSON_OUTPUT_CORRUPTED,
  CONTRACT_NOT_EXIST,
  STATE_CORRUPTED,
  LOG_ENTRY_INSTALL_FAILED,
  MESSAGE_CORRUPTED,
  RECEIPT_IS_NULL,
  MAX_DEPTH_REACHED,
  CHAIN_CALL_DIFF_SHARD,
  PREPARATION_FAILED,
  NO_OUTPUT,
  OUTPUT_ILLEGAL
};

class TransactionReceipt : public SerializableDataBlock {
  Json::Value m_tranReceiptObj = Json::nullValue;
  std::string m_tranReceiptStr;
  uint64_t m_cumGas = 0;
  unsigned int m_depth = 0;
  Json::Value m_errorObj;

 public:
  TransactionReceipt();
  bool Serialize(bytes& dst, unsigned int offset) const override;
  bool Deserialize(const bytes& src, unsigned int offset) override;
  void SetResult(const bool& result);
  void AddError(const unsigned int& errCode);
  void AddDepth();
  void InstallError();
  void SetCumGas(const uint64_t& cumGas);
  void SetEpochNum(const uint64_t& epochNum);
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
  TransactionWithReceipt(const bytes& src, unsigned int offset) {
    Deserialize(src, offset);
  }

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

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
        LOG_GENERAL(WARNING, "Missing txnHash " << th);
        return false;
      }
      vec.emplace_back(it->second);
    }
    trHash = ComputeTransactionReceiptsHash(vec);
    return true;
  }
};

#endif  // __TRANSACTIONRECEIPT_H__
