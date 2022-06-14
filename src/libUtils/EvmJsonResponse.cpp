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
#include "common/Constants.h"
#include "depends/websocketpp/websocketpp/base64/base64.hpp"
#include "nlohmann/json.hpp"  // NOLINT(readability-redundant-declaration)

using websocketpp::base64_decode;

namespace evmproj {

evmproj::CallRespose& GetReturn(const Json::Value& oldJason,
                                evmproj::CallRespose& fo) {
  nlohmann::json newJason;

  try {
    newJason = nlohmann::json::parse(oldJason.toStyledString());
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Error parsing json from evmds " << e.what());
    return fo;
  }

  if (LOG_SC) {
    LOG_GENERAL(WARNING, "Response from EVM-DS " << std::endl << newJason);
  }

  try {
    for (const auto& node : newJason.items()) {
      if (node.key() == "apply" && node.value().is_array()) {
        for (const auto& ap : node.value()) {
          for (const auto& map : ap.items()) {
            nlohmann::json arr = map.value();
            std::shared_ptr<ApplyInstructions> apply = std::make_shared<ApplyInstructions>();

            apply->m_operation_type = map.key();
            try {
              apply->m_address = arr["address"];
            } catch (std::exception& e) {
              std::cout << "address : " << e.what() << std::endl;
            }
            try {
              apply->m_balance = arr["balance"];
            } catch (std::exception& e) {
              std::cout << "balance : " << e.what() << std::endl;
            }
            nlohmann::json cobj;
            try {
              cobj = arr["code"];
            } catch (std::exception& e) {
              std::cout << "code : " << e.what() << std::endl;
            }
            if (not cobj.is_null()) {
              if (cobj.is_binary()) {
                std::cout << "Binary data" << std::endl;
              } else if (cobj.is_string()) {
                apply->m_code = cobj.get<std::string>();
              } else {
                std::cout << "write some code for " << cobj.type_name()
                          << std::endl;
              }
            }
            try {
              apply->m_nonce = arr["nonce"];
            } catch (std::exception& e) {
              std::cout << "nonce : " << e.what() << std::endl;
            }
            try {
              apply->m_resetStorage = arr["reset_storage"];
            } catch (std::exception& e) {
              std::cout << "reset : " << e.what() << std::endl;
            }
            nlohmann::json storageObj;
            try {
              storageObj = arr["storage"];
            } catch (std::exception& e) {
              std::cout << "storage : " << e.what() << std::endl;
            }
            if (not storageObj.is_null()) {
              evmproj::KeyValue kvs;
              for (const auto& kv : storageObj.items()) {
                kvs.m_key = base64_decode(kv.value()[0]);
                kvs.m_value = base64_decode(kv.value()[1]);
              }
              apply->m_storage.push_back(kvs);
              fo.m_apply.push_back(apply);
            }
          }
        }
      } else if (node.key() == "exit_reason") {
        for (const auto& er : node.value().items()) {
          try {
            if (er.key() == "Succeed") {
              fo.m_ok = true;
              fo.m_exitReason = er.value();
            } else if (er.key() == "Fatal") {
              fo.m_ok = false;
              fo.m_exitReason = to_string(er.value());
            }
          } catch (std::exception& e) {
            std::cout << "Exception caught on exit_reason" << std::endl;
          }
        }
      } else if (node.key() == "logs") {
        for (const auto& lg : node.value().items()) {
          fo.m_logs = lg.value();
        }
      } else if (node.key() == "return_value") {
        nlohmann::json j = node.value();
        if (j.is_string()) {
          fo.m_return = j;
        } else {
          std::cout << "invalid node type" << std::endl;
        }
      } else if (node.key() == "remaining_gas") {
        fo.m_gasRemaing = node.value();
      }
    }
  } catch (std::exception& e) {
    std::cout << "exception caught in parsing EVM-DS return value : "
              << e.what() << std::endl;
  }
  return fo;
}

std::ostream& operator<<(std::ostream& os, evmproj::KeyValue& kv) {
  os << "key : " << kv.m_key << std::endl;
  os << "value : " << kv.m_value << std::endl;
  return os;
}

std::ostream& operator<<(std::ostream& os, evmproj::ApplyInstructions& evm) {
  os << "operation type : " << evm.m_operation_type << std::endl;
  os << "address : " << evm.m_address << std::endl;
  os << "code : " << evm.m_code << std::endl;
  os << "balance : " << evm.m_balance << std::endl;
  os << "nonce : " << evm.m_nonce << std::endl;

  os << "reset_storage : " << std::boolalpha << evm.m_resetStorage << std::endl;

  for (const auto& it : evm.m_storage) {
    os << "k : " << it.m_key << std::endl;
    os << "v : " << it.m_value << std::endl;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, evmproj::CallRespose& evmRet) {

  const std::shared_ptr<ApplyInstructions> ap;

  for (const auto& it:evmRet.m_apply){
    os << it << std::endl;;
  }

  for (const auto& it : evmRet.Logs()) {
    os << it << std::endl;
  }

  os << evmRet.m_exitReason << std::endl;
  os << "success" << std::boolalpha << evmRet.isSuccess();

  os << "gasRemaing : " << evmRet.Gas() << std::endl;
  os << "code : " << evmRet.ReturnedBytes() << std::endl;
  return os;
}

}  // namespace evmproj
