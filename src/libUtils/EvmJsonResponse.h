/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_
#define ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_

#include <iostream>
#include <ostream>
#include <vector>
#include "libUtils/JsonUtils.h"

namespace evmproj {

struct KeyValue {
 public:
  const std::string& Key() const { return m_key; }
  void SetKey(const std::string& Key) { m_key = Key; }
  void SetHasKey(const bool value) { m_hasKey = value; }

  const std::string& Value() const { return m_value; }
  void SetValue(const std::string& value) { m_value = value; }
  void SetHasValue(const bool value) { m_hasValue = value; }

 private:
  std::string m_key;
  std::string m_value;

  bool m_hasKey{false};
  bool m_hasValue{false};
};

struct ApplyInstructions {
 public:
  const std::string& OperationType() const { return m_operation_type; }
  void SetOperationType(const std::string& operationType) {
    m_operation_type = operationType;
  }

  const std::string& Address() const { return m_address; }
  void SetAddress(const std::string& address) { m_address = address; }

  const std::string& Balance() const { return m_balance; }
  void SetBalance(const std::string& balance) { m_balance = balance; }

  bool isResetStorage() const { return m_resetStorage; }
  void SetResetStorage(const bool resetStorage) {
    m_resetStorage = resetStorage;
  }

  const std::vector<KeyValue>& Storage() const { return m_storage; }
  void AddStorage(const KeyValue& keyValue) {
    return m_storage.push_back(keyValue);
  }

  bool hasBalance() const { return m_hasBalance; }
  void SetHasBalance(const bool hasBalance) { m_hasBalance = hasBalance; }

  const std::string& Nonce() const { return m_nonce; }
  void SetNonce(const std::string& nonce) { m_nonce = nonce; }
  bool hasNonce() const { return m_hasNonce; }
  void SetHasNonce(const bool hasNonce) { m_hasNonce = hasNonce; };

  const std::string& Code() const { return m_code; }
  void SetCode(const std::string& code) { m_code = code; }
  bool hasCode() const { return m_hasCode; }
  void SetHasCode(const bool hasCode) { m_hasCode = hasCode; }

  bool hasAddress() const { return m_hasAddress; }
  void SetHasAddress(const bool hasAddress) { m_hasAddress = hasAddress; }

 private:
  std::string m_operation_type;
  std::string m_address;
  std::string m_code;
  std::string m_balance;
  std::string m_nonce;

  bool m_hasBalance{false};
  bool m_hasNonce{false};
  bool m_hasCode{false};
  bool m_hasAddress{false};

  bool m_resetStorage{false};
  std::vector<KeyValue> m_storage;
};

struct CallResponse {
 public:
  const std::vector<std::shared_ptr<ApplyInstructions>>& GetApplyInstructions()
      const {
    return m_apply;
  }

  void AddApplyInstruction(const std::shared_ptr<ApplyInstructions>& apply) {
    m_apply.push_back(apply);
  }

  const std::vector<std::string>& Logs() const { return m_logs; }
  void AddLog(const std::string& log) { m_logs.push_back(log); }

  const std::string& ExitReason() const { return m_exitReason; }
  void SetExitReason(const std::string& reason) { m_exitReason = reason; }

  uint64_t Gas() const { return m_gasRemaining; }
  void SetGasRemaining(const uint64_t gas) { m_gasRemaining = gas; }

  const std::string& ReturnedBytes() const { return m_return; }
  void SetReturnedBytes(const std::string& bytes) { m_return = bytes; }

  inline bool Success() const { return m_success; }
  inline void SetSuccess(const bool _ok) { m_success = _ok; }

 private:
  bool m_success{false};
  std::vector<std::shared_ptr<ApplyInstructions>> m_apply;
  std::vector<std::string> m_logs;

  std::string m_exitReason;
  std::string m_return;
  uint64_t m_gasRemaining{0};
};

CallResponse& GetReturn(const Json::Value& oldJson, CallResponse& fo);

std::ostream& operator<<(std::ostream& os, const KeyValue& kv);
std::ostream& operator<<(
    std::ostream& os,
    const std::vector<std::shared_ptr<ApplyInstructions>>& applyInstructions);
std::ostream& operator<<(std::ostream& os,
                         const std::vector<KeyValue>& storage);
std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::string>& strings);
std::ostream& operator<<(std::ostream& os, const CallResponse& evmRet);

}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_
