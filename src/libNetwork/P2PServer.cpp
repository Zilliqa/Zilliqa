/*
 * Copyright (C) 2023 Zilliqa
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

#include "P2PServer.h"

#include <optional>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>

#include "Blacklist.h"
#include "libUtils/Logger.h"

namespace zil::p2p {

using AsioContext = boost::asio::io_context;
using TcpSocket = boost::asio::ip::tcp::socket;
using TcpAcceptor = boost::asio::ip::tcp::acceptor;
using TcpEndpoint = boost::asio::ip::tcp::endpoint;
using ErrorCode = boost::system::error_code;
const ErrorCode OPERATION_ABORTED = boost::asio::error::operation_aborted;
const ErrorCode END_OF_FILE = boost::asio::error::eof;

class P2PServerImpl;

class P2PServerConnection
    : public std::enable_shared_from_this<P2PServerConnection> {
 public:
  P2PServerConnection(std::weak_ptr<P2PServerImpl> owner, uint64_t this_id,
                      Peer&& remote_peer, TcpSocket socket,
                      size_t max_message_size);

  void ReadNextMessage();

  void Close();

 private:
  void OnHeaderRead(const ErrorCode& ec);

  void OnBodyRead(const ErrorCode& ec);

  void CloseSocket();

  void OnConnectionClosed();

  std::weak_ptr<P2PServerImpl> m_owner;
  uint64_t m_id;
  Peer m_remotePeer;
  TcpSocket m_socket;
  size_t m_maxMessageSize;
  zbytes m_readBuffer;
};

namespace {

std::optional<Peer> ExtractRemotePeer(const TcpSocket& socket) {
  ErrorCode ec;
  std::optional<Peer> result;
  auto remote_ep = socket.remote_endpoint(ec);
  if (!ec) {
    auto a = remote_ep.address();
    if (a.is_v4()) {
      result.emplace(uint128_t(htonl(a.to_v4().to_uint())), remote_ep.port());
    } else if (a.is_v6()) {
      result.emplace(uint128_t(a.to_v6().to_bytes()), remote_ep.port());
    }
  } else {
    LOG_GENERAL(WARNING,
                "Cannot extract address from endpoint: " << ec.message());
  }
  return result;
}

}  // namespace

class P2PServerImpl : public P2PServer,
                      public std::enable_shared_from_this<P2PServerImpl> {
 public:
  P2PServerImpl(TcpAcceptor acceptor, size_t max_message_size,
                Callback callback)
      : m_acceptor(std::move(acceptor)),
        m_maxMessageSize(max_message_size),
        m_callback(std::move(callback)) {}

  ~P2PServerImpl() {
    for (auto& conn : m_connections) {
      conn.second->Close();
    }
    // TODO metric
  }

  void AcceptNextConnection() {
    m_acceptor.async_accept(
        [wptr = weak_from_this()](const ErrorCode& ec, TcpSocket sock) {
          if (!wptr.expired() && ec != OPERATION_ABORTED) {
            wptr.lock()->OnAccept(ec, std::move(sock));
          }
        });
  }

  bool OnMessage(uint64_t id, const Peer& from, ReadMessageResult& msg) {
    if (!m_callback(from, msg)) {
      LOG_GENERAL(DEBUG, "Closing incoming connection from " << from);
      OnConnectionClosed(id);
      return false;
    }
    return true;
  }

  void OnConnectionClosed(uint64_t id) {
    m_connections.erase(id);
    // TODO metric about m_connections.size()
    LOG_GENERAL(DEBUG, "Total incoming connections: " << m_connections.size());
  }

 private:
  void OnAccept(const ErrorCode& ec, TcpSocket socket) {
    if (!ec) {
      // TODO
      // Limit connections from peer (do we need it?)

      auto maybe_peer = ExtractRemotePeer(socket);
      if (!maybe_peer) {
        return;
      }

      auto conn = std::make_shared<P2PServerConnection>(
          weak_from_this(), ++m_counter,
          std::move(maybe_peer.value()), /*m_asio,*/
          std::move(socket), m_maxMessageSize);

      conn->ReadNextMessage();

      m_connections[m_counter] = std::move(conn);
      // TODO Metric

      AcceptNextConnection();
    } else {
      LOG_GENERAL(FATAL, "Error in accept : " << ec.message());
    }
  }

  TcpAcceptor m_acceptor;
  size_t m_maxMessageSize;
  Callback m_callback;
  uint64_t m_counter = 0;
  std::unordered_map<uint64_t, std::shared_ptr<P2PServerConnection>>
      m_connections;

  // TODO decide if this is needed. It seems that no
  //  std::map<uint128_t, uint16_t> m_peerConnectionCount;
};

