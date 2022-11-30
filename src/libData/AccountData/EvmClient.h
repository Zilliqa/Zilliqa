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

#include <jsonrpccpp/client.h>
#include <jsonrpccpp/client/connectors/unixdomainsocketclient.h>
#include <jsonrpccpp/common/sharedconstants.h>
#include <boost/asio.hpp>
#include <boost/process.hpp>
#include <boost/process/child.hpp>
#include <map>
#include <memory>
#include "common/Constants.h"
#include "common/Singleton.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/Logger.h"
#include "libUtils/Metrics.h"

// EvmDsDomainSocketClient
//
// This is a Custom socket handler using asio
// for the evmclient connection to evm-ds

namespace evmdsrpc {
class EvmDsDomainSocketClient : public jsonrpc::IClientConnector {
 public:
  EvmDsDomainSocketClient(const std::string& path) : m_path(path){};

  virtual ~EvmDsDomainSocketClient(){};

  virtual void SendRPCMessage(const std::string& message, std::string& result) {
    LOG_MARKER();
    try {
      using boost::asio::local::stream_protocol;
      boost::asio::io_context io_context;
      stream_protocol::socket unixDomainSocket(io_context);
      boost::asio::streambuf streamBuffer;
      //
      // Connect to the stream
      try {
        unixDomainSocket.connect(stream_protocol::endpoint(m_path));
      } catch (std::exception& e) {
        if (LOG_SC) {
          LOG_GENERAL(INFO, "Exception calling connect "
                                << e.what() << " to path: " << m_path
                                << " to send msg: " << message);
        }
        throw e;
      }
      std::string toSend = message + DEFAULT_DELIMITER_CHAR;

      // Write the JsonRpc
      try {
        boost::asio::write(unixDomainSocket,
                           boost::asio::buffer(toSend, toSend.length()));
      } catch (std::exception& e) {
        if (LOG_SC) {
          LOG_GENERAL(INFO, "Exception calling write " << e.what());
        }
        throw e;
      }
      //
      // Read the Response
      try {
        boost::asio::read_until(unixDomainSocket, streamBuffer,
                                DEFAULT_DELIMITER_CHAR);
      } catch (std::exception& e) {
        if (LOG_SC) {
          LOG_GENERAL(INFO, "Exception calling read " << e.what());
        }
        throw e;
      }
      std::istream is(&streamBuffer);
      // transfer it into the users object
      std::getline(is, result);
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING,
                  "Exception caught in custom SendRPCMessage " << e.what());
    }
  }

 private:
  std::string m_path;
};

}  // namespace evmdsrpc

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
  std::unique_ptr<evmdsrpc::EvmDsDomainSocketClient> m_connector;
  boost::process::child m_child;
  // In case we need to protect unsafe code in future.
  std::mutex m_mutexMain;
  metrics::int64_t   m_ctrClient = Metrics::GetInstance().CreateInt64Metric( "zilliqa", "evm_client" , "Calls to EVM-DS");
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMCLIENT_H_
