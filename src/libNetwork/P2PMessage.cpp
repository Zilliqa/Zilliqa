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

#include "P2PMessage.h"

#include "common/Constants.h"
#include "libMetrics/Tracing.h"
#include "libUtils/Logger.h"

namespace zil::p2p {

namespace {

inline uint8_t MsgVersionWithTraces() {
  assert(MSG_VERSION < 128);
  return uint8_t(MSG_VERSION) + 128;
}

}  // namespace

RawMessage::RawMessage(uint8_t* buf, size_t sz)
    : data(buf, [](void* d) { free(d); }), size(sz) {}

RawMessage CreateMessage(const zbytes& message, const zbytes& msg_hash,
                         uint8_t start_byte, bool inject_trace_context) {
  assert(msg_hash.empty() || msg_hash.size() == HASH_LEN);

  if (message.empty()) {
    LOG_GENERAL(WARNING, "Message is empty");
    return {};
  }

  std::string_view trace_info;
  if (inject_trace_context) {
    trace_info = zil::trace::Tracing::GetActiveSpan().GetIds();
  }

  size_t trace_size = trace_info.size();

  size_t total_size = msg_hash.size() + message.size() + trace_size;
  if (trace_size != 0) {
    total_size += 4;
  }

  size_t buf_size_with_header = HDR_LEN + total_size;

  uint8_t* buf_base = (uint8_t*)malloc(buf_size_with_header);
  assert(buf_base);
  if (!buf_base) {
    throw std::bad_alloc{};
  }
  auto* buf = buf_base;

  uint8_t version = (trace_size != 0) ? MsgVersionWithTraces() : MSG_VERSION;
  *buf++ = version;

  *buf++ = (NETWORK_ID >> 8) & 0xFF;
  *buf++ = NETWORK_ID & 0xFF;

  *buf++ = start_byte;

  *buf++ = (total_size >> 24) & 0xFF;
  *buf++ = (total_size >> 16) & 0xFF;
  *buf++ = (total_size >> 8) & 0xFF;
  *buf++ = total_size & 0xFF;

  if (trace_size != 0) {
    *buf++ = (trace_size >> 24) & 0xFF;
    *buf++ = (trace_size >> 16) & 0xFF;
    *buf++ = (trace_size >> 8) & 0xFF;
    *buf++ = trace_size & 0xFF;
  }

  if (!msg_hash.empty()) {
    auto sz = msg_hash.size();
    memcpy(buf, msg_hash.data(), sz);
    buf += sz;
  }

  memcpy(buf, message.data(), message.size());

  if (trace_size != 0) {
    buf += message.size();
    memcpy(buf, trace_info.data(), trace_size);
  }

  return RawMessage(buf_base, buf_size_with_header);
}

ReadState TryReadMessage(const uint8_t* buf, size_t buf_size,
                         ReadMessageResult& result) {
  auto ReadU32BE = [](const uint8_t* bytes) -> uint32_t {
    return (uint32_t(bytes[0]) << 24) + (uint32_t(bytes[1]) << 16) +
           (uint32_t(bytes[2]) << 8) + bytes[3];
  };

  if (!buf || buf_size < HDR_LEN) {
    LOG_GENERAL(WARNING, "Not enough data to read message header");
    return ReadState::NOT_ENOUGH_DATA;
  }

  auto version = buf[0];

  // Check for version requirement
  if (version != (unsigned char)(MSG_VERSION & 0xFF) &&
      version != MsgVersionWithTraces()) {
    LOG_GENERAL(WARNING, "Header version wrong, received ["
                             << version - 0x00 << "] while expected ["
                             << MSG_VERSION << "] or ["
                             << MsgVersionWithTraces() << "]");
    return ReadState::WRONG_MSG_VERSION;
  }

  const uint16_t networkid = (uint16_t(buf[1]) << 8) + buf[2];
  if (networkid != NETWORK_ID) {
    LOG_GENERAL(WARNING, "Header networkid wrong, received ["
                             << networkid << "] while expected [" << NETWORK_ID
                             << "].");
    return ReadState::WRONG_NETWORK_ID;
  }

  result.startByte = buf[3];

  uint32_t length_of_remaining_message = ReadU32BE(buf + 4);

  result.totalMessageBytes = HDR_LEN + length_of_remaining_message;
  if (buf_size < result.totalMessageBytes) {
    return ReadState::NOT_ENOUGH_DATA;
  }

  // For non-broadcast messages w/o trace info
  uint32_t msg_length = length_of_remaining_message;
  uint32_t trace_length = 0;

  buf += HDR_LEN;

  if (version == MsgVersionWithTraces()) {
    if (length_of_remaining_message < 5) {
      LOG_GENERAL(WARNING,
                  "Invalid length [" << length_of_remaining_message << "]");
      return ReadState::WRONG_MESSAGE_LENGTH;
    }

    trace_length = ReadU32BE(buf);
    if (trace_length == 0 || trace_length > length_of_remaining_message - 4) {
      LOG_GENERAL(WARNING,
                  "Invalid trace info length [" << trace_length << "]");
      return ReadState::WRONG_TRACE_LENGTH;
    }

    const char* trace_buf = reinterpret_cast<const char*>(
        buf + length_of_remaining_message - trace_length);

    result.traceInfo.assign(trace_buf, trace_length);

    buf += 4;
    msg_length -= (4 + trace_length);
  }

  if (result.startByte == START_BYTE_BROADCAST) {
    if (msg_length < HASH_LEN) {
      LOG_GENERAL(WARNING,
                  "Invalid broadcast message length [" << msg_length << "]");
      return ReadState::WRONG_MESSAGE_LENGTH;
    }

    result.hash.assign(buf, buf + HASH_LEN);

    buf += HASH_LEN;
    msg_length -= HASH_LEN;
  }

  if (msg_length > 0) {
    result.message.assign(buf, buf + msg_length);
  }

  return ReadState::SUCCESS;
}

}  // namespace zil::p2p
