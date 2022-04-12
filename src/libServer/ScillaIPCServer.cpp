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

#include <jsonrpccpp/server/connectors/unixdomainsocketserver.h>

#include "ScillaIPCServer.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"

using namespace std;
using namespace Contract;
using namespace jsonrpc;

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
      Procedure("fetchBlockchainInfo", PARAMS_BY_NAME, JSON_STRING,
                "query_name", JSON_STRING, "query_args", JSON_STRING, NULL),
      &ScillaIPCServer::fetchBlockchainInfoI);
}

void ScillaIPCServer::setBCInfoProvider(
    std::unique_ptr<const ScillaBCInfo> &&bcInfo) {
  m_BCInfo = std::move(bcInfo);
}

void ScillaIPCServer::fetchStateValueI(const Json::Value &request,
                                       Json::Value &response) {
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

void ScillaIPCServer::updateStateValueI(const Json::Value &request,
                                        Json::Value &response) {
  if (!updateStateValue(request["query"].asString(),
                        request["value"].asString())) {
    throw JsonRpcException("Updating state value failed");
  }

  // We have nothing to return. A null response is expected in the client.
  response.clear();
}

void ScillaIPCServer::fetchBlockchainInfoI(const Json::Value &request,
                                           Json::Value &response) {
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
  bytes destination;

  if (!ContractStorage::GetContractStorage().FetchStateValue(
          m_BCInfo->getCurContrAddr(), DataConversion::StringToCharArray(query),
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
  bytes destination;

  if (!ContractStorage::GetContractStorage().FetchExternalStateValue(
          m_BCInfo->getCurContrAddr(), Address(addr),
          DataConversion::StringToCharArray(query), 0, destination, 0, found,
          type)) {
    return false;
  }

  value = DataConversion::CharArrayToString(destination);

  return true;
}
bool ScillaIPCServer::updateStateValue(const string &query,
                                       const string &value) {
  return ContractStorage::GetContractStorage().UpdateStateValue(
      m_BCInfo->getCurContrAddr(), DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}

bool ScillaIPCServer::fetchBlockchainInfo(const std::string &query_name,
                                          const std::string &query_args,
                                          std::string &value) {
  if (query_name == "CHAINID") {
    value = std::to_string(CHAIN_ID);
    return true;
  }

  if (query_name == "ORIGIN") {
    value = m_BCInfo->getOriginAddr().hex();
    return true;
  }

  // For queries that include the block number.
  uint64_t blockNum = 0;
  if (query_name == "BLOCKHASH" || query_name == "TIMESTAMP") {
    try {
      blockNum = stoull(query_args);
    } catch (...) {
      LOG_GENERAL(WARNING, "Unable to convert to uint64: " << query_args);
      return false;
    }
  } else {
    blockNum = m_BCInfo->getCurBlockNum();
  }

  TxBlockSharedPtr txBlockSharedPtr;
  if (query_name == "BLOCKHASH" || query_name == "TIMESTAMP" ||
      query_name == "BLOCKCOINBASE" || query_name == "BLOCKTIMESTAMP" || query_name == "BLOCKDIFFICULTY" ||
      query_name == "BLOCKGASLIMIT" || query_name == "BLOCK_BASE_FEE_PER_GAS") {
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum,
                                                    txBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
      return false;
    }
  }

  // TODO: this will always return the value 0 so far, as we need the real DS block.
  blockNum = m_BCInfo->getCurDSBlockNum();
  DSBlockSharedPtr dsBlockSharedPtr;
  if (query_name == "BLOCKCOINBASE" || query_name == "BLOCKDIFFICULTY") {
    if (!BlockStorage::GetBlockStorage().GetDSBlock(blockNum,
                                                    dsBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum DS block " << blockNum);
      return false;
    }
  }

  if (query_name == "BLOCHKASH") {
    value = std::to_string(txBlockSharedPtr->GetBlockHash());
  } else if (query_name == "BLOCKNUMBER") {
    value = std::to_string(blockNum);
  } else if (query_name == "TIMESTAMP" || query_name == "BLOCKTIMESTAMP") {
    value = std::to_string(txBlockSharedPtr->GetTimestamp());
  } else if (query_name == "BLOCKDIFFICULTY") {
    value = "100"  // TODO: implement the real difficlty from DS block
  } else if (query_name == "BLOCKGASLIMIT") {
    value = std::to_string(txBlockSharedPtr->GetGasLimit());
  } else if (query_name == "BLOCK_BASE_FEE_PER_GAS") {
    value = "0"  // TODO: implement the real value (from DS block?)
  } else {
    LOG_GENERAL(WARNING, "Invalid query_name: " << query_name);
    return false;
  }
  return true;
}
