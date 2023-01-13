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

#include "WebsocketServerBackend.h"

namespace rpc::ws {

class Connection;

/// Websocket server implementation
class WebsocketServerImpl
   : public WebsocketServerBackend,
     public std::enable_shared_from_this<WebsocketServerImpl> {
public:
 WebsocketServerImpl(AsioCtx& asio, std::shared_ptr<APIThreadPool> threadPool)
     : m_asio(asio), m_threadPool(std::move(threadPool)) {}

 /// Called from Connection with its id.
 /// Empty msg means that the conn is closed.
 /// If returns false then conn will close silently
 bool MessageFromConnection(ConnectionId id, const std::string& from,
                            InMessage msg);

private:
 // overrides
 void SetOptions(Feedback feedback, size_t max_in_msg_size) override;
 void CloseConnection(ConnectionId conn_id) override;
 void SendMessage(ConnectionId conn_id, OutMessage msg) override;
 void CloseAll() override;
 void NewConnection(std::string&& from, Socket&& socket,
                    HttpRequest&& req) override;
 void NewConnection(std::string&& from, Socket&& socket) override;

 std::shared_ptr<Connection> CreateNewConnection(std::string&& from,
                                                 Socket&& socket);

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
};

}  // namespace rpc::ws

#endif  // ZILLIQA_SRC_LIBSERVER_WEBSOCKETSERVERIMPL_H_
