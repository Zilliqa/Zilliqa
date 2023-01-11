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

#ifndef ZILLIQA_SRC_LIBSERVER_APISERVERIMPL_H_
#define ZILLIQA_SRC_LIBSERVER_APISERVERIMPL_H_

#include "APIServer.h"

#include <atomic>
#include <optional>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <jsonrpccpp/server/abstractserverconnector.h>

#include "APIThreadPool.h"
#include "WebsocketServerBackend.h"

namespace rpc {

class APIServerImpl : public APIServer,
                      public jsonrpc::AbstractServerConnector,
                      public std::enable_shared_from_this<APIServerImpl> {
 public:
  /// Incremental IDs, for dispatching
  using ConnectionId = uint64_t;

  /// Ctor
  explicit APIServerImpl(Options options);

  /// Called from server's owner. Cannot put everything into the ctor because of
  /// shared_from_this() usage
  bool Start();

  /// Called from a connection on websocket upgrade
  void OnWebsocketUpgrade(ConnectionId id, std::string&& from, Socket&& socket,
                          HttpRequest&& request);

  /// Called from a connection to put the request into thread pool
  void OnRequest(ConnectionId id, std::string from, HttpRequest&& request);

  /// Called from a connection when it's closed
  void OnClosed(ConnectionId id);

 private:
  class Connection;

  // APIServer overrides
  std::shared_ptr<WebsocketServer> GetWebsocketServer() override;
  jsonrpc::AbstractServerConnector& GetRPCServerBackend() override;
  void Close() override;

  // AbstractServerConnector overrides
  bool StartListening() override;
  bool StopListening() override;

  bool DoListen();

  /// Initiates accept async operation
  void AcceptNext();

  /// Socket accept callback
  void OnAccept(beast::error_code ec, tcp::socket socket);

  /// Performs synchronous call to jsonrpccpp handler inside a thread from the
  /// pool
  APIThreadPool::Response ProcessRequestInThreadPool(
      const APIThreadPool::Request& request);

  /// Processes responses from thread pool in the main thread
  void OnResponseFromThreadPool(APIThreadPool::Response&& response);

  /// Event loop thread
  void EventLoopThread();

  /// Server options
  Options m_options;

  /// If true, dedicated event loop will be running
  bool m_ownEventLoop = false;

  /// Started flag
  std::atomic<bool> m_started{};

  /// Active flag
  std::atomic<bool> m_active{};

  /// Thread pool
  std::shared_ptr<APIThreadPool> m_threadPool;

  /// Websocket server
  std::shared_ptr<WebsocketServerBackend> m_websocket;

  /// Listening socket
  std::optional<tcp::acceptor> m_acceptor;

  /// Incremental counter
  ConnectionId m_counter = 0;

  /// Active connections
  std::unordered_map<ConnectionId, std::shared_ptr<Connection>> m_connections;

  /// Event loop thread (if internal loop enabled)
  std::optional<std::thread> m_eventLoopThread;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_APISERVERIMPL_H_
