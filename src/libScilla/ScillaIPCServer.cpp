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

#include "ScillaIPCServer.h"
#include <jsonrpccpp/common/exception.h>
#include <jsonrpccpp/common/specification.h>
#include "ScillaClient.h"
#include "ScillaUtils.h"
#include "libUtils/GasConv.h"

#include "libData/AccountStore/AccountStore.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

#include <sstream>

using namespace std;
using namespace Contract;
using namespace jsonrpc;


// The following base64 encoding/decoding functions are taken from websocketpp
// and taken out of the original websocketpp namespace.
//
// Please see the license and source code at: https://github.com/zaphoyd/websocketpp/blob/master/COPYING
namespace {

std::string const base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

/// Test whether a character is a valid base64 character
/**
 * @param c The character to test
 * @return true if c is a valid base64 character
 */
bool is_base64(unsigned char c) {
    return (c == 43 || // +
           (c >= 47 && c <= 57) || // /-9
           (c >= 65 && c <= 90) || // A-Z
           (c >= 97 && c <= 122)); // a-z
}

/// Encode a char buffer into a base64 string
/**
 * @param input The input data
 * @param len The length of input in bytes
 * @return A base64 encoded string representing input
 */
std::string base64_encode(unsigned char const * input, size_t len) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (len--) {
        char_array_3[i++] = *(input++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; (i <4) ; i++) {
                ret += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++) {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                          ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                          ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; (j < i + 1); j++) {
            ret += base64_chars[char_array_4[j]];
        }

        while((i++ < 3)) {
            ret += '=';
        }
    }

    return ret;
}

/// Encode a string into a base64 string
/**
 * @param input The input data
 * @return A base64 encoded string representing input
 */
std::string base64_encode(std::string const & input) {
    return base64_encode(
        reinterpret_cast<const unsigned char *>(input.data()),
        input.size()
    );
}

/// Decode a base64 encoded string into a string of raw bytes
/**
 * @param input The base64 encoded input data
 * @return A string representing the decoded raw bytes
 */
std::string base64_decode(std::string const & input) {
    size_t in_len = input.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;

    while (in_len-- && ( input[in_] != '=') && is_base64(input[in_])) {
        char_array_4[i++] = input[in_]; in_++;
        if (i ==4) {
            for (i = 0; i <4; i++) {
                char_array_4[i] = static_cast<unsigned char>(base64_chars.find(char_array_4[i]));
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++) {
                ret += char_array_3[i];
            }
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = static_cast<unsigned char>(base64_chars.find(char_array_4[j]));

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; (j < i - 1); j++) {
            ret += static_cast<std::string::value_type>(char_array_3[j]);
        }
    }

    return ret;
}

Z_I64METRIC &GetCallsCounter() {
  static Z_I64METRIC scillaIPCCount(Z_FL::SCILLA_IPC, "scilla_ipc_count",
                                    "Metrics for ScillaIPCServer", "Calls");
  return scillaIPCCount;
}

}  // namespace

ScillaBCInfo::ScillaBCInfo() {
  m_bcInfoCount.SetCallback([this](auto &&result) {
    if (m_bcInfoCount.Enabled()) {
      if (getCurBlockNum() > 0)
        result.Set(getCurBlockNum(), {{"counter", "BlockNumber"}});
      if (getCurDSBlockNum())
        result.Set(getCurDSBlockNum(), {{"counter", "DSBlockNumber"}});
    }
  });
}

void ScillaBCInfo::SetUp(const uint64_t curBlockNum,
                         const uint64_t curDSBlockNum,
                         const Address &originAddr, const Address &curContrAddr,
                         const dev::h256 &rootHash,
                         const uint32_t scillaVersion) {
  m_curBlockNum = curBlockNum;
  m_curDSBlockNum = curDSBlockNum;
  m_curContrAddr = curContrAddr;
  m_originAddr = originAddr;
  m_rootHash = rootHash;
  m_scillaVersion = scillaVersion;
}

