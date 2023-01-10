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

#include "WebsocketServerImpl.h"

#include <deque>

#include "APIThreadPool.h"
#include "libUtils/Logger.h"

namespace rpc {

/// Creates WS backend w/o threadpool: for Scilla WS server
std::shared_ptr<WebsocketServerBackend> WebsocketServerBackend::Create(
    AsioCtx& asio) {
  return std::make_shared<ws::WebsocketServerImpl>(
      asio, std::shared_ptr<APIThreadPool>{});
}

/// Creates WS backend w/threadpool: for jsonrpc API server
std::shared_ptr<WebsocketServerBackend>
WebsocketServerBackend::CreateWithThreadPool(
    AsioCtx& asio, std::shared_ptr<APIThreadPool> threadPool) {
  if (!threadPool) {
    throw std::runtime_error("Thread pool expected");
  }
  return std::make_shared<ws::WebsocketServerImpl>(asio, std::move(threadPool));
}

namespace ws {

using CloseReason = websocket::close_code;

/// Websocket connection from the server perspective
/// TODO write buffer constraint against slow clients or their sabotage
class Connection : public std::enable_shared_from_this<Connection> {
 public:
  using ConnectionId = WebsocketServer::ConnectionId;
  using OutMessage = WebsocketServer::OutMessage;

  Connection(std::weak_ptr<WebsocketServerImpl> owner, ConnectionId id,
             std::string&& from, Socket&& socket)
      : m_owner(std::move(owner)),
        m_id(id),
        m_from(std::move(from)),
        m_stream(std::move(socket)) {
    websocket::stream_base::timeout opt{
        std::chrono::seconds(30),  // handshake timeout
        std::chrono::seconds(10),  // idle timeout
        true                       // pings
    };
    m_stream.set_option(opt);
    m_stream.set_option(
        websocket::stream_base::decorator([](websocket::response_type& res) {
          res.set(http::field::server, "zilliqa");
        }));
  }

  void WebsocketAccept(HttpRequest&& req) {
    m_stream.async_accept(
        std::move(req),
        beast::bind_front_handler(&Connection::OnAccept, shared_from_this()));
  }

  void WebsocketAccept() {
    m_stream.async_accept(
        beast::bind_front_handler(&Connection::OnAccept, shared_from_this()));
  }

  void Write(OutMessage msg) {
    if (!msg || msg->empty() || m_owner.expired()) {
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
      LOG_GENERAL(INFO, "Websocket accept failed, " << m_from);
      OnClosed();
      return;
    }
    StartReading();
  }

  void OnClosed() {
    auto owner = m_owner.lock();
    if (owner) {
      std::ignore = owner->MessageFromConnection(m_id, m_from, {});
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
    if (n == 0) {
      // ignore, e.g. press Enter on wscat session, don't disconnect
      StartReading();
      return;
    }

    if (ec) {
      if (ec != websocket::error::closed) {
        LOG_GENERAL(INFO, "Websocket connection from " << m_from << " closed, "
                                                       << ec.message());
      }
      OnClosed();
      return;
    }

    auto owner = m_owner.lock();
    if (owner) {
      bool proceed = owner->MessageFromConnection(
          m_id, m_from, beast::buffers_to_string(m_readBuffer.data()));
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
        LOG_GENERAL(INFO, "Websocket connection from " << m_from << " closed, "
                                                       << ec.message());
      }
      OnClosed();
      return;
    }

    if (m_writeQueue.empty() || n != m_writeQueue.front()->size()) {
      // Something corrupted
      LOG_GENERAL(WARNING, "Inconsistency in websocket server");
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

  /// WS server owns and keeps track of connections
  std::weak_ptr<WebsocketServerImpl> m_owner;

  /// Unique id within the owner
  const ConnectionId m_id;

  /// Peer string for logging
  std::string m_from;

  /// Asio stream
  websocket::stream<beast::tcp_stream> m_stream;

  /// Read buffer for incoming messages
  beast::flat_buffer m_readBuffer;

  /// Write queue
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

void WebsocketServerImpl::NewConnection(std::string&& from, Socket&& socket,
                                        HttpRequest&& req) {
  CreateNewConnection(std::move(from), std::move(socket))
      ->WebsocketAccept(std::move(req));
}

void WebsocketServerImpl::NewConnection(std::string&& from, Socket&& socket) {
  CreateNewConnection(std::move(from), std::move(socket))->WebsocketAccept();
}

std::shared_ptr<Connection> WebsocketServerImpl::CreateNewConnection(
    std::string&& from, Socket&& socket) {
  auto conn = std::make_shared<Connection>(weak_from_this(), ++m_counter,
                                           std::move(from), std::move(socket));
  m_connections[m_counter] = conn;
  LOG_GENERAL(INFO, "WS connection #" << m_counter << " from " << from
                                      << ", total=" << m_connections.size());
  return conn;
}

bool WebsocketServerImpl::MessageFromConnection(ConnectionId id,
                                                const std::string& from,
                                                InMessage msg) {
  auto it = m_connections.find(id);
  if (it == m_connections.end()) {
    LOG_GENERAL(WARNING, "Websocket connection #" << id << " from " << from
                                                  << " not found");
    return false;
  }

  if (msg.empty()) {
    // connection is closed
    m_connections.erase(it);
    return false;
  }

  if (msg.size() > m_maxMsgSize) {
    LOG_GENERAL(WARNING, "Too big message from connection #"
                             << id << " from " << from
                             << " size=" << msg.size());
    it->second->Close(CloseReason::too_big);
    m_connections.erase(it);
    return false;
  }

  if (!m_feedback) {
    LOG_GENERAL(WARNING, "Websocket server not initialized");
    it->second->Close(CloseReason::internal_error);
    m_connections.erase(it);
    return false;
  }

  bool unknownMethodFound = false;

  if (!m_feedback(id, msg, unknownMethodFound)) {
    it->second->Close(CloseReason::protocol_error);
    m_connections.erase(it);
    return false;
  }

  if (unknownMethodFound && m_threadPool) {
    // forward msg to the thread pool
    // TODO double json parsing: to be fixed after project dependencies change
    if (!m_threadPool->PushRequest(id, true, from, std::move(msg))) {
      LOG_GENERAL(WARNING, "Request queue is full");
    }
  }

  return true;
}

void WebsocketServerImpl::SetOptions(Feedback feedback,
                                     size_t max_in_msg_size) {
  if (max_in_msg_size == 0 || !feedback) {
    LOG_GENERAL(WARNING, "Ignoring insane websocket server options");
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
      bool dummy{};
      if (self->m_feedback) {
        // send EOF
        self->m_feedback(id, {}, dummy);
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
}  // namespace rpc
