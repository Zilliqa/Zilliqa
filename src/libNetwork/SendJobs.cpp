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

#include <pthread.h>
#include <chrono>
#include <deque>
#include <functional>
#include <thread>

#include <boost/asio.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

#include "SendJobs.h"

#include "Blacklist.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "libUtils/SetThreadName.h"

namespace send_jobs {

using AsioContext = boost::asio::io_context;
using Socket = boost::asio::ip::tcp::socket;
using Endpoint = boost::asio::ip::tcp::endpoint;
using SteadyTimer = boost::asio::steady_timer;
using ErrorCode = boost::system::error_code;
using Milliseconds = std::chrono::milliseconds;

const ErrorCode OPERATION_ABORTED = boost::asio::error::operation_aborted;
const ErrorCode END_OF_FILE = boost::asio::error::eof;
const ErrorCode TIMED_OUT = boost::asio::error::timed_out;
const ErrorCode HOST_UNREACHABLE = boost::asio::error::host_unreachable;
const ErrorCode CONN_REFUSED = boost::asio::error::connection_refused;
const ErrorCode NETWORK_DOWN = boost::asio::error::network_down;
const ErrorCode NETWORK_UNREACHABLE = boost::asio::error::network_unreachable;

namespace {

inline bool IsBlacklisted(const Peer& peer, bool allow_relaxed_blacklist) {
  return Blacklist::GetInstance().Exist(peer.m_ipAddress,
                                        !allow_relaxed_blacklist);
}

inline bool IsHostHavingNetworkIssue(const ErrorCode& ec) {
  return (ec == HOST_UNREACHABLE || ec == TIMED_OUT || ec == NETWORK_DOWN ||
          ec == NETWORK_UNREACHABLE);
}

inline bool IsNodeNotRunning(const ErrorCode& ec) { return ec == CONN_REFUSED; }

inline Milliseconds Clock() {
  return std::chrono::duration_cast<Milliseconds>(
      boost::asio::steady_timer::clock_type::now().time_since_epoch());
}

/// Waits for timer and calls a member function of Object,
/// Timer must be in the scope of object (for pointers validity)
template <typename Object, typename Time>
void WaitTimer(SteadyTimer& timer, Time delay, Object* obj,
               void (Object::*OnTimer)()) {
  ErrorCode ec;
  timer.expires_at(
      boost::asio::steady_timer::clock_type::time_point(Clock() + delay), ec);

  if (ec) {
    LOG_GENERAL(FATAL, "Cannot set timer");
    return;
  }

  timer.async_wait([obj, OnTimer](const ErrorCode& error) {
    if (!error) {
      (obj->*OnTimer)();
    }
  });
}

/// Closes socket gracefully, waits for EOF first. Helps to avoid undesirable
/// TCP states on both sides
class GracefulCloseImpl
    : public std::enable_shared_from_this<GracefulCloseImpl> {
  Socket m_socket;
  std::array<uint8_t, 8> m_dummyArray;

 public:
  GracefulCloseImpl(Socket socket) : m_socket(std::move(socket)) {}

  void Close() {
    m_socket.async_read_some(
        boost::asio::mutable_buffer(m_dummyArray.data(), m_dummyArray.size()),
        [self = shared_from_this()](const ErrorCode& ec, size_t n) {
          if (ec != END_OF_FILE) {
            LOG_GENERAL(INFO,
                        "Expected EOF, got ec=" << ec.message() << " n=" << n);
          }
        });
  }
};

}  // namespace

void CloseGracefully(Socket socket) {
  ErrorCode ec;
  if (socket.is_open()) {
    socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
    if (ec) {
      LOG_GENERAL(INFO, "Shutdown error, ec=" << ec.message());
      return;
    }
    std::make_shared<GracefulCloseImpl>(std::move(socket))->Close();
  }
}

class PeerSendQueue : public std::enable_shared_from_this<PeerSendQueue> {
 public:
  using RawMessage = SendJobs::RawMessage;

  struct Item {
    RawMessage msg;
    bool allow_relaxed_blacklist;
    Milliseconds expires_at;
  };

  using DoneCallback = std::function<void(const Peer& peer, ErrorCode ec)>;

