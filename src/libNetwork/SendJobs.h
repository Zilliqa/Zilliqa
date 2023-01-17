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

#ifndef ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_
#define ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_

#include <memory>

#include "Peer.h"

class SendJobs {
 public:
  struct RawMessage {
    // shared_ptr here is for not to duplicate broadcast messages
    std::shared_ptr<const void> data;
    size_t size = 0;
  };

  static std::shared_ptr<SendJobs> Create();

  /// Serializes a message
  static RawMessage CreateMessage(const zbytes& message, const zbytes& msg_hash,
                                  uint8_t start_byte);

  virtual ~SendJobs() = default;

  /// Enqueues message to be sent to peer
  virtual void SendMessageToPeer(const Peer& peer, RawMessage message,
                                 bool allow_relaxed_blacklist) = 0;

  /// Helper for the function above, for the most common case
  void SendMessageToPeer(const Peer& peer, const zbytes& message,
                         uint8_t start_byte) {
    static const zbytes no_hash;
    SendMessageToPeer(peer, CreateMessage(message, no_hash, start_byte), false);
  }

  /// Sends message to peer in the current thread, without queueing.
  /// WARNING: it blocks
  virtual void SendMessageToPeerSynchronous(const Peer& peer,
                                            const zbytes& message,
                                            uint8_t start_byte) = 0;
};

#endif  // ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_
