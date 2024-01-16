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

#include <functional>
#include <memory>

#include "Peer.h"

#ifndef ZILLIQA_SRC_LIBNETWORK_P2PMESSAGE_H_
#define ZILLIQA_SRC_LIBNETWORK_P2PMESSAGE_H_

namespace zil::p2p {

constexpr unsigned char START_BYTE_NORMAL = 0x11;
constexpr unsigned char START_BYTE_BROADCAST = 0x22;
constexpr unsigned char START_BYTE_GOSSIP = 0x33;
constexpr size_t HDR_LEN = 8;
constexpr size_t HASH_LEN = 32;

// These types are disabled in updated protocol
// constexpr unsigned char START_BYTE_SEED_TO_SEED_REQUEST = 0x44;
// constexpr unsigned char START_BYTE_SEED_TO_SEED_RESPONSE = 0x55;

class P2PServerConnection;
using P2PConnPtr = std::shared_ptr<P2PServerConnection>;

struct Message {
  P2PConnPtr connection;
  zbytes msg;                // P2P protocol message
  std::string traceContext;  // trace context serialized
  Peer from;                 // endpoint
  uint8_t startByte = 0;     // START_BYTE_*
};

using Dispatcher = std::function<void(std::shared_ptr<Message>)>;

struct RawMessage {
  // shared_ptr here is for not to duplicate broadcast messages
  std::shared_ptr<const void> data;
  size_t size = 0;

  RawMessage() = default;

  RawMessage(uint8_t* buf, size_t sz);
};

/* Wire format:

 1) Header: 4 bytes
    VERSION:    1 byte              MSG_VERSION or MSG_VERSION_WITH_TRACES
    NETWORK_ID: 2 bytes big endian  NETWORK_ID from constants.xml
    START_BYTE: 1 byte              START_BYTE_*, see above

 2) Total size of remaining message: 4 bytes big endian

 2opt) Only if VERSION==MSG_VERSION_WITH_TRACES
     Size of trace information: 4 bytes big endian

 3opt) Only if START_BYTE==START_BYTE_BROADCAST
       Hash: 32 bytes

 3) Raw message

 4opt) Only if VERSION==MSG_VERSION_WITH_TRACES
       Trace information
*/

/// Serializes a message
RawMessage CreateMessage(const zbytes& message, const zbytes& msg_hash,
                         uint8_t start_byte, bool inject_trace_context);

enum class ReadState {
  NOT_ENOUGH_DATA,
  SUCCESS,
  WRONG_MSG_VERSION,
  WRONG_NETWORK_ID,
  WRONG_MESSAGE_LENGTH,
  WRONG_TRACE_LENGTH
};

struct ReadMessageResult {
  explicit ReadMessageResult(P2PConnPtr conn) : connection(conn) {}

  /// Connection associated with received message
  P2PConnPtr connection;

  /// START_BYTE_*
  uint8_t startByte = 0;

  /// Raw binary message
  zbytes message;

  /// Non-empty hash for broadcast messages
  zbytes hash;

  /// Non-empty trace information if raw message contained it
  std::string traceInfo;

  /// Total bytes consumed from wire
  size_t totalMessageBytes = 0;
};

ReadState TryReadMessage(const uint8_t* buf, size_t buf_size,
                         ReadMessageResult& result);

inline std::shared_ptr<Message> MakeMsg(P2PConnPtr connection, zbytes msg,
                                        Peer peer, uint8_t startByte,
                                        std::string& traceContext) {
  auto r = std::make_shared<Message>();
  r->connection = connection;
  r->msg = std::move(msg);
  r->traceContext = std::move(traceContext);
  r->from = std::move(peer);
  r->startByte = startByte;
  return r;
}

inline uint32_t ReadU32BE(const uint8_t* bytes) {
  return (uint32_t(bytes[0]) << 24) + (uint32_t(bytes[1]) << 16) +
         (uint32_t(bytes[2]) << 8) + bytes[3];
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_P2PMESSAGE_H_
