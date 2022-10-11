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

#include "libData/AccountData/Account.h"

class ScillaBCInfo {
 public:
  ScillaBCInfo(const uint64_t curBlockNum, const uint64_t curDSBlockNum,
               const Address& curContrAddr)
      : m_curBlockNum(curBlockNum),
        m_curDSBlockNum(curDSBlockNum),
        m_curContrAddr(curContrAddr) {}

  ScillaBCInfo() = default;
  ~ScillaBCInfo() = default;
  ScillaBCInfo(const ScillaBCInfo&) = default;
  ScillaBCInfo(ScillaBCInfo&&) = default;
  ScillaBCInfo& operator=(const ScillaBCInfo&) = default;
  ScillaBCInfo& operator=(ScillaBCInfo&&) = default;

  const uint64_t& getCurBlockNum() const { return m_curBlockNum; }
  const uint64_t& getCurDSBlockNum() const { return m_curDSBlockNum; }
  const Address& getCurContrAddr() const { return m_curContrAddr; }

 private:
  uint64_t m_curBlockNum{};
  uint64_t m_curDSBlockNum{};
  Address m_curContrAddr{};
};

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 public:
  ScillaIPCServer(jsonrpc::AbstractServerConnector& conn);

  ~ScillaIPCServer() = default;
  ScillaIPCServer(const ScillaIPCServer&) = delete;
  ScillaIPCServer(ScillaIPCServer&&) = delete;
  ScillaIPCServer& operator=(const ScillaIPCServer&) = delete;
  ScillaIPCServer& operator=(ScillaIPCServer&) = delete;

  inline virtual void fetchStateValueI(const Json::Value& request,
                                       Json::Value& response);
  inline virtual void fetchExternalStateValueI(const Json::Value& request,
                                               Json::Value& response);
  inline virtual void updateStateValueI(const Json::Value& request,
                                        Json::Value& response);
  inline virtual void fetchExternalStateValueB64I(const Json::Value& request,
                                                  Json::Value& response);
  inline virtual void fetchBlockchainInfoI(const Json::Value& request,
                                           Json::Value& response);
  virtual bool fetchStateValue(const std::string& query, std::string& value,
                               bool& found);
  virtual bool fetchExternalStateValue(const std::string& addr,
                                       const std::string& query,
                                       std::string& value, bool& found,
                                       std::string& type);
  virtual bool updateStateValue(const std::string& query,
                                const std::string& value);
  virtual bool fetchBlockchainInfo(const std::string& query_name,
                                   const std::string& query_args,
                                   std::string& value);
  void setBCInfoProvider(const ScillaBCInfo& bcInfo);

 private:
  ScillaBCInfo m_BCInfo;
};

#endif  // ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_
