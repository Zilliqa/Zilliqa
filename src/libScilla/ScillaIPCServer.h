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
#ifndef ZILLIQA_SRC_LIBSCILLA_SCILLAIPCSERVER_H_
#define ZILLIQA_SRC_LIBSCILLA_SCILLAIPCSERVER_H_

#include <jsonrpccpp/server/abstractserver.h>

#include "depends/common/FixedHash.h"
#include "libData/AccountData/Address.h"
#include "libMetrics/Api.h"
#include "libMetrics/MetricFilters.h"

class ScillaBCInfo {
 public:
  ScillaBCInfo();

  void SetUp(const uint64_t curBlockNum, const uint64_t curDSBlockNum,
             const Address& originAddr, const Address& curContrAddr,
             const dev::h256& rootHash, const uint32_t scillaVersion);

  const uint64_t& getCurBlockNum() const { return m_curBlockNum; }
  const uint64_t& getCurDSBlockNum() const { return m_curDSBlockNum; }
  const dev::h256& getRootHash() const { return m_rootHash; }
  const Address& getOriginAddr() const { return m_originAddr; }
  const Address& getCurContrAddr() const { return m_curContrAddr; }
  const uint32_t& getScillaVersion() const { return m_scillaVersion; }

 private:
  uint64_t m_curBlockNum{};
  uint64_t m_curDSBlockNum{};
  Address m_curContrAddr{};
  Address m_originAddr{};
  dev::h256 m_rootHash{};
  uint32_t m_scillaVersion{};

  Z_I64GAUGE m_bcInfoCount{Z_FL::SCILLA_IPC, "scilla_bcinfo_invocations_count",
                           "Metrics for ScillaBCInfo", "Blocks", true};
};

class AccountStore;

class ScillaIPCServer : public jsonrpc::AbstractServer<ScillaIPCServer> {
 public:
  ScillaIPCServer(AccountStore* parent, jsonrpc::AbstractServerConnector& conn);

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
  inline virtual void fetchStateJsonI(const Json::Value& request,
                                      Json::Value& response);
  inline virtual void fetchCodeJsonI(const Json::Value& request,
                                     Json::Value& response);
  inline virtual void fetchContractInitDataJsonI(const Json::Value& request,
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
  void setBCInfoProvider(const uint64_t curBlockNum,
                         const uint64_t curDSBlockNum,
                         const Address& originAddr, const Address& curContrAddr,
                         const dev::h256& rootHash,
                         const uint32_t scillaVersion);

  // bool fetchExternalStateValue(const std::string& addr,
  //                              const std::string& query, std::string& value,
  //                              bool& found, std::string& type);
 private:
  AccountStore* m_parent;
  ScillaBCInfo m_BCInfo;
};

#endif  // ZILLIQA_SRC_LIBSCILLA_SCILLAIPCSERVER_H_
