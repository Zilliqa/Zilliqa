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

#ifndef ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERBACKEND_H_
#define ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERBACKEND_H_

#include "WebsocketServer.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>

namespace rpc {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;

using tcp = asio::ip::tcp;
using Socket = tcp::socket;
using HttpRequest = http::request<http::basic_string_body<char>>;
using AsioCtx = asio::io_context;

class APIThreadPool;

/// Websocket server backend interface
class WebsocketServerBackend : public WebsocketServer {
 public:
  /// Creates WS backend w/o threadpool: for Scilla WS server
  static std::shared_ptr<WebsocketServerBackend> Create(AsioCtx& asio);

  /// Creates WS backend w/threadpool: for jsonrpc API server
  static std::shared_ptr<WebsocketServerBackend> CreateWithThreadPool(
      AsioCtx& asio, std::shared_ptr<APIThreadPool> threadPool);

  /// Dtor
  virtual ~WebsocketServerBackend() = default;

  /// Called by HTTP server on new WS upgrade request
  virtual void NewConnection(std::string&& from, Socket&& socket,
                             HttpRequest&& req) = 0;

  /// Called by dedicated WS server
  virtual void NewConnection(std::string&& from, Socket&& socket) = 0;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERBACKEND_H_
