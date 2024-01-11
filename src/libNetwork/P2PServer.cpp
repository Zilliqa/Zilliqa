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

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/container/small_vector.hpp>

#include "Blacklist.h"
#include "libUtils/Logger.h"

namespace zil::p2p {

const ErrorCode OPERATION_ABORTED = boost::asio::error::operation_aborted;
const ErrorCode END_OF_FILE = boost::asio::error::eof;

// How long it has to elapse from last successful read to mark connection as
// stale
constexpr auto INACTIVITY_TIME_TO_CLOSE = std::chrono::seconds(3600);
// How often we should check if the connection is still 'alive'
constexpr auto HEARTBEAT_CHECK = std::chrono::seconds(60);

class P2PServerImpl;

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
                bool additional_server, Callback callback)
      : m_acceptor(std::move(acceptor)),
        m_maxMessageSize(max_message_size),
        m_additionalServer(additional_server),
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
          if (!wptr.expired()) {
            auto parent = wptr.lock();
            if (!ec) {
              parent->OnAccept(ec, std::move(sock));
            } else {
              LOG_GENERAL(DEBUG, "Got an error from Accept in P2P Server: "
                                     << ec.message());
            }
            parent->AcceptNextConnection();
          } else {
            LOG_GENERAL(DEBUG,
                        "Parent doesn't exist anymore, this may happen only "
                        "during shutdown phase of Zilliqa");
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
        LOG_GENERAL(WARNING, "Couldn't get the IP address from remove socket!");
        return;
      }
      LOG_GENERAL(DEBUG, "Accepted new connection from: "
                             << maybe_peer.value().GetPrintableIPAddress());
      auto conn = std::make_shared<P2PServerConnection>(
          weak_from_this(), ++m_counter,
          std::move(maybe_peer.value()), /*m_asio,*/
          std::move(socket), m_maxMessageSize, m_additionalServer);

      conn->StartReading();

      m_connections[m_counter] = std::move(conn);
      LOG_GENERAL(DEBUG,
                  "Total incoming connections: " << m_connections.size());
      // TODO Metric
    } else {
      LOG_GENERAL(FATAL, "Error in accept : " << ec.message());
    }
  }

  TcpAcceptor m_acceptor;
  size_t m_maxMessageSize;
  bool m_additionalServer;
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
                                                     bool additional_server,
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
        std::move(socket), /*asio,*/ max_message_size, additional_server,
        std::move(callback));

    // this call is needed to be decoupled from ctor, because in the ctor
    // it's impossible to get weak_from_this(), such a subtliety
    server->AcceptNextConnection();

    return server;

  } catch (const boost::system::system_error& e) {
    error_msg = e.what();
  }

  throw std::runtime_error(error_msg);
}

P2PServerConnection::P2PServerConnection(std::weak_ptr<P2PServerImpl> owner,
                                         uint64_t this_id, Peer&& remote_peer,
                                         TcpSocket socket,
                                         size_t max_message_size,
                                         bool additional_server)
    : m_owner(std::move(owner)),
      m_id(this_id),
      m_remotePeer(std::move(remote_peer)),
      m_socket(std::move(socket)),
      m_timer(m_socket.get_executor()),
      m_last_time_packet_received(std::chrono::steady_clock::now()),
      m_is_marked_as_closed(false),
      m_maxMessageSize(max_message_size),
      m_additionalServer(additional_server) {}

void P2PServerConnection::StartReading() {
  SetupHeartBeat();
  ReadNextMessage();
}

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
        if (ec) {
          // LOG_GENERAL(WARNING, "Got error code: " << ec.message());
        }
        if (ec != OPERATION_ABORTED) {
          self->OnHeaderRead(ec);
        }
      });
}

void P2PServerConnection::OnHeaderRead(const ErrorCode& ec) {
  if (ec) {
    LOG_GENERAL(DEBUG,
                "Peer " << m_remotePeer << " read error: " << ec.message());
    CloseSocket();
    OnConnectionClosed();
    return;
  }

  assert(m_readBuffer.size() == HDR_LEN);
  m_last_time_packet_received = std::chrono::steady_clock::now();
  auto remainingLength = ReadU32BE(m_readBuffer.data() + 4);
  if (remainingLength > m_maxMessageSize) {
    LOG_GENERAL(WARNING, "[blacklist] Encountered data of size: "
                             << remainingLength << " being received."
                             << " Adding sending node "
                             << m_remotePeer.GetPrintableIPAddress()
                             << " as strictly blacklisted");
    Blacklist::GetInstance().Add({m_remotePeer.GetIpAddress(),
                                  m_remotePeer.GetListenPortHost(),
                                  m_remotePeer.GetNodeIndentifier()});

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
        if (ec) {
          // LOG_GENERAL(WARNING, "Got error code: " << ec.message());
        }
        if (ec != OPERATION_ABORTED) {
          self->OnBodyRead(ec);
        }
      });
}

