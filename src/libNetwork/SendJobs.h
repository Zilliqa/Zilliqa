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

#ifndef ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_
#define ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_

#include "libNetwork/P2PMessage.h"

namespace zil::p2p {

class SendJobs {
 public:
  static std::shared_ptr<SendJobs> Create(Dispatcher dispatcher);

  // std::mutex m_mutexTemp;
  // struct Connections {
  //   int failures = 0;
  //   int successes = 0;
  // };
  // int iterations = 0;
  // std::map<std::string, Connections> sendJobsConnectionList;

  virtual ~SendJobs() = default;

  /// Enqueues message to be sent to peer
  virtual void SendMessageToPeer(const Peer& peer, RawMessage message,
                                 bool allow_relaxed_blacklist) = 0;

  /// Helper for the function above, for the most common case
  void SendMessageToPeer(const Peer& peer, const zbytes& message,
                         uint8_t start_byte, bool inject_trace_context) {
    static const zbytes no_hash;
    SendMessageToPeer(
        peer, CreateMessage(message, no_hash, start_byte, inject_trace_context),
        false);
  }

  /// Sends message to peer in the current thread, without queueing.
  /// WARNING: it blocks
  virtual void SendMessageToPeerSynchronous(const Peer& peer,
                                            const zbytes& message,
                                            uint8_t start_byte,
                                            Dispatcher dispatcher) = 0;
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_
