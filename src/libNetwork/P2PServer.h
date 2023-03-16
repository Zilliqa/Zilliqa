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

#ifndef ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_
#define ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_

namespace boost::asio {
class io_context;
}

namespace zil::p2p {

using AsioContext = boost::asio::io_context;

/// P2P messages server. See wire protocol details in P2PMessage.h
class P2PServer {
 public:
  /// Callback for incoming messages
  using Callback = std::function<bool(const Peer& from, ReadMessageResult&)>;

  /// Creates an instance and starts listening. This fn may throw on errors
  static std::shared_ptr<P2PServer> CreateAndStart(AsioContext& asio,
                                                   uint16_t port,
                                                   size_t max_message_size,
                                                   Callback callback);
  /// Closes gracefully
  virtual ~P2PServer() = default;
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_P2PSERVER_H_
