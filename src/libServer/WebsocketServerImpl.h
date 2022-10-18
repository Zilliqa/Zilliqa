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

#ifndef ZILLIQA_SRC_LIBSERVER_NEWWEBSOCKETSERVERIMPL_H_
#define ZILLIQA_SRC_LIBSERVER_NEWWEBSOCKETSERVERIMPL_H_

#include <atomic>
#include <unordered_map>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

#include "NewWebsocketServer.h"

namespace evmproj {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;
using AsioCtx = asio::io_context;
using Socket = tcp::socket;
using HttpRequest = http::request<http::basic_string_body<char>>;

namespace ws {

class Connection;

/// Websocket server implementation
class WebsocketServerImpl
    : public WebsocketServer,
      public std::enable_shared_from_this<WebsocketServerImpl> {
 public:
  explicit WebsocketServerImpl(AsioCtx& asio) : m_asio(asio) {}

  /// Explicit close because of shared_ptr usage. Called by owner
  void CloseAll();

  /// Called by HTTP server on new WS upgrade request
  void NewConnection(Socket&& socket, HttpRequest&& req);

  /// Called from Connection with its id.
  /// Empty msg means that the conn is closed.
  /// If returns false then conn will close silently
  bool MessageFromConnection(ConnectionId id, InMessage msg);

 private:
  // overrides
  void SetOptions(Feedback feedback, size_t max_in_msg_size) override;
  void SendMessage(ConnectionId conn_id, OutMessage msg) override;
  void CloseConnection(ConnectionId conn_id) override;

  /// Asio context is needed here to perform network-related operations in their
  /// dedicated thread
  AsioCtx& m_asio;

  /// Feedback to WebsocketServer owner
  Feedback m_feedback;

  /// Max incoming message size
  size_t m_maxMsgSize = DEF_MAX_INCOMING_MSG_SIZE;

  /// Incremental counter
  ConnectionId m_counter = 0;

  /// Active connections
  std::unordered_map<ConnectionId, std::shared_ptr<Connection>> m_connections;
};

}  // namespace ws
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBSERVER_NEWWEBSOCKETSERVERIMPL_H_
