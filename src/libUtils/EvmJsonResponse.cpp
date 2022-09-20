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

/* GetReturn
 * This method converts a Json message into a C++ response tree
 * the objective of this layer is to separate the concern of JSON from the
 * application code
 *
 * returns a populated CallResponse object
 *
 * throws std::exception or the original exception is passed up to the caller.
 * */

evmproj::CallResponse& GetReturn(const Json::Value& oldJson,
                                 evmproj::CallResponse& fo) {
  nlohmann::json newJson;

  try {
    Json::FastWriter writer;

    newJson = nlohmann::json::parse(writer.write(oldJson));
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Exception JSONRPC parser to nlohmann parser " << e.what())
    throw e;
  }

  if (LOG_SC) {
    LOG_GENERAL(WARNING, "Response from EVM-DS " << std::endl << newJson);
  }

  try {
    for (const auto& node : newJson.items()) {
      if (node.key() == "apply" && node.value().is_array()) {
        for (const auto& ap : node.value()) {
          for (const auto& map : ap.items()) {
            nlohmann::json arr = map.value();
            std::shared_ptr<ApplyInstructions> apply =
                std::make_shared<ApplyInstructions>();
            // Read the apply type one of modify or delete
            try {
              apply->m_operation_type = map.key();
            } catch (std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Address : " << e.what());
              throw e;
            }
            // Read the address this is the address of the account we wish to
            // operate on.
            try {
              apply->m_address = arr["address"];
              apply->m_hasAddress = true;
            } catch (std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Address : " << e.what());
              throw e;
            }
            // The new balance for this account
            try {
              apply->m_balance = arr["balance"];
              apply->m_hasBalance = true;
            } catch (std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Balance : " << e.what());
              throw e;
            }
            // The new binary code that should be associated with the account
            nlohmann::json cobj;
            try {
              cobj = arr["code"];
            } catch (std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Code : " << e.what());
              throw e;
            }
            // check the type, we only support strings in this version
            // we expect it t have been encoded into ascii hex, other
            // components are not happy with nulls in the contents.
            if (not cobj.is_null()) {
              if (cobj.is_binary()) {
                LOG_GENERAL(WARNING, "Code sent as Binary type ignored");
                throw std::runtime_error(
                    "unhandled DataType Binary used in "
                    "Code ");
              } else if (cobj.is_string()) {
                apply->m_code = cobj.get<std::string>();
                apply->m_hasCode = true;
              } else {
                LOG_GENERAL(WARNING, "Code sent as Unexpected type ignored");
                throw std::runtime_error(
                    "unhandled DataType used in Code "
                    "value");
              }
            }
            // get the nonce for the account specified in the address
            try {
              apply->m_nonce = arr["nonce"];
              apply->m_hasNonce = true;
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Nonce : " << e.what());
              throw e;
            }
            // whether the storage values for this account should be reset
            try {
              apply->m_resetStorage = arr["reset_storage"];
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING,
                          "Exception reading reset_storage : " << e.what());
              throw e;
            }
            // The storage object associated with this address
            // the storage object contains an array of key value pairs.
            nlohmann::json storageObj;
            try {
              storageObj = arr["storage"];
            } catch (std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading storage : " << e.what());
              throw e;
            }
            if (not storageObj.is_null()) {
              evmproj::KeyValue kvs;
              for (const auto& kv : storageObj.items()) {
                try {
                  kvs.m_key = base64_decode(kv.value()[0]);
                  kvs.m_hasKey = true;
                } catch (std::exception& e) {
                  LOG_GENERAL(WARNING,
                              "Exception reading storage key : " << e.what());
                  throw e;
                }
                try {
                  kvs.m_value = base64_decode(kv.value()[1]);
                  kvs.m_hasValue = true;
                } catch (const std::exception& e) {
                  LOG_GENERAL(WARNING,
                              "Exception reading storage value : " << e.what());
                  throw e;
                }
                // store the keys and values within the storage
                try {
                  apply->m_storage.push_back(kvs);
                } catch (const std::exception& e) {
                  LOG_GENERAL(WARNING,
                              "Exception adding key/value pair to storage : "
                                  << e.what());
                  throw e;
                }
              }
              //
              // store the apply instruction within the response.
              //
              try {
                fo.m_apply.push_back(apply);
              } catch (const std::exception& e) {
                LOG_GENERAL(WARNING, "Exception adding apply to response : "
                                         << e.what());
                throw e;
              }
            }
          }
        }
      } else if (node.key() == "exit_reason") {
        for (const auto& er : node.value().items()) {
          try {
            if (er.key() == "Succeed") {
              fo.SetSuccess(true);
              fo.m_exitReason = er.value();
            } else if (er.key() == "Fatal") {
              fo.SetSuccess(false);
              fo.m_exitReason = er.value();
            }
          } catch (std::exception& e) {
            LOG_GENERAL(WARNING,
                        "Exception reading exit_reason : " << e.what());
            throw e;
          }
        }
      } else if (node.key() == "logs") {
        for (const auto& lg : node.value().items()) {
          try {
            fo.m_logs.push_back(to_string(lg.value()));
          } catch (std::exception& e) {
            LOG_GENERAL(WARNING, "Exception reading logs : " << e.what());
            throw e;
          }
        }
      } else if (node.key() == "return_value") {
        if (node.value().is_string()) {
          std::string node_value{node.value().get<std::string>()};
          LOG_GENERAL(INFO, "Return value is " << node_value);
          fo.m_return.assign(node_value);
        } else {
          LOG_GENERAL(WARNING, "Error reading return value  : wrong type");
          throw std::runtime_error(
              "Exception assigning code as a string from"
              " value");
        }
      } else if (node.key() == "remaining_gas") {
        try {
          fo.m_gasRemaining = node.value();
        } catch (std::exception& e) {
          LOG_GENERAL(WARNING,
                      "Exception reading remaining_gas : " << e.what());
          throw e;
        }
      }
    }
  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Exception reading remaining_gas : " << e.what());
    throw e;
  }
  return fo;
}

//
// Debugging routines, allows developer to dump each object directly onto
// the output stream.
//

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

std::ostream& operator<<(std::ostream& os, evmproj::CallResponse& evmRet) {
  const std::shared_ptr<ApplyInstructions> ap;

  for (const auto& it : evmRet.m_apply) {
    os << it << std::endl;
    ;
  }

  for (const auto& it : evmRet.Logs()) {
    os << it << std::endl;
  }

  os << evmRet.m_exitReason << std::endl;
  os << "success" << std::boolalpha << evmRet.GetSuccess();

  os << "gasRemaining : " << evmRet.Gas() << std::endl;
  os << "code : " << evmRet.ReturnedBytes() << std::endl;
  return os;
}

}  // namespace evmproj
