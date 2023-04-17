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
#include <boost/container/small_vector.hpp>

#include "SendJobs.h"

#include "Blacklist.h"
#include "Peer.h"
#include "libMetrics/Api.h"
#include "libUtils/Logger.h"
#include "libUtils/SetThreadName.h"

namespace zil {
namespace local {

class SendJobsVariables {
  std::atomic<int> sendMessageToPeerCount = 0;
  std::atomic<int> sendMessageToPeerFailed = 0;
  std::atomic<int> sendMessageToPeerSyncCount = 0;
  std::atomic<int> activePeersSize = 0;
  std::atomic<int> activeQueueSize = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void AddSendMessageToPeerCount(int count) {
    Init();
    sendMessageToPeerCount += count;
  }

  void AddSendMessageToPeerFailed(int count) {
    Init();
    sendMessageToPeerFailed += count;
  }

  void AddSendMessageToPeerSyncCount(int count) {
    Init();
    sendMessageToPeerSyncCount += count;
  }

  void SetActiveQueueSize(int amount) {
    Init();
    activeQueueSize = amount;
  }

  void SetActivePeersSize(int amount) {
    Init();
    activePeersSize = amount;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "sendjobs.gauge",
                                          "Send Jobs metrics", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(sendMessageToPeerCount.load(), {{"counter", "SendMessageToPeerCount"}});
        result.Set(sendMessageToPeerFailed.load(), {{"counter", "SendMessageToPeerFailed"}});
        result.Set(sendMessageToPeerSyncCount.load(), {{"counter", "SendMessageToPeerSyncCount"}});
        result.Set(activePeersSize.load(), {{"counter", "ActivePeersSize"}});
        result.Set(activeQueueSize.load(), {{"counter", "ActiveQueueSize"}});
      });
    }
  }
};

static SendJobsVariables variables{};

}  // namespace local
}  // namespace zil
namespace zil::p2p {

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
  return (ec == HOST_UNREACHABLE || ec == NETWORK_DOWN ||
          ec == NETWORK_UNREACHABLE);
}

inline bool IsNodeNotRunning(const ErrorCode& ec) { return (ec == TIMED_OUT || ec == CONN_REFUSED); }

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
            LOG_GENERAL(DEBUG,
                        "Expected EOF, got ec=" << ec.message() << " n=" << n);
          }
        });
  }
};

void CloseGracefully(Socket socket) {
  ErrorCode ec;
//  LOG_GENERAL(INFO, "X0003");
  if (!socket.is_open()) {
    return;
  }
  socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
//  LOG_GENERAL(INFO, "X0004");
  if (ec) {
    return;
  }
  size_t unread = socket.available(ec);
  if (ec) {
    return;
  }
//  LOG_GENERAL(INFO, "X0005");
  if (unread > 0) {
    boost::container::small_vector<uint8_t, 4096> buf;
    buf.resize(unread);
    socket.read_some(boost::asio::mutable_buffer(buf.data(), unread), ec);
  }
//  LOG_GENERAL(INFO, "X0006");
  if (!ec) {
    std::make_shared<GracefulCloseImpl>(std::move(socket))->Close();
  }
//  LOG_GENERAL(INFO, "X0007");
}

}  // namespace

class PeerSendQueue : public std::enable_shared_from_this<PeerSendQueue> {
 public:
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

    zil::local::variables.SetActiveQueueSize(m_queue.size());
//    LOG_GENERAL(INFO, "X0001: Message queued to " << m_peer << " len= " << m_queue.size());
  }

  void Close() {
//    LOG_GENERAL(INFO, "X0002: Close() m_closed " << m_closed);
    if (!m_closed) {
      m_closed = true;
      CloseGracefully(std::move(m_socket));
    }
  }

 private:
  void Connect() {
    ErrorCode ec;

//    LOG_GENERAL(INFO, "X0008:" << m_peer);
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
//      LOG_GENERAL(INFO, "X0009" << m_peer);
      m_endpoint = Endpoint(std::move(address), m_peer.GetListenPortHost());
    }

    m_timer.cancel(ec);

    //LOG_GENERAL(DEBUG, "Connecting to " << m_peer);

