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

#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"

#include "ScillaIPCServer.h"

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
}

void ScillaIPCServer::setContractAddressVerRoot(const Address &address,
                                                uint32_t version,
                                                const dev::h256 &rootHash) {
  m_contrAddr = address;
  m_version = version;
  m_rootHash = rootHash;
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

bool ScillaIPCServer::fetchStateValue(const string &query, string &value,
                                      bool &found) {
  bytes destination;

  if (!ContractStorage::GetContractStorage().FetchStateValue(
          m_contrAddr, DataConversion::StringToCharArray(query), 0, destination,
          0, found)) {
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
          m_contrAddr, Address(addr), DataConversion::StringToCharArray(query),
          0, destination, 0, found, type)) {
    return false;
  }

  value = DataConversion::CharArrayToString(destination);

  return true;
}
bool ScillaIPCServer::updateStateValue(const string &query,
                                       const string &value) {
  return ContractStorage::GetContractStorage().UpdateStateValue(
      m_contrAddr, DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}
