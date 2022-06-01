/*
 * Copyright (C) 202 Zilliqa
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
  std::string _key;
  std::string _value;

  friend std::ostream& operator<<(std::ostream &os, KeyValue &kv);
};

struct ApplyInstructions {
  std::string _operation_type;
  std::string _address;
  std::string _code;
  std::string _balance;
  std::string _nonce;
  bool _reset_storage{false};
  std::vector<KeyValue> _storage;

  friend std::ostream& operator<<(std::ostream &os, ApplyInstructions &evm);
};

struct CallRespose {
  ApplyInstructions _apply;
  std::string _logs;
  std::vector<std::string> _exit_reasons;
  std::string _return;
  uint64_t _gasRemaing{0};
  friend std::ostream& operator<<(std::ostream &os, CallRespose &evmret);
};

CallRespose &GetReturn(const Json::Value &oldJason, CallRespose &fo);

}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBUTILS_EVMJSONRESPONSE_H_
