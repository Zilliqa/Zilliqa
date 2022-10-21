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

namespace evmproj {
struct CallResponse;
}

// Custom socket handler using asio
// for evmclient connection to evm-ds

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

      stream_protocol::socket s(io_context);
      s.connect(stream_protocol::endpoint(m_path));

      std::string toSend = message + DEFAULT_DELIMITER_CHAR;
      if (LOG_SC) {
        LOG_GENERAL(INFO, "Writing to socket " << toSend);
      }
      boost::asio::write(s, boost::asio::buffer(toSend, toSend.length()));

      boost::asio::streambuf b;

      size_t reply_length =
          boost::asio::read_until(s, b, DEFAULT_DELIMITER_CHAR);
      std::istream is(&b);
      std::getline(is, result);
      if (LOG_SC) {
        LOG_GENERAL(INFO, "reading from socket " << reply_length
                                                 << " bytes : " << result);
      }
    } catch (std::exception& e) {
      LOG_GENERAL(WARNING,
                  "Exception caught in custom SendRPCMessage " << e.what());
    }
  };

 private:
  std::string m_path;
};

}  // namespace evmdsrpc
class EvmClient : public Singleton<EvmClient> {
 public:
  EvmClient() {
    if (LOG_SC) {
      LOG_GENERAL(INFO, "Evm Client Created");
    }
  };

  virtual ~EvmClient();

  void Init();

  bool Terminate();

  virtual bool CallRunner(uint32_t version, const Json::Value& _json,
                          evmproj::CallResponse& result,
                          const uint32_t counter = MAXRETRYCONN);

 protected:
  virtual bool OpenServer();

  virtual bool CleanupPreviousInstances();

 private:
  std::unique_ptr<jsonrpc::Client> m_client;
  std::unique_ptr<evmdsrpc::EvmDsDomainSocketClient> m_connector;
  boost::process::child m_child;
  std::mutex m_mutexMain;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMCLIENT_H_
