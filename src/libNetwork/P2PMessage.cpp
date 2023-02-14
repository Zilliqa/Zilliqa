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
#include "libMetrics/Helper.h"
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
  assert(msg_hash.empty() || msg_hash.size() == 32);

  if (message.empty()) {
    LOG_GENERAL(WARNING, "Message is empty");
    return {};
  }

  std::string trace_info;

  // TODO make build setting #if TRACES_ENABLED from Cmake

  if (inject_trace_context) {
    trace::ExtractTraceInfoFromActiveSpan(trace_info);
  }

  size_t trace_size = trace_info.size();

  size_t total_size = msg_hash.size() + message.size() + trace_size;
  size_t buf_size_with_header = HDR_LEN + total_size;

  uint8_t* buf_base = (uint8_t*)malloc(buf_size_with_header);
  assert(buf_base);
  if (!buf_base) {
    throw std::bad_alloc{};
  }
  auto buf = buf_base;

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
  if (!buf || buf_size < HDR_LEN) {
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

  const uint16_t networkid = (buf[1] << 8) + buf[2];
  if (networkid != NETWORK_ID) {
    LOG_GENERAL(WARNING, "Header networkid wrong, received ["
                             << networkid << "] while expected [" << NETWORK_ID
                             << "].");
    return ReadState::WRONG_NETWORK_ID;
  }

  result.startByte = buf[3];

  uint32_t total_length =
      (buf[4] << 24) + (buf[5] << 16) + (buf[6] << 8) + buf[7];

  result.totalMessageBytes = HDR_LEN + total_length;
  if (buf_size < result.totalMessageBytes) {
    return ReadState::NOT_ENOUGH_DATA;
  }

  uint32_t trace_length = 0;

  if (version == MsgVersionWithTraces()) {
    if (total_length < 5) {
      LOG_GENERAL(WARNING, "Invalid length [" << total_length << "]");
      return ReadState::WRONG_MESSAGE_LENGTH;
    }

    trace_length = (buf[8] << 24) + (buf[9] << 16) + (buf[10] << 8) + buf[11];
    if (trace_length == 0 || trace_length > total_length) {
      LOG_GENERAL(WARNING,
                  "Invalid trace info length [" << trace_length << "]");
      return ReadState::WRONG_TRACE_LENGTH;
    }

    buf += 12;

    const char* trace_buf =
        reinterpret_cast<const char*>(buf + total_length - trace_length);

    result.traceInfo.assign(trace_buf, trace_length);

  } else {
    buf += 8;
  }

  assert(total_length >= trace_length);

  size_t msg_length = total_length - trace_length;

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
