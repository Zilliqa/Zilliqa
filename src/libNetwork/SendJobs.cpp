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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "SendJobs.h"

#include "Blacklist.h"
#include "Peer.h"
#include "common/MessageNames.h"
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
  std::atomic<int> reconnectionToPeerCount = 0;

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

  void SetActivePeersSize(int amount) {
    Init();
    activePeersSize = amount;
  }

  void AddReconntionToPeerCount(int count) {
    Init();
    reconnectionToPeerCount += count;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "sendjobs.gauge",
                                          "Send Jobs metrics", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(sendMessageToPeerCount.load(),
                   {{"counter", "SendMessageToPeerCount"}});
        result.Set(sendMessageToPeerFailed.load(),
                   {{"counter", "SendMessageToPeerFailed"}});
        result.Set(sendMessageToPeerSyncCount.load(),
                   {{"counter", "SendMessageToPeerSyncCount"}});
        result.Set(activePeersSize.load(), {{"counter", "ActivePeersSize"}});
        result.Set(reconnectionToPeerCount.load(),
                   {{"counter", "ReconnectionToPeerCount"}});
      });
    }
  }
};

static SendJobsVariables variables{};

}  // namespace local
}  // namespace zil
namespace zil::p2p {

using AsioContext = boost::asio::io_context;
using Tcp = boost::asio::ip::tcp;
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
  return Blacklist::GetInstance().Exist(
      {peer.GetIpAddress(), peer.GetListenPortHost(),
       peer.GetNodeIndentifier()},
      !allow_relaxed_blacklist);
}

inline Milliseconds Clock() {
  return std::chrono::duration_cast<Milliseconds>(
      boost::asio::steady_timer::clock_type::now().time_since_epoch());
}

/// Waits for timer and calls a member function of Object,
/// Timer must be in the scope of object (for pointers validity)
template <typename F, typename Time>
void WaitTimer(SteadyTimer& timer, Time delay, F onTimerHandler) {
  ErrorCode ec;
  timer.expires_at(
      boost::asio::steady_timer::clock_type::time_point(Clock() + delay), ec);

  if (ec) {
    LOG_GENERAL(FATAL,
                "Cannot set timer: " << ec.message() << " (" << ec << ')');
    return;
  }

  timer.async_wait(
      [timerHandler = std::move(onTimerHandler)](const ErrorCode& error) {
        if (!error) {
          timerHandler();
        }
      });
}

std::set<Peer> ExtractMultipliers() {
  using boost::property_tree::ptree;

  std::set<Peer> peers;

  try {
    ptree pt;
    read_xml("constants.xml", pt);
    for (const ptree::value_type& v : pt.get_child("node.multipliers")) {
      if (v.first == "peer") {
        struct in_addr ip_addr {};
        inet_pton(AF_INET, v.second.get<std::string>("ip").c_str(), &ip_addr);
        if (ip_addr.s_addr == 0) {
          LOG_GENERAL(WARNING, "Ignoring zero multiplier IP");
          continue;
        }
        auto port = v.second.get<uint32_t>("port");
        if (port == 0) {
          LOG_GENERAL(WARNING, "Ignoring zero multiplier port");
          continue;
        }
        auto inserted = peers.emplace(uint128_t(ip_addr.s_addr), port);
        if (inserted.second) {
          LOG_GENERAL(INFO, "Found multiplier at " << *inserted.first);
        }
      }
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Cannot read multipliers from constants.xml: " << e.what());
  }

  return peers;
}

/// Returns a dummy buffer to read into (we really need to use reads in this
/// part of the protocol just to detect EOFs)
inline auto& GetDummyBuffer() {
  static std::array<uint8_t, 2048> dummyArray;
  static auto buf =
      boost::asio::mutable_buffer(dummyArray.data(), dummyArray.size());
  return buf;
}

/// Closes socket gracefully, waits for EOF first. Helps to avoid undesirable
/// TCP states on both sides
class GracefulCloseImpl
    : public std::enable_shared_from_this<GracefulCloseImpl> {
  Socket m_socket;

 public:
  GracefulCloseImpl(Socket&& socket) : m_socket(std::move(socket)) {}

  void Close() {
    m_socket.async_read_some(
        GetDummyBuffer(),
        [self = shared_from_this()](const ErrorCode& ec, size_t n) {
          if (ec != END_OF_FILE) {
            LOG_GENERAL(DEBUG,
                        "Expected EOF, got ec=" << ec.message() << " n=" << n);
          }
          ErrorCode ignored;
          self->m_socket.close(ignored);
        });
  }
};

void CloseGracefully(Socket&& socket) {
  ErrorCode ec;
  if (!socket.is_open()) {
    return;
  }
  socket.shutdown(boost::asio::socket_base::shutdown_both, ec);
  if (ec) {
    socket.close(ec);
    return;
  }
  size_t unread = socket.available(ec);
  if (ec) {
    socket.close(ec);
    return;
  }
  if (unread > 0) {
    do {
      constexpr size_t BUFF_SIZE = 4096;
      boost::container::small_vector<uint8_t, BUFF_SIZE> buf;
      buf.resize(std::min(BUFF_SIZE, unread));
      socket.read_some(boost::asio::mutable_buffer(buf.data(), unread), ec);
      LOG_GENERAL(INFO, "Draining remaining IO before close");
    } while (!ec && (unread = socket.available(ec)) > 0);
  }
  socket.close(ec);
}

constinit std::chrono::seconds IDLE_TIMEOUT_IP_ONLY(120);
// We don't want to resolve dns name very often, give more idle time for this
// type of connections
constinit std::chrono::seconds IDLE_TIMEOUT_DNS(600);
constinit std::chrono::milliseconds SLOW_SEND_TO_REPORT(5000);

}  // namespace