void P2PServerConnection::OnBodyRead(const ErrorCode& ec) {
  if (ec) {
    CloseSocket();
    OnConnectionClosed();
    return;
  }
  m_last_time_packet_received = std::chrono::steady_clock::now();
  ReadMessageResult result{shared_from_this()};
  auto state = TryReadMessage(m_readBuffer.data(), m_readBuffer.size(), result);

  if (state != ReadState::SUCCESS) {
    LOG_GENERAL(WARNING, "Message deserialize error: blacklisting "
                             << m_remotePeer.GetPrintableIPAddress());
    Blacklist::GetInstance().Add({m_remotePeer.GetIpAddress(),
                                  m_remotePeer.GetListenPortHost(),
                                  m_remotePeer.GetNodeIndentifier()});

    CloseSocket();
    OnConnectionClosed();
    return;
  }
  auto owner = m_owner.lock();
  if (!owner || !owner->OnMessage(m_id, m_remotePeer, result)) {
    CloseSocket();
    OnConnectionClosed();
    return;
  }

  ReadNextMessage();
}

void P2PServerConnection::SetupHeartBeat() {
  ErrorCode ec;
  m_timer.cancel(ec);
  m_timer.expires_from_now(boost::posix_time::seconds(HEARTBEAT_CHECK.count()));
  m_timer.async_wait(std::bind(&P2PServerConnection::OnHeartBeat,
                               shared_from_this(), std::placeholders::_1));
}

void P2PServerConnection::OnHeartBeat(const zil::p2p::ErrorCode& ec) {
  if (m_is_marked_as_closed) {
    return;
  }

  if (!ec && !m_is_marked_as_closed) {
    // Timer still can expire without error code despite being explicitly
    // cancelled therefore check activity on the socket
    if (std::chrono::steady_clock::now() <
        m_last_time_packet_received + INACTIVITY_TIME_TO_CLOSE) {
      SetupHeartBeat();
      return;
    }
    LOG_GENERAL(DEBUG, "Due to inactivity on socket with peer: "
                           << m_remotePeer.GetPrintableIPAddress()
                           << " connection is closed");
    CloseSocket();
    OnConnectionClosed();
  }
}

void P2PServerConnection::SendMessage(RawMessage msg) {
  boost::asio::post(m_socket.get_executor(), [this, self = shared_from_this(),
                                              msg = std::move(msg)] {
    if (m_is_marked_as_closed) {
      return;
    }

    const auto isSending = !m_sendQueue.empty();
    m_sendQueue.push_back(std::move(msg));
    if (!isSending) {
      const auto& rawMsg = m_sendQueue.front();
      boost::asio::async_write(
          m_socket, boost::asio::const_buffer(rawMsg.data.get(), rawMsg.size),
          std::bind(&P2PServerConnection::OnSend, shared_from_this(),
                    std::placeholders::_1));
    }
  });
}

void P2PServerConnection::OnSend(const boost::system::error_code& ec) {
  if (ec || m_is_marked_as_closed) {
    return;
  }
  m_sendQueue.pop_front();
  if (std::empty(m_sendQueue)) {
    return;
  }
  const auto& rawMsg = m_sendQueue.front();
  boost::asio::async_write(
      m_socket, boost::asio::const_buffer(rawMsg.data.get(), rawMsg.size),
      std::bind(&P2PServerConnection::OnSend, shared_from_this(),
                std::placeholders::_1));
}

void P2PServerConnection::Close() {
  m_is_marked_as_closed = true;
  m_owner.reset();
  CloseSocket();
}

void P2PServerConnection::CloseSocket() {
  m_is_marked_as_closed = true;
  ErrorCode ec;
  m_timer.cancel(ec);
  // both close and shutdown should be none blocking calls certainly on current
  // linux shutdown marks the socket as blocked for both read and write shutdown
  // tells the OS to begin the graceful closedown of the TCP connection. close()
  // is a blocking call that waits for the OS to complete the closedown. close
  // also frees the OS resources from the program so should be called even if an
  // error condition is encountered
  m_socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
  if (ec) {
    m_socket.close(ec);
    return;
  }
  size_t unread = m_socket.available(ec);
  if (ec) {
    m_socket.close(ec);
    return;
  }
  // On Linux, the  close()  function for sockets does not necessarily wait
  // for the operating system to complete the operation before returning.
  // It typically marks the socket as closed and releases any resources
  // associated with it immediately. However, this does not guarantee that all
  // pending data has been sent or received. It is important to handle any
  // necessary error checking and ensure all data transmission is complete
  // before calling  close()  on a socket.
  if (unread > 0) {
    do {
      constexpr size_t BUFF_SIZE = 4096;
      boost::container::small_vector<uint8_t, BUFF_SIZE> buf;
      buf.resize(std::min(unread, BUFF_SIZE));
      m_socket.read_some(boost::asio::mutable_buffer(buf.data(), unread), ec);
    } while (!ec && (unread = m_socket.available(ec)) > 0);
  }
  m_socket.close(ec);
  LOG_GENERAL(DEBUG, "Connection completely closed with peer: "
                         << m_remotePeer.GetPrintableIPAddress());
}

void P2PServerConnection::OnConnectionClosed() {
  auto owner = m_owner.lock();
  if (owner) {
    owner->OnConnectionClosed(m_id);
    m_owner.reset();
  }
}

}  // namespace zil::p2p
