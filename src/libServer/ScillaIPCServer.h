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

#include "libData/AccountData/Address.h"

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 public:
  ScillaIPCServer(jsonrpc::AbstractServerConnector& conn);
  ~ScillaIPCServer() = default;

  inline virtual void fetchStateValueI(const Json::Value& request,
                                       Json::Value& response);
  inline virtual void updateStateValueI(const Json::Value& request,
                                        Json::Value& response);
  virtual bool fetchStateValue(const std::string& query, std::string& value,
                               bool& found);
  virtual bool updateStateValue(const std::string& query,
                                const std::string& value);
  void setContractAddressVer(const Address& address, uint32_t version);

  bool fetchExternalStateValue(const std::string& addr,
                               const std::string& query, std::string& value,
                               bool& found, std::string& type);

 private:
  Address m_contrAddr = Address();
  uint32_t m_version = std::numeric_limits<uint32_t>::max();
};

#endif  // ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_