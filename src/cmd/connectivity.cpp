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

#include <random>

#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "libCrypto/Sha2.h"
#include "libNetwork/P2P.h"
#include "libUtils/Logger.h"

using namespace zil::p2p;

namespace {

using SteadyTimer = boost::asio::steady_timer;
using ErrorCode = boost::system::error_code;

using Time = std::chrono::milliseconds;

// Initial delay to wait all other nodes are probably up
static constexpr Time INITIAL_DELAY_TIME(15000);

// Average delay between payload sending actions
static constexpr size_t AVERAGE_DELAY_TIME(3333);
static constexpr size_t DELAY_RANGE(2800);
static_assert(AVERAGE_DELAY_TIME - DELAY_RANGE / 2 > 0);

// Random payloads parametrized
static constexpr size_t MIN_PAYLOAD_LENGTH = 3;
static constexpr size_t MAX_PAYLOAD_LENGTH = 2023;

// Check expirations rough period
static constexpr Time CHECK_EXPIRATIONS_TIME(5000);

// Check new peers rough period
static constexpr Time CHECK_PEERS_TIME(15000);

// Will warn if no ack during this time
static constexpr Time DELAY_TIME(1000);

// Will warn and delete the expected hash from the container after this period
static constexpr Time FULL_EXPIRATION_TIME(300000);

static constexpr uint16_t LISTEN_PORT = 40000;

// Message types
static constexpr uint8_t MSG_PAYLOAD = 1;
static constexpr uint8_t MSG_ACK = 2;
static constexpr uint8_t MSG_PEERS_REQUEST = 3;
static constexpr uint8_t MSG_PEERS_RESPONSE = 4;

zbytes HashPayload(const zbytes& payload) {
  if (payload.size() < MIN_PAYLOAD_LENGTH) {
    return {};
  }
  SHA256Calculator hasher;
  hasher.Update(payload.data() + 1, payload.size() - 1);
  auto hash = hasher.Finalize();
  hash[0] = MSG_ACK;
  return hash;
}

inline Time Clock() {
  return std::chrono::duration_cast<Time>(
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

class RNG {
 public:
  RNG() : m_generator(m_rd()) {}

  zbytes GenRandomPayload() {
    std::uniform_int_distribution<size_t> distr(MIN_PAYLOAD_LENGTH,
                                                MAX_PAYLOAD_LENGTH);
    size_t sz = distr(m_generator);
    zbytes payload;
    payload.resize(sz);
    std::uniform_int_distribution<uint8_t> bytes_distr;
    std::generate(payload.begin(), payload.end(),
                  [this, &bytes_distr]() { return bytes_distr(m_generator); });
    payload[0] = MSG_PAYLOAD;
    return payload;
  }

  Time GenRandomDelay() {
    std::uniform_int_distribution<size_t> distr(
        AVERAGE_DELAY_TIME - DELAY_RANGE, AVERAGE_DELAY_TIME + DELAY_RANGE);
    return Time(distr(m_generator));
  }

  size_t GenRandomIndex(size_t sizeOfContainer) {
    if (sizeOfContainer <= 1) {
      return 0;
    }
    std::uniform_int_distribution<size_t> distr;
    return distr(m_generator) % sizeOfContainer;
  }

 private:
  std::random_device m_rd;
  std::default_random_engine m_generator;
};

class Connectivity {
  struct PeerStatus {
    size_t empty_msgs = 0;
    size_t invalid_msg_types = 0;
    size_t success = 0;
    size_t delayed = 0;
    size_t expired = 0;
  };

  struct PayloadStatus {
    Peer sent_to;
    Time expire_time{};
    bool delayed = false;
  };

  boost::asio::io_context m_asio{1};
  P2P m_p2p;
  Peer m_lookup;
  std::map<Peer, PeerStatus> m_peers;
  std::map<zbytes, PayloadStatus> m_messagesSent;
  Time m_nextExpirationCheck{};
  Time m_nextPeersCheck{};
  SteadyTimer m_timer;
  RNG m_randomGen;

  PeerStatus& GetPeer(Peer& from) {
    from.m_listenPortHost = LISTEN_PORT;
    auto it = m_peers.find(from);
    if (it == m_peers.end()) {
      LOG_GENERAL(INFO, "New peer " << from);
      return m_peers[from];
    }
    return it->second;
  }

  void Complain(const Peer& peer, const PeerStatus& st,
                const std::string_view what) {
    std::stringstream sstream;
#define ADDF(field)                            \
  if (st.field != 0) {                         \
    sstream << ", " #field << "=" << st.field; \
  }
    ADDF(empty_msgs)
    ADDF(invalid_msg_types)
    ADDF(success)
    ADDF(delayed)
    ADDF(expired)
#undef ADDF
    LOG_GENERAL(WARNING, what << ", peer=" << peer << sstream.str());
  }

  void Send(const Peer& peer, const zbytes& msg) {
    assert(peer.m_listenPortHost == LISTEN_PORT);
    m_p2p.SendMessage(peer, msg, START_BYTE_NORMAL, false);
  }

  void SendPeersRequest() {
    // N.B. sizes <= 2 can be rejected due to Zil p2p protocol details
    static const zbytes request = {MSG_PEERS_REQUEST, 0, 0};
    m_p2p.SendMessage(m_lookup, request, START_BYTE_NORMAL, false);
  }

  void OnPayload(const Peer& peer, const zbytes& payload) {
    auto hash = HashPayload(payload);
    assert(hash.at(0) == MSG_ACK);
    Send(peer, hash);
  }

  void OnAck(const Peer& peer, PeerStatus& st, const zbytes& msg) {
    if (msg.size() != 32) {
      LOG_GENERAL(WARNING,
                  "Unexpected ack of size=" << msg.size() << " from " << peer);
      return;
    }
    auto it = m_messagesSent.find(msg);
    if (it == m_messagesSent.end()) {
      LOG_GENERAL(WARNING, "Unexpected ack from " << peer);
      return;
    }
    auto& status = it->second;
    if (status.sent_to != peer) {
      LOG_GENERAL(WARNING, "Unexpected peer " << peer);
    } else {
      ++st.success;
    }
    m_messagesSent.erase(it);
  }

  void OnPeerListRequest(const Peer& peer) {
    zbytes response;
    response.reserve(1 + UINT128_SIZE * (m_peers.size() - 1));
    response.push_back(MSG_PEERS_RESPONSE);
    size_t offset = 1;
    for (const auto& [p, _] : m_peers) {
      static constexpr uint128_t localhost(0x7f000001);
      if (p.m_ipAddress != localhost && p.m_ipAddress != peer.m_ipAddress) {
        Serializable::SetNumber(response, offset, p.m_ipAddress, UINT128_SIZE);
        offset += UINT128_SIZE;
      }
    }
    Send(peer, response);
  }

  void OnPeerListResponse(const zbytes& msg) {
    if (msg.size() < UINT128_SIZE + 1) {
      return;
    }
    size_t oldSize = m_peers.size();
    size_t offset = 1;
    size_t end = msg.size() - UINT128_SIZE;
    while (offset <= end) {
      auto number =
          Serializable::GetNumber<uint128_t>(msg, offset, UINT128_SIZE);
      Peer peer(number, LISTEN_PORT);
      std::ignore = GetPeer(peer);
      offset += UINT128_SIZE;
    }
    if (oldSize != m_peers.size()) {
      LOG_GENERAL(INFO, "Peers added: " << oldSize << " -> " << m_peers.size());
    }
  }

  void Dispatch(std::shared_ptr<Message>&& msg) {
    auto &st = GetPeer(msg->from);
    if (msg->msg.empty()) {
      ++st.empty_msgs;
      Complain(msg->from, st, "Empty message");
      return;
    }
    switch (msg->msg[0]) {
      case MSG_PAYLOAD:
        OnPayload(msg->from, msg->msg);
        break;
      case MSG_ACK:
        OnAck(msg->from, st, msg->msg);
        break;
      case MSG_PEERS_REQUEST:
        OnPeerListRequest(msg->from);
        break;
      case MSG_PEERS_RESPONSE:
        OnPeerListResponse(msg->msg);
        break;
      default:
        ++st.invalid_msg_types;
        Complain(msg->from, st, "Invalid message type");
        break;
    }
  }

  void CheckExpirations(Time now) {
    for (auto it = m_messagesSent.begin(); it != m_messagesSent.end();) {
      auto& st = it->second;
      bool erase = false;

      if (st.expire_time < now) {
        auto& peer_st = GetPeer(st.sent_to);
        if (!st.delayed) {
          st.delayed = true;
          st.expire_time = now + FULL_EXPIRATION_TIME;
          ++peer_st.delayed;
          Complain(st.sent_to, peer_st, "Roundtrip delayed");
        } else {
          erase = true;
          --peer_st.delayed;
          ++peer_st.expired;
          Complain(st.sent_to, peer_st, "Roundtrip expired");
        }
      }

      if (erase) {
        it = m_messagesSent.erase(it);
      } else {
        ++it;
      }
    }
  }

  void SendRandomPayload(Time now) {
    if (!m_peers.empty()) {
      auto it = m_peers.begin();
      auto idx = m_randomGen.GenRandomIndex(m_peers.size());
      for (size_t i = 0; i < idx; ++i) {
        ++it;
        assert(it != m_peers.end());
      }
      zbytes payload = m_randomGen.GenRandomPayload();
      assert(payload.at(0) == MSG_PAYLOAD &&
             payload.size() >= MIN_PAYLOAD_LENGTH);
      Send(it->first, payload);
      auto& state = m_messagesSent[HashPayload(payload)];
      state.sent_to = it->first;
      state.expire_time = now + DELAY_TIME;
    }
  }

  void OnTimer() {
    LOG_GENERAL(INFO, ".");

    auto now = Clock();
    if (now >= m_nextExpirationCheck) {
      CheckExpirations(now);
      m_nextExpirationCheck = now + CHECK_EXPIRATIONS_TIME;
    }

    if (now >= m_nextPeersCheck) {
      SendPeersRequest();
      m_nextPeersCheck = Clock() + CHECK_PEERS_TIME;
    }

    SendRandomPayload(now);

    WaitTimer(m_timer, m_randomGen.GenRandomDelay(), this,
              &Connectivity::OnTimer);
  }

 public:
  Connectivity() : m_timer(m_asio) {}

  void Run(Peer&& lookup) {
    LOG_GENERAL(INFO, "Lookup: " << lookup);
    m_lookup = std::move(lookup);
    std::ignore = GetPeer(m_lookup);

    boost::asio::signal_set sig(m_asio, SIGINT, SIGTERM);
    sig.async_wait([&](const ErrorCode&, int) { m_asio.stop(); });

    auto dispatcher = [this](std::shared_ptr<Message> message) {
      Dispatch(std::move(message));
    };
    m_p2p.StartServer(m_asio, LISTEN_PORT, 0, std::move(dispatcher));

    m_nextExpirationCheck =
        Clock() + Time(CHECK_EXPIRATIONS_TIME + INITIAL_DELAY_TIME);

    WaitTimer(m_timer, INITIAL_DELAY_TIME, this, &Connectivity::OnTimer);

    m_asio.run();
  }
};

std::optional<Peer> ExtractLookup() {
  using boost::property_tree::ptree;

  try {
    ptree pt;
    read_xml("constants.xml", pt);
    for (const ptree::value_type& v : pt.get_child("node.lookups")) {
      if (v.first == "peer") {
        struct in_addr ip_addr {};
        inet_pton(AF_INET, v.second.get<std::string>("ip").c_str(), &ip_addr);
        if (ip_addr.s_addr == 0) {
          throw std::runtime_error("Zero lookup ip");
        }
        return Peer(uint128_t(ip_addr.s_addr), LISTEN_PORT);
      }
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Cannot read lookup ip from constants.xml: " << e.what());
  }
  return std::nullopt;
}

}  // namespace

int main(int argc, char* argv[]) {
#ifdef __linux__
  if (argc == 2 && std::string("-d") == argv[1]) {
    pid_t pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    }
    if (pid > 0) {
      exit(EXIT_SUCCESS);
    }
    if (setsid() < 0) {
      exit(EXIT_FAILURE);
    }
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
  }
#endif

  INIT_FILE_LOGGER("connectivity", std::filesystem::current_path());
  LOG_DISPLAY_LEVEL_ABOVE(INFO);

  auto lookup = ExtractLookup();
  if (!lookup) {
    return 1;
  }

  Connectivity conn;

  try {
    LOG_GENERAL(INFO, "Starting server");
    conn.Run(std::move(*lookup));
    LOG_GENERAL(INFO, "Done");
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception: " << e.what());
    return 2;
  }

  return 0;
}
