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
#include "libPersistence/ContractStorage2.h"
#include "libUtils/DataConversion.h"

using namespace jsonrpc;
using namespace std;

ScillaIPCServer::ScillaIPCServer(const string &unixdomainsocket_path, const dev::h160 &contract_address) :
                                 AbstractServer<ScillaIPCServer>(
                                 UnixDomainSocketServer server(unixdomainsocket_path),
                                 JSONRPC_SERVER_V2) {

  this->bindAndAddMethod(Procedure("fetchStateValue", PARAMS_BY_NAME, JSON_BOOLEAN, 
                                   "query", JSON_STRING, "value", JSON_STRING, NULL), 
                                   &ScillaIPCServer::fetchStateValueI);

  this->bindAndAddMethod(Procedure("updateStateValue", PARAMS_BY_NAME,
                                   "query", JSON_STRING, "value", JSON_STRING, NULL), 
                                   &ScillaIPCServer::updateStateValueI);

  this->bindAndAddMethod(Procedure("testServerRPC", PARAMS_BY_NAME,JSON_STRING,
                                  "query", JSON_STRING, "value", JSON_STRING, NULL),
                                  &ScillaIPCServer::testServerRPCI);

  this->contract_address = contract_address;

}

void setContractAddress(const dev::h160 &address) {
  this->contract_address = address;
}

dev::h160 getContractAddress() {
  return this->contract_address;
}

void ScillaIPCServer::updateStateValue(const string &query, const string &value) {
  ContractStorage2::UpdateStateValue(this->contract_address, DataConversion::StringToCharArray(query),
                                     0, DataConversion:: StringToCharArray(value), 0);
}
bool ScillaIPCServer::fetchStateValue(const string &query, const string &value) {
  return ContractStorage2::FetchStateValue(this->contract_address, DataConversion::StringToCharArray(query),
                                           0, DataConversion:: StringToCharArray(value), 0);
}

bool ScillaIPCServer::testServer() {
  return ContractStorage2::checkIfAlive();
}

string ScillaIPCServer::testServerRPC(const string &query, const string &value) {
  return ("Query = " + query + " & Value = " + value);
}