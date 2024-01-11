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

#include "P2PMessage.h"

#include <deque>

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#ifndef ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_
#define ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_

namespace boost::asio {
class io_context;
}

namespace zil::p2p {

using AsioContext = boost::asio::io_context;
using DeadlineTimer = boost::asio::deadline_timer;
using TcpSocket = boost::asio::ip::tcp::socket;
using TcpAcceptor = boost::asio::ip::tcp::acceptor;
using TcpEndpoint = boost::asio::ip::tcp::endpoint;
using ErrorCode = boost::system::error_code;

/// P2P messages server. See wire protocol details in P2PMessage.h
class P2PServer {
 public:
  /// Callback for incoming messages
  using Callback = std::function<bool(const Peer& from, ReadMessageResult&)>;

  /// Creates an instance and starts listening. This fn may throw on errors
  static std::shared_ptr<P2PServer> CreateAndStart(AsioContext& asio,
                                                   uint16_t port,
                                                   size_t max_message_size,
                                                   bool additional_server,
                                                   Callback callback);
  /// Closes gracefully
  virtual ~P2PServer() = default;
};

class P2PServerImpl;

class P2PServerConnection
    : public std::enable_shared_from_this<P2PServerConnection> {
 public:
  P2PServerConnection(std::weak_ptr<P2PServerImpl> owner, uint64_t this_id,
                      Peer&& remote_peer, TcpSocket socket,
                      size_t max_message_size, bool additional_server);

  void StartReading();

  void Close();

  void SendMessage(RawMessage msg);

  bool IsAdditionalServer() const { return m_additionalServer; }

 private:
  void OnHeaderRead(const ErrorCode& ec);

  void OnBodyRead(const ErrorCode& ec);

  void ReadNextMessage();

  void SetupHeartBeat();

  void OnHeartBeat(const ErrorCode& ec);

  void CloseSocket();

  void OnConnectionClosed();

  void OnSend(const boost::system::error_code& ec);

  std::weak_ptr<P2PServerImpl> m_owner;
  uint64_t m_id;
  Peer m_remotePeer;
  TcpSocket m_socket;
  DeadlineTimer m_timer;
  std::chrono::steady_clock::time_point m_last_time_packet_received;
  bool m_is_marked_as_closed;
  size_t m_maxMessageSize;
  bool m_additionalServer;
  zbytes m_readBuffer;
  std::deque<RawMessage> m_sendQueue;
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_
