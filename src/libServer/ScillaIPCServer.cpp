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

#include "libPersistence/ContractStorage2.h"
#include "libUtils/DataConversion.h"

#include "ScillaIPCServer.h"

using namespace std;
using namespace Contract;

ScillaIPCServer::ScillaIPCServer(const dev::h160 &contrAddr,
                                 jsonrpc::UnixDomainSocketServer &conn,
                                 jsonrpc::serverVersion_t type)
    : jsonrpc::AbstractServer<ScillaIPCServer>(conn, type),
      m_contrAddr(contrAddr) {
  // These signatures matche that of the actual functions below.
  bindAndAddMethod(
      jsonrpc::Procedure("fetchStateValue", jsonrpc::PARAMS_BY_NAME,
                         jsonrpc::JSON_OBJECT, "query", jsonrpc::JSON_STRING,
                         NULL),
      &ScillaIPCServer::fetchStateValueI);
  bindAndAddMethod(
      jsonrpc::Procedure("updateStateValue", jsonrpc::PARAMS_BY_NAME,
                         jsonrpc::JSON_STRING, "query", jsonrpc::JSON_STRING,
                         "value", jsonrpc::JSON_STRING, NULL),
      &ScillaIPCServer::updateStateValueI);
}

void ScillaIPCServer::fetchStateValueI(const Json::Value &request,
                                       Json::Value &response) {
  std::string value;
  bool found;
  if (!fetchStateValue(request["query"].asString(), value, found)) {
    throw jsonrpc::JsonRpcException("Fetching state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(value));
}

void ScillaIPCServer::updateStateValueI(const Json::Value &request,
                                        Json::Value &response) {
  if (!updateStateValue(request["query"].asString(),
                        request["value"].asString())) {
    throw jsonrpc::JsonRpcException("Updating state value failed");
  }

  // Dummy response because a return value is needed for JSON-RPC.
  response = "";
}

bool ScillaIPCServer::ScillaIPCServer::fetchStateValue(const string &query,
                                                       string &value,
                                                       bool &found) {
  bytes destination;

  if (!ContractStorage2::GetContractStorage().FetchStateValue(
          m_contrAddr, DataConversion::StringToCharArray(query), 0, destination,
          0, found)) {
    return false;
  }

  string value_new = DataConversion::CharArrayToString(destination);
  value.swap(value_new);
  return true;
}

bool ScillaIPCServer::ScillaIPCServer::updateStateValue(const string &query,
                                                        const string &value) {
  return ContractStorage2::GetContractStorage().UpdateStateValue(
      m_contrAddr, DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}
