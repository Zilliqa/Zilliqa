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
    std::shared_ptr<const void> data;
    size_t size = 0;
  };

  static std::shared_ptr<SendJobs> Create();

  static RawMessage CreateMessage(const bytes& message, const bytes& msg_hash,
                                  uint8_t start_byte);

  virtual ~SendJobs() = default;

  virtual void SendMessageToPeer(const Peer& peer, RawMessage message,
                                 bool allow_relaxed_blacklist) = 0;

  void SendMessageToPeer(const Peer& peer, const bytes& message,
                         uint8_t start_byte) {
    static const bytes no_hash;
    SendMessageToPeer(peer, CreateMessage(message, no_hash, start_byte), false);
  }
};

#endif  // ZILLIQA_SRC_LIBNETWORK_SENDJOBS_H_
