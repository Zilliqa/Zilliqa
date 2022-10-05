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
#include <sstream>
#include "common/Constants.h"
#include "libUtils/GasConv.h"
#include "websocketpp/base64/base64.hpp"

#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"

#include "ScillaIPCServer.h"

using namespace std;
using namespace Contract;
using namespace jsonrpc;

using websocketpp::base64_decode;
using websocketpp::base64_encode;

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

  // TODO @CSideSteve.
  // There is a bug in the method below that leads to a crash. This is why it is
  // commented out. To reproduce the bug, uncomment the below, rebuild, and run
  // ds_test against the isolated server as
  //      pytest -k test_bcinfo
  //
   bindAndAddMethod(
       Procedure("fetchBlockchainInfo", PARAMS_BY_NAME, JSON_STRING,
                 "query_name", JSON_STRING, "query_args", JSON_STRING, NULL),
       &ScillaIPCServer::fetchBlockchainInfoI);
}

void ScillaIPCServer::setBCInfoProvider(const ScillaBCInfo &bcInfo) {
  m_BCInfo = bcInfo;
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

void ScillaIPCServer::fetchExternalStateValueB64I(const Json::Value &request,
                                                  Json::Value &response) {
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
  bytes destination;

  if (!ContractStorage::GetContractStorage().FetchExternalStateValue(
          m_BCInfo.getCurContrAddr(), Address(addr),
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
      m_BCInfo.getCurContrAddr(), DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}

bool ScillaIPCServer::fetchBlockchainInfo(const std::string &query_name,
                                          const std::string &query_args,
                                          std::string &value) {
  if (query_name == "BLOCKNUMBER") {
    value = std::to_string(m_BCInfo.getCurBlockNum());
    return true;
  } else if (query_name == "TIMESTAMP") {
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

    value = std::to_string(txBlockSharedPtr->GetTimestamp());
    return true;
  } else if (query_name == "CHAINID") {
    value = std::to_string(CHAIN_ID);
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
    blockNum = m_BCInfo.getCurBlockNum();
    if (blockNum > 0) {
      // We need to look at the previous block,
      // as the current block is incomplete at the moment
      // of transaction execution. It is complete at eth_call time,
      // but still look at previous block to keep behavior consistent.
      blockNum -= 1;
    }
  }

  TxBlockSharedPtr txBlockSharedPtr;
  if (query_name == "BLOCKCOINBASE" || query_name == "BLOCKTIMESTAMP" ||
      query_name == "BLOCKDIFFICULTY" || query_name == "BLOCKGASLIMIT") {
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum,
                                                    txBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
      return false;
    }
  }

  // TODO: this will always return the value 0 so far, as we need the real DS
  // block.
  blockNum = m_BCInfo.getCurDSBlockNum();
  DSBlockSharedPtr dsBlockSharedPtr;
  if (query_name == "BLOCKCOINBASE" || query_name == "BLOCKDIFFICULTY" ||
      query_name == "BLOCKGASPRICE") {
    if (!BlockStorage::GetBlockStorage().GetDSBlock(blockNum,
                                                    dsBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum DS block " << blockNum);
      return false;
    }
  }

  if (query_name == "BLOCKHASH") {
    value = txBlockSharedPtr->GetBlockHash().hex();
  } else if (query_name == "BLOCKNUMBER") {
    value = std::to_string(blockNum);
  } else if (query_name == "BLOCKTIMESTAMP") {
    value = std::to_string(txBlockSharedPtr->GetTimestamp() /
                           1000000);  // in seconds
  } else if (query_name == "BLOCKDIFFICULTY") {
    value = std::to_string(dsBlockSharedPtr->GetHeader().GetDifficulty());
  } else if (query_name == "BLOCKGASLIMIT") {
    value = std::to_string(GasConv::GasUnitsFromCoreToEth(
        txBlockSharedPtr->GetHeader().GetGasLimit()));
  } else if (query_name == "BLOCKGASPRICE") {
    uint256_t gasPrice =
        (dsBlockSharedPtr->GetHeader().GetGasPrice() * EVM_ZIL_SCALING_FACTOR) /
            GasConv::GetScalingFactor() +
        EVM_ZIL_SCALING_FACTOR;
    std::ostringstream s;
    s << gasPrice;
    value = s.str();
  } else {
    LOG_GENERAL(WARNING, "Invalid query_name: " << query_name);
    return false;
  }
  return true;
}
