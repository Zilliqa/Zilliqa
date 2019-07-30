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
#include <jsonrpccpp/server/abstractserver.h>
#include <jsonrpccpp/server/connectors/unixdomainsocketserver.h>

#include "depends/common/FixedHash.h"

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 public:
  ScillaIPCServer(const dev::h160& contrAddr,
                  jsonrpc::UnixDomainSocketServer& conn,
                  jsonrpc::serverVersion_t type = jsonrpc::JSONRPC_SERVER_V2);

  inline virtual void fetchStateValueI(const Json::Value& request,
                                       Json::Value& response);
  inline virtual void updateStateValueI(const Json::Value& request,
                                        Json::Value& response);
  virtual bool fetchStateValue(const std::string& query, std::string& value,
                               bool& found) = 0;
  virtual bool updateStateValue(const std::string& query,
                                const std::string& value) = 0;
  void setContractAddress(const dev::h160& address);

 private:
  dev::h160 m_contrAddr;
};

#endif  // ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_