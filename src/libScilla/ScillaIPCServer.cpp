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
#include "websocketpp/base64/base64.hpp"

#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

#include <sstream>

using namespace std;
using namespace Contract;
using namespace jsonrpc;

using websocketpp::base64_decode;
using websocketpp::base64_encode;

namespace {

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

ScillaIPCServer::ScillaIPCServer(AbstractServerConnector &conn)
    : AbstractServer<ScillaIPCServer>(conn, JSONRPC_SERVER_V2) {
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

  if (!ContractStorage::GetContractStorage().FetchStateJsonForContract(
          response, address, vname, indicesVector)) {
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
    return;
  }
  std::string rootVersion;
  if (!ScillaUtils::PrepareRootPathWVersion(0, rootVersion)) {
    return;
  }
  constexpr auto GAS_LIMIT = std::numeric_limits<uint32_t>::max();
  const auto checkerJson =
      ScillaUtils::GetContractCheckerJson(rootVersion, false, GAS_LIMIT);
  std::string interprinterPrint;
  if (!ScillaClient::GetInstance().CallChecker(
          0, ScillaUtils::GetContractCheckerJson(rootVersion, false, GAS_LIMIT),
          interprinterPrint)) {
    return;
  }
  JSONUtils::GetInstance().convertStrtoJson(interprinterPrint, response);
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