//    LOG_GENERAL(INFO, "X0010" << m_peer);
    m_socket.async_connect(m_endpoint,
                           [self = shared_from_this()](const ErrorCode& ec) {
                           //LOG_GENERAL(INFO, "X0011" << self->m_peer << " ec " << ec);
                           if (ec != OPERATION_ABORTED) {
                               self->OnConnected(ec);
                             }
                           });
  }

  void OnConnected(const ErrorCode& ec) {
    //LOG_GENERAL(INFO, "X0012" << this->m_peer << " ec " << ec);
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

    //LOG_GENERAL(DEBUG, "Sending " << msg.size << " bytes to " << m_peer);

    boost::asio::async_write(
        m_socket, boost::asio::const_buffer(msg.data.get(), msg.size),
        [self = shared_from_this()](const ErrorCode& ec, size_t) {
        //LOG_GENERAL(INFO, "X0013" << self->m_peer << " ec " << ec);
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
        //LOG_GENERAL(INFO,
        //            "Peer " << m_peer << " is strictly blacklisted, Q=" << sz);
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
    //LOG_GENERAL(INFO, "X0014" << this->m_peer << " ec " << ec);
    if (m_closed) {
      return;
    }

    if (ec) {
      ScheduleReconnectOrGiveUp(ec);
      return;
    }

    if (m_queue.empty()) {
      // impossible
      zil::local::variables.AddSendMessageToPeerFailed(1);
      LOG_GENERAL(WARNING, "Unexpected queue state, peer="
                               << m_peer.GetPrintableIPAddress() << ":"
                               << m_peer.GetListenPortHost());
      Done();
      return;
    }

    m_queue.pop_front();

    //LOG_GENERAL(INFO, "X0015" << this->m_peer << " ec " << ec);
    Reconnect();
  }

  bool ExpiredOrDone(const ErrorCode& ec = ErrorCode{}) {
    //LOG_GENERAL(INFO, "X0016" << this->m_peer << " ec " << ec << " q " << m_queue.size());
    if (m_queue.empty()) {
      Done();
      return true;
    }

    if (m_queue.front().expires_at < Clock()) {
      //LOG_GENERAL(INFO, "X0017 " << this->m_peer);
      Done(ec ? ec : TIMED_OUT);
      return true;
    }
    //LOG_GENERAL(INFO, "X0018" << this->m_peer);
    return false;
  }

  void ScheduleReconnectOrGiveUp(const ErrorCode& ec) {
    if (ExpiredOrDone(ec)) {
      return;
    }

    assert(ec);
    //LOG_GENERAL(INFO, "X0019" << this->m_peer);
    WaitTimer(m_timer, Milliseconds(1000), this, &PeerSendQueue::Reconnect);
  }

  void Reconnect() {
    //LOG_GENERAL(INFO, "X0020 " << this->m_peer);
    if (!CheckAgainstBlacklist() || ExpiredOrDone()) {
      return;
    }

    //LOG_GENERAL(INFO, "X0021 " << this->m_peer);
    // TODO the current protocol is weird and it assumes reconnecting every
    // time. This should be changed!!!
    CloseGracefully(std::move(m_socket));
    m_socket = Socket(m_asioContext);
    Connect();
  }

  void Done(const ErrorCode& ec = ErrorCode{}) {
    //LOG_GENERAL(INFO, "X0022 " << this->m_peer << " ec " << ec);
    if (!m_closed) {
      m_doneCallback(m_peer, ec);
    }
  }

  AsioContext& m_asioContext;
  DoneCallback m_doneCallback;

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
    zil::local::variables.AddSendMessageToPeerCount(1);
    if (peer.m_listenPortHost == 0) {
      LOG_GENERAL(WARNING, "Ignoring message to peer " << peer);
      zil::local::variables.AddSendMessageToPeerFailed(1);
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
    zil::local::variables.AddSendMessageToPeerSyncCount(1);

    AsioContext localCtx(1);

    auto doneCallback = [&localCtx](const Peer& peer, ErrorCode ec) {
      auto peerStr = peer.GetPrintableIPAddress();
      if (ec) {
        zil::local::variables.AddSendMessageToPeerFailed(1);
        LOG_GENERAL(WARNING, "Send message to "
                                 << peerStr
                                 << " failed with error: " << ec.message());
      } else {
        LOG_GENERAL(DEBUG, "Send message to " << peerStr << " done");
      }
      localCtx.stop();
    };

    auto peerCtx = std::make_shared<PeerSendQueue>(localCtx, doneCallback,
                                                   std::move(peer));
    peerCtx->Enqueue(CreateMessage(message, {}, start_byte, false), false);

    localCtx.run();

    peerCtx->Close();
  }

  void OnNewJob(Peer&& peer, RawMessage&& msg, bool allow_relaxed_blacklist) {
    if (IsBlacklisted(peer, allow_relaxed_blacklist)) {
      //LOG_GENERAL(INFO,
      //            "Ignoring blacklisted peer " <<
      //            peer.GetPrintableIPAddress()
      //            << "allow relaxed blacklist " << allow_relaxed_blacklist);
      return;
    }

    //LOG_GENERAL(INFO, "X0100: Send to " << peer << " for " << msg.size);
    auto& ctx = m_activePeers[peer];
    if (!ctx) {
      ctx = std::make_shared<PeerSendQueue>(m_asioCtx, m_doneCallback,
                                            std::move(peer));
    }
    zil::local::variables.SetActivePeersSize(m_activePeers.size());
    ctx->Enqueue(std::move(msg), allow_relaxed_blacklist);
    //LOG_GENERAL(INFO, "X0101: done Send to " << peer << " for " << msg.size);
  }

  void OnPeerQueueFinished(const Peer& peer, ErrorCode ec) {

    if (ec) {
//      LOG_GENERAL(
//          INFO, "Peer queue finished, peer=" << peer.GetPrintableIPAddress()
//                                             << ":" << peer.GetListenPortHost()
//                                             << " ec=" << ec.message());
    } else {
      //LOG_GENERAL(INFO, "X0102: OnPeerQueueFinished with ec = 0");
    }

    auto it = m_activePeers.find(peer);
    if (it == m_activePeers.end()) {
      // impossible
      zil::local::variables.AddSendMessageToPeerFailed(1);
      //LOG_GENERAL(INFO, "X0103: SendMessageToPeer failed");
      return;
    }

    if (IsHostHavingNetworkIssue(ec)) {
      zil::local::variables.AddSendMessageToPeerFailed(1);
      if (Blacklist::GetInstance().IsWhitelistedSeed(peer.m_ipAddress)) {
        //LOG_GENERAL(WARNING, "[blacklist] Encountered "
        //                         << ec.value() << " (" << ec.message()
        //                         << "). Adding seed "
        //                         << peer.GetPrintableIPAddress()
        //                         << " as relaxed blacklisted");
        // Add this seed node to relaxed blacklist even if it is whitelisted
        // in general.
        Blacklist::GetInstance().Add(peer.m_ipAddress, false, true);
      } else {
        //LOG_GENERAL(WARNING, "[blacklist] Encountered "
        //                         << ec.value() << " (" << ec.message()
        //                         << "). Adding " << peer.GetPrintableIPAddress()
        //                         << " as strictly blacklisted");
        Blacklist::GetInstance().Add(peer.m_ipAddress);  // strict
      }
    } else if (IsNodeNotRunning(ec)) {
      //LOG_GENERAL(WARNING, "[blacklist] Encountered "
      //                         << ec.value() << " (" << ec.message()
      //                         << "). Adding " << peer.GetPrintableIPAddress()
      //                         << " as relaxed blacklisted");
      Blacklist::GetInstance().Add(peer.m_ipAddress, false);
    }

    // explicit Close() because shared_ptr may be reused in async operation
    it->second->Close();
    m_activePeers.erase(it);
    //LOG_GENERAL(INFO, "X0104: done OnPeerQueueFinished()");
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

std::shared_ptr<SendJobs> SendJobs::Create() {
  return std::make_shared<SendJobsImpl>();
}

}  // namespace zil::p2p