class PeerSendQueue : public std::enable_shared_from_this<PeerSendQueue> {
 public:
  struct Item {
    RawMessage msg;
    bool allow_relaxed_blacklist;
    Milliseconds expires_at;
  };

  using DoneCallback = std::function<void(const Peer& peer)>;

  PeerSendQueue(AsioContext& ctx, const DoneCallback& done_cb, Peer peer,
                bool is_multiplier, bool no_wait)
      : m_asioContext(ctx),
        m_doneCallback(done_cb),
        m_peer(std::move(peer)),
        m_socket(m_asioContext),
        m_resolver(m_asioContext),
        m_timer(m_asioContext),
        m_messageExpireTime(std::max(15000u, TX_DISTRIBUTE_TIME_IN_MS * 5 / 6)),
        m_isMultiplier(is_multiplier),
        m_noWait(no_wait) {}

  ~PeerSendQueue() { Close(); }

  void Enqueue(RawMessage msg, bool allow_relaxed_blacklist) {
    m_queue.emplace_back();
    auto& item = m_queue.back();
    item.msg = std::move(msg);
    item.allow_relaxed_blacklist = allow_relaxed_blacklist;
    item.expires_at = Clock() + m_messageExpireTime;
    if (m_queue.size() == 1) {
      if (!m_connected) {
        if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
          LOG_GENERAL(INFO, "Not connected, so trying to connect to: "
                                << m_peer.GetPrintableIPAddress() << ", "
                                << m_peer.GetHostname());
        }
        Resolve();
      } else {
        if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
          LOG_GENERAL(INFO, "Queue size > 1, so sending message to: "
                                << m_peer.GetPrintableIPAddress() << ", "
                                << m_peer.GetHostname());
        }
        SendMessage();
      }
    }
    m_inIdleTimeout = false;
  }

  void Close() {
    if (!m_closed) {
      m_closed = true;
      CloseGracefully(std::move(m_socket));
    }
  }

 private:
  void Resolve() {
    if (!std::empty(m_peer.GetHostname()) && !m_is_resolving) {
      m_is_resolving = true;
      m_resolver.async_resolve(
          m_peer.GetHostname(), std::to_string(m_peer.GetListenPortHost()),
          [self = shared_from_this()](
              const ErrorCode& ec,
              const Tcp::resolver::results_type& endpoints) {
            if (!ec) {
              LOG_GENERAL(INFO, "Successfully resolved dns name: "
                                    << self->m_peer.GetHostname());
              self->OnResolved(endpoints);
            } else {
              LOG_GENERAL(WARNING, "Unable to resolve dns name: "
                                       << self->m_peer.GetHostname());
              self->Done();
            }
          });

    } else {
      Connect();
    }
  }

  void OnResolved(const Tcp::resolver::results_type& endpoints) {
    ErrorCode ignored;
    m_timer.cancel(ignored);

    WaitTimer(m_timer, Milliseconds{CONNECTION_TIMEOUT_IN_MS},
              [self = shared_from_this()]() {
                if (!self->m_connected) {
                  LOG_GENERAL(WARNING,
                              "Unable to connect within "
                                  << CONNECTION_TIMEOUT_IN_MS
                                  << ", canceling any operation on the socket "
                                     "with remote dns/ip: "
                                  << self->m_peer.GetHostname() << "/"
                                  << self->m_peer.GetPrintableIPAddress());
                  self->m_socket.cancel();
                  self->OnConnected(TIMED_OUT);
                }
              });

    LOG_GENERAL(INFO,
                "I'll try to connect to remote IP since the dns name was "
                "successfully resolved, peer: "
                    << m_peer.GetHostname());
    boost::asio::async_connect(
        m_socket, endpoints,
        [self = shared_from_this()](const ErrorCode& ec,
                                    const Endpoint& endpoint) {
          LOG_GENERAL(INFO,
                      "Async connect handler called with ec: " << ec.message());
          if (ec != OPERATION_ABORTED) {
            self->m_endpoint = endpoint;
            LOG_GENERAL(INFO, "Connection (via async resolve) to "
                                  << self->m_endpoint << ": " << ec.message()
                                  << " (" << ec << ')'
                                  << ", queue size: " << self->m_queue.size());

            self->m_timer.cancel();
            self->OnConnected(ec);
          }
        });
  }

  void Connect() {
    ErrorCode ec;

    if (m_endpoint.port() == 0) {
      auto address =
          boost::asio::ip::make_address(m_peer.GetPrintableIPAddress(), ec);
      if (ec) {
        LOG_GENERAL(INFO, "Cannot create endpoint for address "
                              << m_peer.GetPrintableIPAddress() << ":"
                              << m_peer.GetListenPortHost());
        Done();
        return;
      }
      m_endpoint = Endpoint(std::move(address), m_peer.GetListenPortHost());
    }

    m_timer.cancel(ec);

    WaitTimer(m_timer, Milliseconds{CONNECTION_TIMEOUT_IN_MS},
              [self = shared_from_this()]() {
                if (!self->m_connected) {
                  LOG_GENERAL(
                      WARNING,
                      "Unable to connect within "
                          << CONNECTION_TIMEOUT_IN_MS
                          << ", canceling any operation on the socket to: "
                          << self->m_peer.GetPrintableIPAddress() << ", "
                          << self->m_peer.GetHostname());
                  self->m_socket.cancel();
                  self->OnConnected(TIMED_OUT);
                }
              });

    m_socket.async_connect(
        m_endpoint, [self = shared_from_this()](const ErrorCode& ec) {
          LOG_GENERAL(DEBUG, "Connection (via direct ip) to "
                                 << self->m_endpoint << ": " << ec.message()
                                 << " (" << ec << ')');
          if (ec != OPERATION_ABORTED) {
            self->m_timer.cancel();
            self->OnConnected(ec);
          }
        });
  }

  void OnConnected(const ErrorCode& ec) {
    if (m_closed) {
      LOG_GENERAL(INFO,
                  "It looks the OnConnected was called when socket was already "
                  "closed, returning");
      return;
    }
    if (!ec) {
      LOG_GENERAL(INFO,
                  "There's no error, so sending the message from queue to: "
                      << m_peer.GetPrintableIPAddress() << ", "
                      << m_peer.GetHostname());
      m_connected = true;
      m_is_resolving = false;
      SendMessage();
    } else {
      LOG_GENERAL(INFO, "There was an error: " << ec.message()
                                               << ", so I'll try to reconnect");
      m_connected = false;
      ScheduleReconnectOrGiveUp();
    }
  }

  bool FindNotExpiredMessage() {
    auto clock = Clock();
    while (!m_queue.empty()) {
      // Messages sent to entities having dns name don't expire
      if (std::empty(m_peer.GetHostname()) &&
          m_queue.front().expires_at < clock) {
        LOG_GENERAL(INFO, "Dropping P2P message as expired, peer="
                              << m_peer << ", elapsed [ms]: "
                              << (clock - m_queue.front().expires_at).count());
        m_queue.pop_front();
        // TODO metric about message drops
      } else {
        return true;
      }
    }
    return false;
  }

  void OnIdleTimer() {
    if (m_inIdleTimeout && m_queue.empty()) {
      Done();
    }
  }

  void SendMessage() {
    if (!FindNotExpiredMessage()) {
      if (m_connected && !m_noWait && !m_isMultiplier) {
        m_inIdleTimeout = true;
        if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
          LOG_GENERAL(INFO,
                      "FindNotExpiredMessage is false, scheduling timer for: "
                          << m_peer.GetPrintableIPAddress() << ", "
                          << m_peer.GetHostname());
        }
        const auto delay = std::empty(m_peer.GetHostname())
                               ? IDLE_TIMEOUT_IP_ONLY
                               : IDLE_TIMEOUT_DNS;
        WaitTimer(m_timer, delay, [this]() { OnIdleTimer(); });
      } else {
        if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
          LOG_GENERAL(INFO,
                      "FindNotExpiredMessage is false, calling Done() for: "
                          << m_peer.GetPrintableIPAddress() << ", "
                          << m_peer.GetHostname());
        }
        Done();
      }
      if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
        LOG_GENERAL(
            INFO,
            "FindNotExpiredMessage is false, returning from this branch for: "
                << m_peer.GetPrintableIPAddress() << ", "
                << m_peer.GetHostname());
      }
      return;
    }

    assert(!m_queue.empty());

    auto& msg = m_queue.front().msg;

    if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
      LOG_GENERAL(INFO, "Calling async_write on a socket, for: "
                            << m_peer.GetPrintableIPAddress() << ", "
                            << m_peer.GetHostname());
    }

    boost::asio::async_write(
        m_socket, boost::asio::const_buffer(msg.data.get(), msg.size),
        [self = shared_from_this(),
         start_time = std::chrono::steady_clock::now()](const ErrorCode& ec,
                                                        size_t) {
          if (ec != OPERATION_ABORTED) {
            const auto now = std::chrono::steady_clock::now();

            if (now - start_time > SLOW_SEND_TO_REPORT) {
              LOG_GENERAL(
                  WARNING,
                  "Slow send, it took: "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(
                             now - start_time)
                             .count()
                      << "[ms] to deliver msg");
            }
            self->OnWritten(ec);
          }
        });
  }

  void OnWritten(const ErrorCode& ec) {
    if (m_closed) {
      if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
        LOG_GENERAL(INFO, "I am closed, returning, for: "
                              << m_peer.GetPrintableIPAddress() << ", "
                              << m_peer.GetHostname());
      }
      return;
    }

    if (ec) {
      if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
        LOG_GENERAL(INFO,
                    "I got error, will try to reconnect and send again, error: "
                        << ec.message()
                        << ", for: " << m_peer.GetPrintableIPAddress() << ", "
                        << m_peer.GetHostname());
      }

      m_connected = false;
      ScheduleReconnectOrGiveUp();
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

    if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
      LOG_GENERAL(INFO,
                  "Queue not empty, pop from and schedule SendMessage for: "
                      << m_peer.GetPrintableIPAddress() << ", "
                      << m_peer.GetHostname());
    }

    m_queue.pop_front();
    SendMessage();
  }

  void ScheduleReconnectOrGiveUp() {
    if (!FindNotExpiredMessage()) {
      Done();
      return;
    }

    WaitTimer(m_timer, Milliseconds{RECONNECT_INTERVAL_IN_MS},
              [this]() { Reconnect(); });
  }

  void Reconnect() {
    LOG_GENERAL(INFO, "Peer " << m_peer << " reconnects");
    CloseGracefully(std::move(m_socket));
    m_socket = Socket(m_asioContext);
    Resolve();
  }

  void Done() {
    if (!m_closed) {
      m_doneCallback(m_peer);
    }
  }

  AsioContext& m_asioContext;

  // cb to the owner
  DoneCallback m_doneCallback;

  // remote peer
  Peer m_peer;

  // peer's endpoint
  Endpoint m_endpoint;

  // message queue
  std::deque<Item> m_queue;

  // tcp socket
  Socket m_socket;

  // Resolved for asynchronous dns lookups
  Tcp::resolver m_resolver;

  // Timer is used
  SteadyTimer m_timer;

  // Every message has some expire time for delivery
  // TODO: make it explicit for various kinds of messages
  Milliseconds m_messageExpireTime;

  bool m_isMultiplier;

  // If true, then this instance will nolonger disturb the owner which may not
  // exist at the moment (shared_ptr may be live in some async operations)
  bool m_closed = false;

  // it's hard to determine is an asio socket really connected, so explicit var
  bool m_connected = false;

  // Checks wheter dns resolve call is taking place
  bool m_is_resolving = false;

  bool m_inIdleTimeout = false;

  bool m_noWait = false;
};

