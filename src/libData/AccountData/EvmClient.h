/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMCLIENT_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMCLIENT_H_

#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <map>
#include <memory>
#include "common/Constants.h"
#include "common/Singleton.h"
#include "libServer/UnixDomainSocketClient.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/Logger.h"
#include "libUtils/Metrics.h"

/*
 * EvmClient
 * The Client interface to the EVM-daemon via jsonRpc
 * uses jsoncpprpc but with our own custom client
 */

class EvmClient : public Singleton<EvmClient> {
 public:
  EvmClient() {
    if (LOG_SC) {
      LOG_GENERAL(INFO, "Evm Client Created");
    }
  };

  virtual ~EvmClient();

  // Init Routine Called once on System Startup as the EvmClient is accessed as
  // a singleton.

  void Init();

  // Reset
  // Use this method with care as it terminates the current instance of the
  // evm-ds, first it does it politely, then it goes for the kill -9 approach

  void Reset();

  // CallRunner
  // Invoked the RPC method contained within the _json parameter
  // Returns a C++ object populated with the results.

  virtual bool CallRunner(const Json::Value& _json, evm::EvmResult& result);

 protected:
  // OpenServer
  //
  // Used by the custom network handler.
  virtual bool OpenServer();

 private:
  std::unique_ptr<jsonrpc::Client> m_client;
  std::unique_ptr<rpc::UnixDomainSocketClient> m_connector;
  boost::process::child m_child;
  // In case we need to protect unsafe code in future.
  std::mutex m_mutexMain;
  metrics::int64_t m_ctrClient = Metrics::GetInstance().CreateInt64Metric(
      "zilliqa", "evm_client", "Calls to EVM-DS");
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMCLIENT_H_
