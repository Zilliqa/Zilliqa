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

#include "libUtils/EvmJsonResponse.h"
#include "depends/websocketpp/websocketpp/base64/base64.hpp"

#include "nlohmann/json.hpp"

using websocketpp::base64_decode;

EvmReturn &GetReturn(const Json::Value &oldJason, EvmReturn &fo) {
  nlohmann::json newJason;

  try {
    newJason = nlohmann::json::parse(oldJason.toStyledString());
  } catch (std::exception &e) {
    std::cout << "Error parsing json from evmds " << e.what() << std::endl;
    return fo;
  }

  for (const auto &node : newJason.items()) {
    if (node.key() == "apply" && node.value().is_array()) {
      for (const auto &ap : node.value()) {
        for (const auto &map : ap.items()) {
          EvmOperation op;
          nlohmann::json arr = map.value();
          op._operation_type = map.key();
          try {
            op._address = arr["address"];
          } catch (std::exception &e) {
            std::cout << "address : " << e.what() << std::endl;
          }
          try {
            op._balance = arr["balance"];
          } catch (std::exception &e) {
            std::cout << "balance : " << e.what() << std::endl;
          }
          nlohmann::json cobj;
          try {
            cobj = arr["code"];
          } catch (std::exception &e) {
            std::cout << "code : " << e.what() << std::endl;
          }
          if (not cobj.is_null()) {
            if (cobj.is_binary()) {
              std::cout << "Binary data" << std::endl;
            } else if (cobj.is_string()) {
              op._code = cobj.get<std::string>();
            } else {
              std::cout << "write some code for " << cobj.type_name()
                        << std::endl;
            }
          }
          try {
            op._nonce = arr["nonce"];
          } catch (std::exception &e) {
            std::cout << "nonce : " << e.what() << std::endl;
          }
          try {
            op._reset_storage = arr["reset_storage"];
          } catch (std::exception &e) {
            std::cout << "reset : " << e.what() << std::endl;
          }
          nlohmann::json storageObj;
          try {
            storageObj = arr["storage"];
          } catch (std::exception &e) {
            std::cout << "storage : " << e.what() << std::endl;
          }
          if (not storageObj.is_null()) {
            KeyValue kvs;
            for (const auto &kv : storageObj.items()) {
              kvs._key = base64_decode(kv.value()[0]);
              kvs._value = base64_decode(kv.value()[1]);
            }
            op._storage.push_back(kvs);
          }
          fo._operations.push_back(op);
        }
      }
    } else if (node.key() == "exit_reason") {
      for (const auto &er : node.value().items()) {
        fo._exit_reasons.push_back(er.key());
        fo._exit_reasons.push_back(er.value());
      }
    } else if (node.key() == "logs") {
      for (const auto &lg : node.value().items()) {
        fo._logs.push_back(lg.value());
      }
    } else if (node.key() == "return_value") {
      nlohmann::json j = node.value();
      if (j.is_string()) {
        fo._return = j;
      } else {
        std::cout << "invalid node type" << std::endl;
      }
    } else if (node.key() == "remaining_gas") {
      fo._gasRemaing = node.value();
    }
  }
  return fo;
}

std::ostream &operator<<(std::ostream &os, KeyValue &kv) {
  os << "key : " << kv._key << std::endl;
  os << "value : " << kv._value << std::endl;
  return os;
}

std::ostream &operator<<(std::ostream &os, EvmOperation &evm) {
  os << "operation type : " << evm._operation_type << std::endl;
  os << "address : " << evm._address << std::endl;
  os << "code : " << evm._code << std::endl;
  os << "balance : " << evm._balance << std::endl;
  os << "nonce : " << evm._nonce << std::endl;

  os << "reset_storage : " << std::boolalpha << evm._reset_storage << std::endl;

  for (const auto &it : evm._storage) {
    os << "k : " << it._key << std::endl;
    os << "v : " << it._value << std::endl;
  }
  return os;
}

std::ostream &operator<<(std::ostream &os, EvmReturn &evmret) {
  os << "EvmReturn object" << std::endl;

  for (auto it : evmret._operations) {
    os << it << std::endl;
  }
  for (const auto &it : evmret._logs) {
    os << it << std::endl;
  }
  for (const auto &it : evmret._exit_reasons) {
    os << it << std::endl;
  }

  os << "gasRemaing : " << evmret._gasRemaing << std::endl;
  os << "code : " << evmret._return << std::endl;
  return os;
}