  PeerSendQueue(AsioContext& ctx, const DoneCallback& done_cb, Peer peer)
      : m_asioContext(ctx),
        m_doneCallback(done_cb),
        m_peer(std::move(peer)),
        m_socket(m_asioContext),
        m_timer(m_asioContext),
        m_expireTime(std::max(5000u, TX_DISTRIBUTE_TIME_IN_MS * 3 / 4)) {}

  ~PeerSendQueue() { Close(); }

  void Enqueue(RawMessage msg, bool allow_relaxed_blacklist) {
    m_queue.emplace_back();
    auto& item = m_queue.back();
    item.msg = std::move(msg);
    item.allow_relaxed_blacklist = allow_relaxed_blacklist;
    item.expires_at = Clock() + m_expireTime;
    if (m_queue.size() == 1) {
      Connect();
    }
  }

  void Close() {
    m_closed = true;
    CloseGracefully(std::move(m_socket));
  }

 private:
  void Connect() {
    ErrorCode ec;

    if (m_endpoint.port() == 0) {
      auto address =
          boost::asio::ip::make_address(m_peer.GetPrintableIPAddress(), ec);
      if (ec) {
        LOG_GENERAL(INFO, "Cannot create endpoint for address "
                              << m_peer.GetPrintableIPAddress() << ":"
                              << m_peer.GetListenPortHost());
        Done(ec);
        return;
      }
      m_endpoint = Endpoint(std::move(address), m_peer.GetListenPortHost());
    }

    m_timer.cancel(ec);

    LOG_GENERAL(DEBUG, "Connecting to " << m_peer);

    m_socket.async_connect(m_endpoint,
                           [self = shared_from_this()](const ErrorCode& ec) {
                             if (ec != OPERATION_ABORTED) {
                               self->OnConnected(ec);
                             }
                           });
  }

  void OnConnected(const ErrorCode& ec) {
    if (m_closed) {
      return;
    }
    if (!ec) {
      SendMessage();
    } else {
      ScheduleReconnectOrGiveUp(ec);
    }
  }

  void SendMessage() {
    if (!CheckAgainstBlacklist()) {
      return;
    }

    auto& msg = m_queue.front().msg;

    LOG_GENERAL(DEBUG, "Sending " << msg.size << " bytes to " << m_peer);

    boost::asio::async_write(
        m_socket, boost::asio::const_buffer(msg.data.get(), msg.size),
        [self = shared_from_this()](const ErrorCode& ec, size_t) {
          if (ec != OPERATION_ABORTED) {
            self->OnWritten(ec);
          }
        });
  }

  /// Deal with blacklist in which peer may have appeared after some delay
  bool CheckAgainstBlacklist() {
    auto sz = m_queue.size();
    if (sz > 0 && IsBlacklisted(m_peer, false)) {
      if (!IsBlacklisted(m_peer, true)) {
        LOG_GENERAL(INFO,
                    "Peer " << m_peer << " is relaxed blacklisted, Q=" << sz);
        // Find 1st item which allows to be sent in non-strict blacklist mode
        while (!m_queue.empty()) {
          auto& item = m_queue.front();
          if (item.allow_relaxed_blacklist) {
            break;
          }
          m_queue.pop_front();
        }
      } else {
        // the peer is blacklisted strictly
        LOG_GENERAL(INFO,
                    "Peer " << m_peer << " is strictly blacklisted, Q=" << sz);
        m_queue.clear();
      }
    }

    if (m_queue.empty()) {
      Done();
      return false;
    }

    return true;
  }

  void OnWritten(const ErrorCode& ec) {
    if (m_closed) {
      return;
    }

    if (ec) {
      ScheduleReconnectOrGiveUp(ec);
      return;
    }

    if (m_queue.empty()) {
      // impossible
      LOG_GENERAL(WARNING, "Unexpected queue state, peer="
                               << m_peer.GetPrintableIPAddress() << ":"
                               << m_peer.GetListenPortHost());
      Done();
      return;
    }

    m_queue.pop_front();

    Reconnect();
  }

  bool ExpiredOrDone(const ErrorCode& ec = ErrorCode{}) {
    if (m_queue.empty()) {
      Done();
      return true;
    }

    if (m_queue.front().expires_at < Clock()) {
      Done(ec ? ec : TIMED_OUT);
      return true;
    }

    return false;
  }