class SendJobsImpl : public SendJobs,
                     public std::enable_shared_from_this<SendJobsImpl> {
 public:
  SendJobsImpl()
      : m_doneCallback([this](const Peer& peer) { OnPeerQueueFinished(peer); }),
        m_multipliers(ExtractMultipliers()),
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

    LOG_GENERAL(DEBUG, "Enqueueing message, size=" << message.size
                                                   << " peer = " << peer);

    if (ARCHIVAL_LOOKUP || LOOKUP_NODE_MODE) {
      LOG_GENERAL(INFO,
                  "Enqueueing message for: " << peer.GetPrintableIPAddress()
                                             << ", " << peer.GetHostname());
    }

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

    auto doneCallback = [&localCtx](const Peer& peer) {
      auto peerStr = peer.GetPrintableIPAddress();
      LOG_GENERAL(DEBUG, "Done with " << peer);
      localCtx.stop();
    };

    auto peerCtx = std::make_shared<PeerSendQueue>(
        localCtx, doneCallback, std::move(peer), false, true);
    peerCtx->Enqueue(CreateMessage(message, {}, start_byte, false), false);

    localCtx.run();

    peerCtx->Close();
  }

  void OnNewJob(Peer&& peer, RawMessage&& msg, bool allow_relaxed_blacklist) {
    if (IsBlacklisted(peer, allow_relaxed_blacklist)) {
      LOG_GENERAL(INFO, "Ignoring blacklisted peer "
                            << peer.GetPrintableIPAddress()
                            << "allow relaxed blacklist "
                            << allow_relaxed_blacklist);
      return;
    }

    auto& ctx = m_activePeers[peer];
    if (!ctx) {
      bool is_multiplier = m_multipliers.contains(peer);
      ctx = std::make_shared<PeerSendQueue>(
          m_asioCtx, m_doneCallback, std::move(peer), is_multiplier, false);
    }
    zil::local::variables.SetActivePeersSize(m_activePeers.size());
    ctx->Enqueue(std::move(msg), allow_relaxed_blacklist);
  }

  void OnPeerQueueFinished(const Peer& peer) {
    auto it = m_activePeers.find(peer);
    if (it == m_activePeers.end()) {
      // impossible
      zil::local::variables.AddSendMessageToPeerFailed(1);
      return;
    }

    // explicit Close() because shared_ptr may be reused in async operation
    LOG_GENERAL(
        INFO, "Nothing else to be sent to peer, so closing socket with remote: "
                  << peer.GetPrintableIPAddress());
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
  std::set<Peer> m_multipliers;
  std::thread m_workerThread;
  std::map<Peer, std::shared_ptr<PeerSendQueue>> m_activePeers;
};

std::shared_ptr<SendJobs> SendJobs::Create() {
  return std::make_shared<SendJobsImpl>();
}

}  // namespace zil::p2p