ScillaIPCServer::ScillaIPCServer(AccountStore *parent,
                                 AbstractServerConnector &conn)
    : AbstractServer<ScillaIPCServer>(conn, JSONRPC_SERVER_V2),
      m_parent(parent) {
  // These JSON signatures match that of the actual functions below.
  bindAndAddMethod(Procedure("fetchStateValue", PARAMS_BY_NAME, JSON_OBJECT,
                             "query", JSON_STRING, NULL),
                   &ScillaIPCServer::fetchStateValueI);
  bindAndAddMethod(
      Procedure("fetchExternalStateValue", PARAMS_BY_NAME, JSON_OBJECT, "addr",
                JSON_STRING, "query", JSON_STRING, NULL),
      &ScillaIPCServer::fetchExternalStateValueI);
  bindAndAddMethod(Procedure("updateStateValue", PARAMS_BY_NAME, JSON_STRING,
                             "query", JSON_STRING, "value", JSON_STRING, NULL),
                   &ScillaIPCServer::updateStateValueI);
  bindAndAddMethod(
      Procedure("fetchExternalStateValueB64", PARAMS_BY_NAME, JSON_OBJECT,
                "addr", JSON_STRING, "query", JSON_STRING, NULL),
      &ScillaIPCServer::fetchExternalStateValueB64I);

  bindAndAddMethod(Procedure("fetchStateJson", PARAMS_BY_NAME, JSON_OBJECT,
                             "addr", JSON_STRING, "vname", JSON_STRING, NULL),
                   &ScillaIPCServer::fetchStateJsonI);
  bindAndAddMethod(Procedure("fetchCodeJson", PARAMS_BY_NAME, JSON_OBJECT,
                             "addr", JSON_STRING, "query", JSON_STRING, NULL),
                   &ScillaIPCServer::fetchCodeJsonI);

  bindAndAddMethod(
      Procedure("fetchContractInitDataJson", PARAMS_BY_NAME, JSON_OBJECT,
                "addr", JSON_STRING, "query", JSON_STRING, NULL),
      &ScillaIPCServer::fetchContractInitDataJsonI);

  bindAndAddMethod(
      Procedure("fetchBlockchainInfo", PARAMS_BY_NAME, JSON_STRING,
                "query_name", JSON_STRING, "query_args", JSON_STRING, NULL),
      &ScillaIPCServer::fetchBlockchainInfoI);
}

void ScillaIPCServer::setBCInfoProvider(const uint64_t curBlockNum,
                                        const uint64_t curDSBlockNum,
                                        const Address &originAddr,
                                        const Address &curContrAddr,
                                        const dev::h256 &rootHash,
                                        const uint32_t scillaVersion) {
  INC_CALLS(GetCallsCounter());

  m_BCInfo.SetUp(curBlockNum, curDSBlockNum, originAddr, curContrAddr, rootHash,
                 scillaVersion);
}