  void ScheduleReconnectOrGiveUp(const ErrorCode& ec) {
    if (ExpiredOrDone(ec)) {
      return;
    }

    assert(ec);

    WaitTimer(m_timer, Milliseconds(1000), this, &PeerSendQueue::Reconnect);
  }

  void Reconnect() {
    if (!CheckAgainstBlacklist() || ExpiredOrDone()) {
      return;
    }

    // TODO the current protocol is weird and it assumes reconnecting every
    // time. This should be changed!!!
    CloseGracefully(std::move(m_socket));
    m_socket = Socket(m_asioContext);
    Connect();
  }

  void Done(const ErrorCode& ec = ErrorCode{}) {
    if (!m_closed) {
      m_doneCallback(m_peer, ec);
    }
  }

  AsioContext& m_asioContext;
  const DoneCallback& m_doneCallback;

  Peer m_peer;

  Endpoint m_endpoint;

  std::deque<Item> m_queue;
  Socket m_socket;

  SteadyTimer m_timer;

  Milliseconds m_expireTime;

  bool m_closed = false;
};

class SendJobsImpl : public SendJobs,
                     public std::enable_shared_from_this<SendJobsImpl> {
 public:
  SendJobsImpl()
      : m_doneCallback([this](const Peer& peer, ErrorCode ec) {
          OnPeerQueueFinished(peer, ec);
        }),
        m_workerThread([this] { WorkerThread(); }) {}

  ~SendJobsImpl() override {
    LOG_MARKER();
    m_asioCtx.stop();
    m_workerThread.join();
  }

 private:
  void SendMessageToPeer(const Peer& peer, RawMessage message,
                         bool allow_relaxed_blacklist) override {
    if (peer.m_listenPortHost == 0) {
      LOG_GENERAL(INFO, "Ignoring message to peer " << peer);
      return;
    }

    LOG_GENERAL(DEBUG, "Enqueueing message, size=" << message.size);

    // this fn enqueues the lambda to be executed on WorkerThread with
    // sequential guarantees for messages from every calling thread
    m_asioCtx.post([this, peer = peer, msg = std::move(message),
                    allow_relaxed_blacklist]() mutable {
      OnNewJob(std::forward<Peer>(peer), std::forward<RawMessage>(msg),
               allow_relaxed_blacklist);
    });
  }

  void SendMessageToPeerSynchronous(const Peer& peer, const zbytes& message,
                                    uint8_t start_byte) override {
    LOG_MARKER();

    AsioContext localCtx(1);

    auto doneCallback = [&](const Peer& peer, ErrorCode ec) {
      auto peerStr = peer.GetPrintableIPAddress();
      if (ec) {
        LOG_GENERAL(WARNING, "Send message to "
                                 << peerStr
                                 << " failed with error: " << ec.message());
      } else {
        LOG_GENERAL(INFO, "Send message to " << peerStr << " done");
      }
      localCtx.stop();
    };

    auto peerCtx = std::make_shared<PeerSendQueue>(localCtx, doneCallback,
                                                   std::move(peer));
    peerCtx->Enqueue(CreateMessage(message, {}, start_byte), false);

    localCtx.run();

    peerCtx->Close();
  }

  void OnNewJob(Peer&& peer, RawMessage&& msg, bool allow_relaxed_blacklist) {
    if (IsBlacklisted(peer, allow_relaxed_blacklist)) {
      LOG_GENERAL(INFO,
                  "Ignoring blacklisted peer " << peer.GetPrintableIPAddress());
      return;
    }

    auto& ctx = m_activePeers[peer];
    if (!ctx) {
      ctx = std::make_shared<PeerSendQueue>(m_asioCtx, m_doneCallback,
                                            std::move(peer));
    }
    ctx->Enqueue(std::move(msg), allow_relaxed_blacklist);
  }

  void OnPeerQueueFinished(const Peer& peer, ErrorCode ec) {
    if (ec) {
      LOG_GENERAL(
          INFO, "Peer queue finished, peer=" << peer.GetPrintableIPAddress()
                                             << ":" << peer.GetListenPortHost()
                                             << " ec=" << ec.message());
    }

    auto it = m_activePeers.find(peer);
    if (it == m_activePeers.end()) {
      // impossible
      return;
    }

    if (IsHostHavingNetworkIssue(ec)) {
      if (Blacklist::GetInstance().IsWhitelistedSeed(peer.m_ipAddress)) {
        LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                 << ec.value() << " (" << ec.message()
                                 << "). Adding seed "
                                 << peer.GetPrintableIPAddress()
                                 << " as relaxed blacklisted");
        // Add this seed node to relaxed blacklist even if it is whitelisted
        // in general.
        Blacklist::GetInstance().Add(peer.m_ipAddress, false, true);
      } else {
        LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                 << ec.value() << " (" << ec.message()
                                 << "). Adding " << peer.GetPrintableIPAddress()
                                 << " as strictly blacklisted");
        Blacklist::GetInstance().Add(peer.m_ipAddress);  // strict
      }
    } else if (IsNodeNotRunning(ec)) {
      LOG_GENERAL(WARNING, "[blacklist] Encountered "
                               << ec.value() << " (" << ec.message()
                               << "). Adding " << peer.GetPrintableIPAddress()
                               << " as relaxed blacklisted");
      Blacklist::GetInstance().Add(peer.m_ipAddress, false);
    }

