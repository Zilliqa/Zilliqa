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
  const std::string& Key() const { return m_key; }
  const std::string& Value() const { return m_value; }
  void setKey(const std::string& Key) { m_key = Key; }
  void setValue(const std::string& value) { m_value = value; }

  std::string m_key;
  std::string m_value;

  bool m_hasKey{false};
  bool m_hasValue{false};

  friend std::ostream& operator<<(std::ostream& os, KeyValue& kv);
};

struct ApplyInstructions {
  const std::string& OperationType() const { return m_operation_type; }
  const std::string& Address() const { return m_address; }
  const std::string& Code() const { return m_code; }
  const std::string& Balance() const { return m_balance; }
  const std::string& Nonce() const { return m_nonce; }
  bool isResetStorage() const { return m_resetStorage; }
  const std::vector<KeyValue> Storage() const { return m_storage; }

  std::string m_operation_type;
  std::string m_address;
  std::string m_code;
  std::string m_balance;
  std::string m_nonce;

  bool m_hasBalance{false};
  bool m_hasGas{false};
  bool m_hasNonce{false};
  bool m_hasCode{false};
  bool m_hasAddress{false};

  const bool& hasBalance() { return m_hasBalance; }
  const bool& hasGas() { return m_hasGas; }
  const bool& hasNonce() { return m_hasNonce; }
  const bool& hasCode() { return m_hasCode; }
  const bool& hasAddress() { return m_hasAddress; }

  bool m_resetStorage{false};
  std::vector<KeyValue> m_storage;

  friend std::ostream& operator<<(std::ostream& os, ApplyInstructions& evm);
};

struct CallResponse {
  const std::vector<std::shared_ptr<ApplyInstructions>>& Apply() const {
    return m_apply;
  }
  const std::string Logs() const { return m_logs; }
  const std::string& ExitReason() const { return m_exitReason; }
  uint64_t Gas() const { return m_gasRemaining; }
  const std::string& ReturnedBytes() const { return m_return; }
  bool isSuccess();
  std::vector<std::shared_ptr<ApplyInstructions>> m_apply;
  std::string m_logs;
  bool m_ok{false};
  std::string m_exitReason;
  std::string m_return;
  uint64_t m_gasRemaining{0};

  friend std::ostream& operator<<(std::ostream& os, CallResponse& evmRet);
};

inline bool CallResponse::isSuccess() { return m_ok; }

CallResponse& GetReturn(const Json::Value& oldJason, CallResponse& fo);

}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_
