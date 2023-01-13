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

#ifndef ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_

#include <functional>
#include <memory>
#include <string>

namespace rpc {

/// Websocket server: owner's interface
class WebsocketServer {
 public:
  /// Default max inbound raw message size. That big because it can handle EVM
  /// contract bytes
  static constexpr size_t DEF_MAX_INCOMING_MSG_SIZE = 5 * 1024 * 1024;

  /// Connection ID: auto-incremented integer unique for server instance
  using ConnectionId = uint64_t;

  /// Incoming text message, all JSON processing is up to owner
  using InMessage = std::string;

  /// Outgoing text message const-and-shared for low cost reuse between sessions
  /// and their transfer speeds
  using OutMessage = std::shared_ptr<const std::string>;

  /// Callback from server to its owner. Receives incoming messages or EOF.
  /// Empty msg means EOF.
  /// unknownMethodFound is set to true if owner is not going to handle this
  /// request, but the request is valid
  /// The owner returns true to proceed with connection, false to close it.
  using Feedback = std::function<bool(ConnectionId id, const InMessage& msg,
                                      bool& unknownMethodFound)>;

  /// Dtor
  virtual ~WebsocketServer() = default;

  /// Owner initializes server
  virtual void SetOptions(Feedback feedback, size_t max_input_msg_size) = 0;

  /// Enqueues outbound message into connection.
  virtual void SendMessage(ConnectionId conn_id, OutMessage msg) = 0;

  /// Closes connection with a given id, if exists
  virtual void CloseConnection(ConnectionId conn_id) = 0;

  /// Closes all, no incoming messages via Feedback after it
  virtual void CloseAll() = 0;
};

}  // namespace rpc

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVER_H_