void ScillaIPCServer::fetchStateValueI(const Json::Value &request,
                                       Json::Value &response) {
  INC_CALLS(GetCallsCounter());

  std::string value;
  bool found;
  if (!fetchStateValue(request["query"].asString(), value, found)) {
    throw JsonRpcException("Fetching state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(value));
}

void ScillaIPCServer::fetchExternalStateValueI(const Json::Value &request,
                                               Json::Value &response) {
  INC_CALLS(GetCallsCounter());

  std::string value, type;
  bool found;
  if (!fetchExternalStateValue(request["addr"].asString(),
                               request["query"].asString(), value, found,
                               type)) {
    throw JsonRpcException("Fetching external state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(value));
  response.append(Json::Value(type));
}

void ScillaIPCServer::fetchExternalStateValueB64I(const Json::Value &request,
                                                  Json::Value &response) {
  INC_CALLS(GetCallsCounter());

  std::string value, type;
  bool found;
  string query = base64_decode(request["query"].asString());
  if (!fetchExternalStateValue(request["addr"].asString(), query, value, found,
                               type)) {
    throw JsonRpcException("Fetching external state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(base64_encode(value)));
  response.append(Json::Value(type));
}

void ScillaIPCServer::updateStateValueI(const Json::Value &request,
                                        Json::Value &response) {
  INC_CALLS(GetCallsCounter());

  if (!updateStateValue(request["query"].asString(),
                        request["value"].asString())) {
    throw JsonRpcException("Updating state value failed");
  }

  // We have nothing to return. A null response is expected in the client.
  response.clear();
}

void ScillaIPCServer::fetchBlockchainInfoI(const Json::Value &request,
                                           Json::Value &response) {
  INC_CALLS(GetCallsCounter());

  std::string value;
  if (!fetchBlockchainInfo(request["query_name"].asString(),
                           request["query_args"].asString(), value)) {
    throw JsonRpcException("Fetching blockchain info failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(true));
  response.append(Json::Value(value));
}

bool ScillaIPCServer::fetchStateValue(const string &query, string &value,
                                      bool &found) {
  INC_CALLS(GetCallsCounter());

  zbytes destination;

  if (!ContractStorage::GetContractStorage().FetchStateValue(
          m_BCInfo.getCurContrAddr(), DataConversion::StringToCharArray(query),
          0, destination, 0, found)) {
    return false;
  }

  string value_new = DataConversion::CharArrayToString(destination);
  value.swap(value_new);
  return true;
}

bool ScillaIPCServer::fetchExternalStateValue(const std::string &addr,
                                              const string &query,
                                              string &value, bool &found,
                                              string &type) {
  INC_CALLS(GetCallsCounter());

  zbytes destination;

  if (!ContractStorage::GetContractStorage().FetchExternalStateValue(
          m_BCInfo.getCurContrAddr(), Address(addr),
          DataConversion::StringToCharArray(query), 0, destination, 0, found,
          type)) {
    return false;
  }

  value = DataConversion::CharArrayToString(destination);

  if (LOG_SC) {
    LOG_GENERAL(WARNING,
                "Request for state val: " << addr << " with query: " << query);
    LOG_GENERAL(WARNING,
                "Resp for state val:    "
                    << DataConversion::Uint8VecToHexStrRet(destination));
  }

  return true;
}

void ScillaIPCServer::fetchStateJsonI(const Json::Value &request,
                                      Json::Value &response) {
  INC_CALLS(GetCallsCounter());
  const auto address = Address{request["addr"].asString()};
  const auto vname = request["vname"].asString();
  if (!request["indices"].isArray()) {
    LOG_GENERAL(WARNING, "Given indices field is not an array!");
    return;
  }
  std::vector<std::string> indicesVector;
  for (const auto &index : request["indices"]) {
    if (!index.isString()) {
      continue;
    }
    std::stringstream ss;
    ss << quoted(index.asString());
    indicesVector.emplace_back(ss.str());
  }

  // Query state also from not committed changes yet
  constexpr bool FROM_TEMP_STATE = true;
  if (!ContractStorage::GetContractStorage().FetchStateJsonForContract(
          response, address, vname, indicesVector, FROM_TEMP_STATE)) {
    LOG_GENERAL(WARNING, "Unable to fetch json state for addr " << address);
  }

  if (LOG_SC) {
    LOG_GENERAL(WARNING,
                "Successfully fetch json substate for addr " << address);
  }
}

void ScillaIPCServer::fetchCodeJsonI(const Json::Value &request,
                                     Json::Value &response) {
  INC_CALLS(GetCallsCounter());
  const auto address = Address{request["addr"].asString()};
  std::string code;
  std::string type;
  bool found = false;

  string query = base64_decode(request["query"].asString());
  if (!fetchExternalStateValue(address.hex(), query, code, found, type)) {
    LOG_GENERAL(WARNING,
                "Unable to query external state with given query: " << query);
    return;
  }
  auto *account = m_parent->GetAccount(address);
  if (account == nullptr) {
    LOG_GENERAL(WARNING,
                "Unable to find account with given address: " << address.hex());
    return;
  }

  std::vector<Address> extlibs;
  bool isLibrary = false;
  uint32_t scillaVersion;
  if (!account->GetContractAuxiliaries(isLibrary, scillaVersion, extlibs)) {
    LOG_GENERAL(WARNING, "Failed to retrieve auxiliaries for contract address: "
                             << address.hex());
    return;
  }
  std::map<Address, std::pair<std::string, std::string>> extlibsExports;
  if (!ScillaUtils::PopulateExtlibsExports(*m_parent, scillaVersion, extlibs,
                                           extlibsExports)) {
    LOG_GENERAL(WARNING, "Unable to populate extlibs for contract address: "
                             << address.hex());
    return;
  }

  std::string rootVersion;
  if (!ScillaUtils::PrepareRootPathWVersion(scillaVersion, rootVersion)) {
    LOG_GENERAL(WARNING, "Can't prepare scilla root path with version");
    return;
  }

  if (!ScillaUtils::ExportCreateContractFiles(
          account->GetCode(), account->GetInitData(), isLibrary, rootVersion,
          scillaVersion, extlibsExports)) {
    LOG_GENERAL(WARNING, "Failed to export contract create files");
    return;
  }
  constexpr auto GAS_LIMIT = std::numeric_limits<uint32_t>::max();
  std::string interprinterPrint;
  const auto callCheckerInput =
      ScillaUtils::GetContractCheckerJson(rootVersion, false, GAS_LIMIT);
  JSONUtils::GetInstance().convertJsontoStr(callCheckerInput);
  if (!ScillaClient::GetInstance().CallChecker(0, callCheckerInput,
                                               interprinterPrint)) {
    return;
  }
  JSONUtils::GetInstance().convertStrtoJson(interprinterPrint, response);
}

void ScillaIPCServer::fetchContractInitDataJsonI(const Json::Value &request,
                                                 Json::Value &response) {
  INC_CALLS(GetCallsCounter());
  const auto address = Address{request["addr"].asString()};
  std::string code;
  std::string type;
  bool found = false;

  string query = base64_decode(request["query"].asString());
  if (!fetchExternalStateValue(address.hex(), query, code, found, type)) {
    LOG_GENERAL(WARNING,
                "Unable to query external state with given query: " << query);
    return;
  }
  auto *account = m_parent->GetAccount(address);
  if (account == nullptr) {
    LOG_GENERAL(WARNING,
                "Unable to find account with given address: " << address.hex());
    return;
  }

  const auto initData = account->GetInitData();
  const auto initDataStr = DataConversion::CharArrayToString(initData);

  JSONUtils::GetInstance().convertStrtoJson(initDataStr, response);
}

bool ScillaIPCServer::updateStateValue(const string &query,
                                       const string &value) {
  INC_CALLS(GetCallsCounter());

  return ContractStorage::GetContractStorage().UpdateStateValue(
      m_BCInfo.getCurContrAddr(), DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}

bool ScillaIPCServer::fetchBlockchainInfo(const std::string &query_name,
                                          const std::string &query_args,
                                          std::string &value) {
  INC_CALLS(GetCallsCounter());

  if (query_name == "BLOCKNUMBER") {
    value = std::to_string(m_BCInfo.getCurBlockNum());
    return true;
  } else if (query_name == "TIMESTAMP" || query_name == "BLOCKHASH") {
    uint64_t blockNum = 0;
    try {
      blockNum = stoull(query_args);
    } catch (...) {
      LOG_GENERAL(WARNING, "Unable to convert to uint64: " << query_args);
      return false;
    }

    TxBlockSharedPtr txBlockSharedPtr;
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum,
                                                    txBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
      return false;
    }

    if (query_name == "TIMESTAMP") {
      value = std::to_string(txBlockSharedPtr->GetTimestamp());
    } else {
      value = txBlockSharedPtr->GetBlockHash().hex();
    }
    return true;
  } else if (query_name == "CHAINID") {
    value = std::to_string(CHAIN_ID);
    return true;
  }

  LOG_GENERAL(WARNING, "Invalid query_name: " << query_name);
  return true;
}