    // explicit Close() because shared_ptr may be reused in async operation
    it->second->Close();
    m_activePeers.erase(it);
  }

  void WorkerThread() {
    utility::SetThreadName("SendJobs");

    // Need this workaround to prevent the event loop from returning when there
    // are no current jobs
    boost::asio::signal_set sig(m_asioCtx, SIGABRT);
    sig.async_wait([](const ErrorCode&, int) {});

    LOG_GENERAL(INFO, "SendJobs event loop is starting");
    m_asioCtx.run();
    LOG_GENERAL(INFO, "SendJobs event loop stopped");
  }

  AsioContext m_asioCtx;
  PeerSendQueue::DoneCallback m_doneCallback;
  std::thread m_workerThread;
  std::map<Peer, std::shared_ptr<PeerSendQueue>> m_activePeers;
};

}  // namespace send_jobs

std::shared_ptr<SendJobs> SendJobs::Create() {
  return std::make_shared<send_jobs::SendJobsImpl>();
}

SendJobs::RawMessage SendJobs::CreateMessage(const zbytes& message,
                                             const zbytes& msg_hash,
                                             uint8_t start_byte) {
  // Transmission format:
  // 0x01 ~ 0xFF - version, defined in constant file
  // 0xLL 0xLL - 2-byte NETWORK_ID, defined in constant file
  // 0x11 - start byte
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
  // <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0xLL 0xLL - 2-byte NETWORK_ID, defined in constant file
  // 0x22 - start byte (broadcast)
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
  // <32-byte hash> <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0xLL 0xLL - 2-byte NETWORK_ID, defined in constant file
  // 0x33 - start byte (report)
  // 0x00 0x00 0x00 0x01 - 4-byte length of message
  // 0x00

  assert(msg_hash.empty() || msg_hash.size() == 32);

  static const size_t HDR_LEN = 8;

  RawMessage ret;

  if (message.empty()) {
    LOG_GENERAL(WARNING, "Message is empty");
    return ret;
  }

  size_t length = msg_hash.size() + message.size();

  ret.size = HDR_LEN + length;
  uint8_t* buf = (uint8_t*)malloc(ret.size);
  assert(buf);
  if (!buf) {
    throw std::bad_alloc{};
  }

  ret.data.reset(buf, [](void* d) { free(d); });

  *buf++ = MSG_VERSION & 0xFF;
  *buf++ = (NETWORK_ID >> 8) & 0xFF;
  *buf++ = NETWORK_ID & 0xFF;
  *buf++ = start_byte;
  *buf++ = (length >> 24) & 0xFF;
  *buf++ = (length >> 16) & 0xFF;
  *buf++ = (length >> 8) & 0xFF;
  *buf++ = length & 0xFF;

  if (!msg_hash.empty()) {
    auto sz = msg_hash.size();
    memcpy(buf, msg_hash.data(), sz);
    buf += sz;
  }

  memcpy(buf, message.data(), message.size());

  return ret;
}
