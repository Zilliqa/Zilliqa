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
#ifndef ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_

#include <jsonrpccpp/server.h>
#include <jsonrpccpp/server/connectors/unixdomainsocketserver.h>
#include "depends/common/FixedHash.h"

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 private:
  dev::h160 contract_address;
  string DEFAULT_ERROR_MESSAGE = "ERROR";
 public:
  ScillaIPCServer(jsonrpc::UnixDomainSocketServer &server,
                  const dev::h160 &contract_address);

  inline void fetchStateValueI(const Json::Value &request,
                               Json::Value &response) {
    response = this->fetchStateValue(request["query"].asString());
  }

  inline void updateStateValueI(const Json::Value &request,
                                Json::Value &response) {
    response = this->updateStateValue(request["query"].asString(),
                                      request["value"].asString());
  }
  bool fetchStateValue(const std::string &query);
  bool updateStateValue(const std::string &query, const std::string &value);
  void setContractAddress(dev::h160 &address);
  dev::h160 getContractAddress();

  // TODO: Remove once the relevant methods in ContractStorage2 are complete
  inline void testServerRPCI(const Json::Value &request,
                             Json::Value &response) {
    response = this->testServerRPC(request["query"].asString());
  }
  bool testServer();
  std::string testServerRPC(const std::string &query);
};

#endif  // ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_