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

#ifndef ZILLIQA_SRC_LIBSERVER_LOCALAPISERVER_H_
#define ZILLIQA_SRC_LIBSERVER_LOCALAPISERVER_H_

#include <atomic>
#include <optional>
#include <thread>

#include <jsonrpccpp/server/abstractserverconnector.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace rpc {

class LocalAPIServer : public jsonrpc::AbstractServerConnector {
 public:
  LocalAPIServer(const std::string& ipToBind, uint16_t port)
      : m_ip(ipToBind), m_port(port), m_asio(1){};

  ~LocalAPIServer() override;

 private:
  // AbstractServerConnector overrides
  bool StartListening() override;
  bool StopListening() override;

  // NVI for stop listening
  bool DoStop();

  // WorkerThread that accepts connections
  void WorkerThread();

  std::string m_ip;
  uint16_t m_port;
  boost::asio::ip::tcp::endpoint m_endpoint;
  boost::asio::io_context m_asio;
  std::optional<boost::asio::ip::tcp::acceptor> m_acceptor;
  std::optional<std::thread> m_thread;
  std::atomic<bool> m_started{};
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_LOCALAPISERVER_H_
