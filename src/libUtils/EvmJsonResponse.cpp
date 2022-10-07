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
#include "nlohmann/json.hpp"  // NOLINT(readability-redundant-declaration)
#include "websocketpp/base64/base64.hpp"

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
            const nlohmann::json arr = map.value();
            auto apply = std::make_shared<ApplyInstructions>();
            // Read the apply type one of modify or delete
            try {
              apply->SetOperationType(map.key());
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Address : " << e.what());
              throw e;
            }
            // Read the address this is the address of the account we wish to
            // operate on.
            try {
              apply->SetAddress(arr["address"]);
              apply->SetHasAddress(true);
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Address : " << e.what());
              throw e;
            }
            // The new balance for this account
            try {
              if (arr.contains("balance")) {
                apply->SetBalance(arr["balance"]);
                apply->SetHasBalance(true);
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Balance : " << e.what());
              throw e;
            }
            // The new binary code that should be associated with the account
            nlohmann::json cobj;
            try {
              if (arr.contains("code")) {
                cobj = arr["code"];
              }
            } catch (const std::exception& e) {
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
                apply->SetCode(cobj.get<std::string>());
                apply->SetHasCode(true);
              } else {
                LOG_GENERAL(WARNING, "Code sent as Unexpected type ignored");
                throw std::runtime_error(
                    "unhandled DataType used in Code "
                    "value");
              }
            }
            // get the nonce for the account specified in the address
            try {
              if (arr.contains("nonce")) {
                apply->SetNonce(arr["nonce"]);
                apply->SetHasNonce(true);
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading Nonce : " << e.what());
              throw e;
            }
            // whether the storage values for this account should be reset
            try {
              if (arr.contains("reset_storage")) {
                apply->SetResetStorage(arr["reset_storage"]);
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING,
                          "Exception reading reset_storage : " << e.what());
              throw e;
            }
            // The storage object associated with this address
            // the storage object contains an array of key value pairs.
            nlohmann::json storageObj;
            try {
              if (arr.contains("storage")) {
                storageObj = arr["storage"];
              }
            } catch (const std::exception& e) {
              LOG_GENERAL(WARNING, "Exception reading storage : " << e.what());
              throw e;
            }
            if (not storageObj.is_null()) {
              evmproj::KeyValue kvs;
              for (const auto& kv : storageObj.items()) {
                try {
                  kvs.SetKey(base64_decode(kv.value()[0]));
                  kvs.SetHasKey(true);
                } catch (const std::exception& e) {
                  LOG_GENERAL(WARNING,
                              "Exception reading storage key : " << e.what());
                  throw e;
                }

                try {
                  kvs.SetValue(base64_decode(kv.value()[1]));
                  kvs.SetHasValue(true);
                } catch (const std::exception& e) {
                  LOG_GENERAL(WARNING,
                              "Exception reading storage value : " << e.what());
                  throw e;
                }

                // store the keys and values within the storage
                try {
                  apply->AddStorage(kvs);
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
                fo.AddApplyInstruction(apply);
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
            } else if ((er.key() == "Fatal") || (er.key() == "Revert")) {
              fo.SetSuccess(false);
            } else {
              throw std::runtime_error("Unexpected exit reason:" + er.key());
            }
            // exit reason value can be any type  and is converted 'as is' to a
            // string
            if (er.value().is_string()) {
              fo.SetExitReason(er.value());
            } else {
              fo.SetExitReason(to_string(er.value()));
            }
          } catch (const std::exception& e) {
            LOG_GENERAL(WARNING,
                        "Exception reading exit_reason : " << e.what());
            throw e;
          }
        }
      } else if (node.key() == "logs") {
        for (const auto& lg : node.value().items()) {
          try {
            fo.AddLog(to_string(lg.value()));
          } catch (const std::exception& e) {
            LOG_GENERAL(WARNING, "Exception reading logs : " << e.what());
            throw e;
          }
        }
      } else if (node.key() == "return_value") {
        if (node.value().is_string()) {
          const std::string node_value{node.value().get<std::string>()};
          LOG_GENERAL(INFO, "Return value is " << node_value);
          fo.SetReturnedBytes(node_value);
        } else {
          LOG_GENERAL(WARNING, "Error reading return value  : wrong type");
          throw std::runtime_error(
              "Exception assigning code as a string from"
              " value");
        }
      } else if (node.key() == "remaining_gas") {
        try {
          fo.SetGasRemaining(node.value());
        } catch (const std::exception& e) {
          LOG_GENERAL(WARNING,
                      "Exception reading remaining_gas : " << e.what());
          throw e;
        }
      }
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception reading remaining_gas : " << e.what());
    throw e;
  }
  return fo;
}

//
// Debugging routines, allows developer to dump each object directly onto
// the output stream.
//

std::ostream& operator<<(std::ostream& os,
                         const std::vector<std::string>& stringVector) {
  os << "{";
  for (const auto& it : stringVector) {
    os << it << ",";
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os, const evmproj::KeyValue& kv) {
  os << "{key:" << kv.Key() << ", value:" << kv.Value() << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const std::vector<KeyValue>& storage) {
  os << "{";
  for (const auto& it : storage) {
    os << it << ",";
  }
  os << "}";
  return os;
}

std::ostream& operator<<(
    std::ostream& os,
    const std::vector<std::shared_ptr<ApplyInstructions>>& applyInstructions) {
  os << "{";
  for (const auto& applyInstruction : applyInstructions) {
    os << "{OperationType:" << applyInstruction->OperationType()  //
       << ", Address:" << applyInstruction->Address()             //
       << ", Code:" << applyInstruction->Code()                   //
       << ", Balance:" << applyInstruction->Balance()             //
       << ", Nonce:" << applyInstruction->Nonce()                 //
       << ", ResetStorage:" << std::boolalpha
       << applyInstruction->isResetStorage()  //
       << ", Storage:" << applyInstruction->Storage() << "},";
  }
  os << "}";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const evmproj::CallResponse& evmRet) {
  os << "ApplyInstructions:" << evmRet.GetApplyInstructions()  //
     << ", Logs:" << evmRet.Logs()                             //
     << ", Success:" << std::boolalpha << evmRet.Success()     //
     << ", ExitReason:" << evmRet.ExitReason()                 //
     << ", GasRemaining:" << evmRet.Gas()                      //
     << ", Code:" << evmRet.ReturnedBytes();
  return os;
}

}  // namespace evmproj
