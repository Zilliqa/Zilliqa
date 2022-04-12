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
  ScillaBCInfo(uint64_t curBlockNum, uint64_t curDSBlockNum, Address originAddr,
               Address curContrAddr, dev::h256 rootHash, uint32_t scillaVersion)
      : m_curBlockNum(curBlockNum),
        m_curDSBlockNum(curDSBlockNum),
        m_curContrAddr(curContrAddr),
        m_originAddr(originAddr),
        m_rootHash(rootHash),
        m_scillaVersion(scillaVersion) {}

  const uint64_t& getCurBlockNum() const { return m_curBlockNum; }
  const uint64_t& getCurDSBlockNum() const { return m_curDSBlockNum; }
  const dev::h256& getRootHash() const { return m_rootHash; }
  const Address& getOriginAddr() const { return m_originAddr; }
  const Address& getCurContrAddr() const { return m_curContrAddr; }
  const uint32_t& getScillaVersion() const { return m_scillaVersion; }

 private:
  uint64_t m_curBlockNum;
  uint64_t m_curDSBlockNum;
  Address m_curContrAddr;
  Address m_originAddr;
  dev::h256 m_rootHash;
  uint32_t m_scillaVersion;
};

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 public:
  ScillaIPCServer(jsonrpc::AbstractServerConnector& conn);
  ~ScillaIPCServer() = default;

  inline virtual void fetchStateValueI(const Json::Value& request,
                                       Json::Value& response);
  inline virtual void fetchExternalStateValueI(const Json::Value& request,
                                               Json::Value& response);
  inline virtual void updateStateValueI(const Json::Value& request,
                                        Json::Value& response);
  inline virtual void fetchExternalStateValueB64I(const Json::Value& request,
                                                  Json::Value& response);
  virtual bool fetchStateValue(const std::string& query, std::string& value,
                               bool& found);
  virtual bool fetchExternalStateValue(const std::string& addr,
                                       const std::string& query,
                                       std::string& value, bool& found,
                                       std::string& type);
  virtual bool updateStateValue(const std::string& query,
                                const std::string& value);
  void setContractAddressVerRoot(const Address& address, uint32_t version,
                                 const dev::h256& rootHash);

  // bool fetchExternalStateValue(const std::string& addr,
  //                              const std::string& query, std::string& value,
  //                              bool& found, std::string& type);

 private:
  Address m_contrAddr = Address();
  uint32_t m_version = std::numeric_limits<uint32_t>::max();
  dev::h256 m_rootHash = dev::h256();
};

#endif  // ZILLIQA_SRC_LIBSERVER_SCILLAIPCSERVER_H_