std::shared_ptr<P2PServer> P2PServer::CreateAndStart(AsioContext& asio,
                                                     uint16_t port,
                                                     size_t max_message_size,
                                                     Callback callback) {
  if (port == 0 || max_message_size == 0 || !callback) {
    throw std::runtime_error("P2PServer::CreateAndStart : invalid args");
  }

  std::string error_msg;

  try {
    auto endpoint = TcpEndpoint{boost::asio::ip::make_address("0.0.0.0"), port};

    TcpAcceptor socket(asio);
    socket.open(endpoint.protocol());
    socket.set_option(TcpAcceptor::reuse_address(true));
    socket.bind(endpoint);
    socket.listen();

    auto server = std::make_shared<P2PServerImpl>(
        std::move(socket), /*asio,*/ max_message_size, std::move(callback));

    // this call is needed to be decoupled from ctor, because in the ctor
    // it's impossible to get weak_from_this(), such a subtliety
    server->AcceptNextConnection();

    return server;

  } catch (const boost::system::system_error& e) {
    error_msg = e.what();
  }

  throw std::runtime_error(error_msg);
}

P2PServerConnection::P2PServerConnection(
    std::weak_ptr<P2PServerImpl> owner, uint64_t this_id, Peer&& remote_peer,
    TcpSocket socket, size_t max_message_size)
    : m_owner(std::move(owner)),
      m_id(this_id),
      m_remotePeer(std::move(remote_peer)),
      m_socket(std::move(socket)),
      m_maxMessageSize(max_message_size) {}

void P2PServerConnection::ReadNextMessage() {
  static constexpr size_t RESERVE_SIZE = 1024;
  static constexpr size_t THRESHOLD_SIZE = 1024 * 100;

  auto cap = m_readBuffer.capacity();
  if (cap > THRESHOLD_SIZE) {
    zbytes b;
    m_readBuffer.swap(b);
  }
  m_readBuffer.reserve(RESERVE_SIZE);
  m_readBuffer.resize(HDR_LEN);

  boost::asio::async_read(
      m_socket, boost::asio::buffer(m_readBuffer),
      [self = shared_from_this()](const ErrorCode& ec, size_t n) {
        if (!ec) {
          assert(n == HDR_LEN);
        }
        if (ec != OPERATION_ABORTED) {
          self->OnHeaderRead(ec);
        }
      });
}

void P2PServerConnection::OnHeaderRead(const ErrorCode& ec) {
  if (ec) {
    if (ec != END_OF_FILE) {
      LOG_GENERAL(INFO,
                  "Peer " << m_remotePeer << " read error: " << ec.message());
    }
    OnConnectionClosed();
    return;
  }

  assert(m_readBuffer.size() == HDR_LEN);

  auto remainingLength = ReadU32BE(m_readBuffer.data() + 4);
  if (remainingLength > m_maxMessageSize) {
    LOG_GENERAL(WARNING, "[blacklist] Encountered data of size: "
                             << remainingLength << " being received."
                             << " Adding sending node "
                             << m_remotePeer.GetPrintableIPAddress()
                             << " as strictly blacklisted");
    Blacklist::GetInstance().Add(m_remotePeer.m_ipAddress);

    CloseSocket();
    OnConnectionClosed();
    return;
  }

  m_readBuffer.resize(HDR_LEN + remainingLength);
  boost::asio::async_read(
      m_socket,
      boost::asio::buffer(m_readBuffer.data() + HDR_LEN, remainingLength),
      [self = shared_from_this()](const ErrorCode& ec, size_t n) {
        if (!ec) {
          assert(n == self->m_readBuffer.size() - HDR_LEN);
        }
        if (ec != OPERATION_ABORTED) {
          self->OnBodyRead(ec);
        }
      });
}

void P2PServerConnection::OnBodyRead(const ErrorCode& ec) {
  if (ec) {
    LOG_GENERAL(INFO, "Read error: " << ec.message());
    OnConnectionClosed();
    return;
  }

  ReadMessageResult result;
  auto state = TryReadMessage(m_readBuffer.data(), m_readBuffer.size(), result);

  if (state != ReadState::SUCCESS) {
    LOG_GENERAL(WARNING, "Message deserialize error: blacklisting "
                             << m_remotePeer.GetPrintableIPAddress());
    Blacklist::GetInstance().Add(m_remotePeer.m_ipAddress);

    CloseSocket();
    OnConnectionClosed();
    return;
  }

  auto owner = m_owner.lock();
  if (!owner || !owner->OnMessage(m_id, m_remotePeer, result)) {
    CloseSocket();
    return;
  }

  ReadNextMessage();
}

void P2PServerConnection::Close() {
  m_owner.reset();
  CloseSocket();
}

void P2PServerConnection::CloseSocket() {
  ErrorCode ec;
  m_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
  m_socket.close(ec);
}

void P2PServerConnection::OnConnectionClosed() {
  auto owner = m_owner.lock();
  if (owner) {
    owner->OnConnectionClosed(m_id);
    m_owner.reset();
  }
}

}  // namespace zil::p2p
