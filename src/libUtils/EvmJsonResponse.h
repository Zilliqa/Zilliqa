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
  bool m_resetStorage{false};
  std::vector<KeyValue> m_storage;

  friend std::ostream& operator<<(std::ostream& os, ApplyInstructions& evm);
};

struct CallRespose {
  const ApplyInstructions& Apply() const { return m_apply; }
  const std::string& Logs() const { return m_logs; }
  const std::vector<std::string> ExitReasons() const { return m_exitReasons; }
  uint64_t Gas() const { return m_gasRemaing; }
  const std::string& ReturnedBytes() const { return m_return; }
  ApplyInstructions m_apply;
  std::string m_logs;
  std::vector<std::string> m_exitReasons;
  std::string m_return;
  uint64_t m_gasRemaing{0};

  friend std::ostream& operator<<(std::ostream& os, CallRespose& evmRet);
};

CallRespose& GetReturn(const Json::Value& oldJason, CallRespose& fo);

}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_
