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

#include <deque>

#include "WebsocketServerImpl.h"

namespace evmproj {
namespace ws {

using CloseReason = websocket::close_code;

/// Websocket connection from the server perspective
/// TODO write buffer constraint against slow clients or their sabotage
class Connection : public std::enable_shared_from_this<Connection> {
 public:
  using ConnectionId = WebsocketServer::ConnectionId;
  using OutMessage = WebsocketServer::OutMessage;

  Connection(std::weak_ptr<WebsocketServerImpl> owner, ConnectionId id,
             Socket&& socket)
      : m_owner(std::move(owner)), m_id(id), m_stream(std::move(socket)) {}

  void WebsocketAccept(HttpRequest&& req) {
    websocket::stream_base::timeout opt{
        std::chrono::seconds(30),  // handshake timeout
        std::chrono::seconds(10),  // idle timeout
        true                       // pings
    };
    m_stream.set_option(opt);

    m_stream.set_option(
        websocket::stream_base::decorator([](websocket::response_type& res) {
          res.set(http::field::server, ".");  // TODO server string for http
        }));
    m_stream.async_accept(
        std::move(req),
        beast::bind_front_handler(&Connection::OnAccept, shared_from_this()));
  }

  void Write(OutMessage msg) {
    if (!msg || msg->empty() || m_owner.expired()) {
      // XXX warn
      return;
    }

    m_writeQueue.emplace_back(std::move(msg));
    if (m_writeQueue.size() == 1) {
      StartWriting();
    }
  }

  /// Called from owner
  void Close(CloseReason reason) {
    m_owner.reset();
    if (m_stream.is_open()) {
      if (reason == CloseReason::protocol_error || m_writeQueue.empty()) {
        beast::error_code ec;
        m_stream.close(reason, ec);
      } else {
        // push EOF and close after everything queued is sent
        m_writeQueue.push_back({});
      }
    }
  }

 private:
  void OnAccept(beast::error_code ec) {
    if (ec) {
      // LOG
      OnClosed();
      return;
    }
    StartReading();
  }

  void OnClosed() {
    auto owner = m_owner.lock();
    if (owner) {
      std::ignore = owner->MessageFromConnection(m_id, {});
      m_owner.reset();
    }
  }

  void StartReading() {
    m_readBuffer.consume(m_readBuffer.size());
    m_stream.async_read(
        m_readBuffer,
        beast::bind_front_handler(&Connection::OnRead, shared_from_this()));
  }

  void OnRead(beast::error_code ec, size_t n) {
    if (ec || n == 0) {
      if (ec != websocket::error::closed) {
        // LOG
      }
      OnClosed();
      return;
    }

    auto owner = m_owner.lock();
    if (owner) {
      bool proceed = owner->MessageFromConnection(
          m_id, beast::buffers_to_string(m_readBuffer.data()));
      if (proceed) {
        StartReading();
      } else {
        Close(CloseReason::protocol_error);
      }
    } else {
      Close(CloseReason::going_away);
    }
  }

  void StartWriting() {
    if (m_writeQueue.empty()) {
      return;
    }

    auto& msg = m_writeQueue.front();

    m_stream.text(true);
    m_stream.async_write(
        asio::const_buffer(msg->data(), msg->size()),
        beast::bind_front_handler(&Connection::OnWrite, shared_from_this()));
  }

  void OnWrite(beast::error_code ec, size_t n) {
    if (ec) {
      if (ec != websocket::error::closed) {
        // LOG
      }
      OnClosed();
      return;
    }

    if (m_writeQueue.empty() || n != m_writeQueue.front()->size()) {
      // Something corrupted
      // XXX LOG
      OnClosed();
      return;
    }

    m_writeQueue.pop_front();

    if (!m_writeQueue.empty()) {
      auto& msg = m_writeQueue.front();

      if (!msg) {
        // EOF
        m_writeQueue.clear();
        Close(CloseReason::normal);
        return;
      }

      StartWriting();
    }
  }

  std::weak_ptr<WebsocketServerImpl> m_owner;
  const ConnectionId m_id;
  websocket::stream<beast::tcp_stream> m_stream;
  beast::flat_buffer m_readBuffer;
  std::deque<OutMessage> m_writeQueue;
};

void WebsocketServerImpl::CloseAll() {
  m_asio.post([self = shared_from_this()] {
    for (auto& p : self->m_connections) {
      p.second->Close(CloseReason::going_away);
    }
    self->m_connections.clear();
  });
}

void WebsocketServerImpl::NewConnection(Socket&& socket, HttpRequest&& req) {
  auto conn = std::make_shared<Connection>(weak_from_this(), ++m_counter,
                                           std::move(socket));
  conn->WebsocketAccept(std::move(req));
  m_connections[m_counter] = std::move(conn);
}

bool WebsocketServerImpl::MessageFromConnection(ConnectionId id,
                                                InMessage msg) {
  auto it = m_connections.find(id);
  if (it == m_connections.end()) {
    // XXX log
    return false;
  }

  if (msg.empty()) {
    // connection is closed
    m_connections.erase(it);
    return false;
  }

  if (!m_feedback) {
    // XXX warning
    it->second->Close(CloseReason::internal_error);
    m_connections.erase(it);
    return false;  // ???????
  }

  if (msg.size() > m_maxMsgSize) {
    // XXX warning
    it->second->Close(CloseReason::too_big);
    m_connections.erase(it);
    return false;
  }

  // TODO atomic about abandoned by the owner ???

  if (!m_feedback(id, std::move(msg))) {
    m_connections.erase(it);
    return false;
  }

  return true;
}

void WebsocketServerImpl::SetOptions(Feedback feedback,
                                     size_t max_in_msg_size) {
  if (max_in_msg_size < 32 || !feedback) {
    // XXX warning
    return;
  }

  // will set options asynchronously in the network thread
  m_asio.post([self = shared_from_this(), feedback = std::move(feedback),
               size = max_in_msg_size]() mutable {
    self->m_feedback = std::move(feedback);
    self->m_maxMsgSize = size;
  });
}

void WebsocketServerImpl::SendMessage(ConnectionId conn_id, OutMessage msg) {
  m_asio.post([self = shared_from_this(), id = conn_id,
               msg = std::move(msg)]() mutable {
    auto it = self->m_connections.find(id);
    if (it == self->m_connections.end()) {
      // closed
      if (self->m_feedback) {
        self->m_feedback(id, {});
      }
      return;
    }
    it->second->Write(std::move(msg));
  });
}

void WebsocketServerImpl::CloseConnection(ConnectionId conn_id) {
  m_asio.post([self = shared_from_this(), id = conn_id]() {
    auto it = self->m_connections.find(id);
    if (it != self->m_connections.end()) {
      it->second->Close(CloseReason::protocol_error);
      self->m_connections.erase(it);
    }
  });
}

}  // namespace ws
}  // namespace evmproj
