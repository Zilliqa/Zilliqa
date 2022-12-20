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

#ifndef ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERIMPL_H_
#define ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERIMPL_H_

#include <atomic>
#include <unordered_map>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "NewWebsocketServer.h"

namespace rpc {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;
using AsioCtx = asio::io_context;
using Socket = tcp::socket;
using HttpRequest = http::request<http::basic_string_body<char>>;

class APIThreadPool;

namespace ws {

class Connection;

/// Websocket server implementation
class WebsocketServerImpl
    : public WebsocketServer,
      public std::enable_shared_from_this<WebsocketServerImpl> {
 public:
  WebsocketServerImpl(AsioCtx& asio, std::shared_ptr<APIThreadPool> threadPool)
      : m_asio(asio), m_threadPool(std::move(threadPool)) {
    assert(m_threadPool);
  }

  /// A metric
  size_t GetConnectionsNumber() const {
    return m_totalConnections;
  }

  /// Called by HTTP server on new WS upgrade request
  void NewConnection(std::string&& from, Socket&& socket, HttpRequest&& req);

  /// Called from Connection with its id.
  /// Empty msg means that the conn is closed.
  /// If returns false then conn will close silently
  bool MessageFromConnection(ConnectionId id, const std::string& from,
                             InMessage msg);

  // overrides
  void SendMessage(ConnectionId conn_id, OutMessage msg) override;
  void CloseAll() override;

 private:
  // overrides
  void SetOptions(Feedback feedback, size_t max_in_msg_size) override;
  void CloseConnection(ConnectionId conn_id) override;

  /// Asio context is needed here to perform network-related operations in their
  /// dedicated thread
  AsioCtx& m_asio;

  /// Thread pool to which other messages than eth_[un]subscribe to be
  /// dispatched
  std::shared_ptr<APIThreadPool> m_threadPool;

  /// Feedback to WebsocketServer owner
  Feedback m_feedback;

  /// Max incoming message size
  size_t m_maxMsgSize = DEF_MAX_INCOMING_MSG_SIZE;

  /// Incremental counter
  ConnectionId m_counter = 0;

  /// Active connections
  std::unordered_map<ConnectionId, std::shared_ptr<Connection>> m_connections;

  /// Metric, can be accessed from foreign thread
  std::atomic<size_t> m_totalConnections{};
};

}  // namespace ws
}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERIMPL_H_
