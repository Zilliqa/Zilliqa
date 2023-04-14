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

#include "PeerDiscovery.h"

#include <map>
#include <random>
#include <set>
#include <unordered_map>

#include <boost/asio/steady_timer.hpp>

#include "common/Messages.h"
#include "libNetwork/P2P.h"
#include "libUtils/Logger.h"

namespace zil::p2p {

// TODO move asio stuff below to libUtils/AsioUtils.h
using SteadyTimer = boost::asio::steady_timer;
using ErrorCode = boost::system::error_code;

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

void RoleAndIndexFromString(const std::string& str, Role& role,
                            uint32_t& index) {
  role = Role::INDEFINITE;
  index = 0;
  auto pos = str.find('-');
  if (pos < str.size()) {
    index = std::atoi(str.data() + pos + 1);
    std::string_view role_str(str.data(), pos);
    if (role_str == "normal") {
      role = Role::NORMAL;
    } else if (role_str == "dsguard") {
      role = Role::DSGUARD;
    } else if (role_str == "lookup") {
      role = Role::LOOKUP;
    } else if (role_str == "seedpub") {
      role = Role::SEEDPUB;
    }
  }
}

std::string RoleAndIndexToString(Role role, uint32_t index) {
  std::string str;
  switch (role) {
    case Role::NORMAL:
      str = "normal";
      break;
    case Role::DSGUARD:
      str = "dsguard";
      break;
    case Role::LOOKUP:
      str = "lookup";
      break;
    case Role::SEEDPUB:
      str = "seedpub";
      break;
    default:
      str = "peer";
      break;
  }
  str += '-';
  str += std::to_string(index);
  return str;
}

class PeerDiscoveryImpl
    : public PeerDiscovery,
      public std::enable_shared_from_this<PeerDiscoveryImpl> {
 public:
  PeerDiscoveryImpl(boost::asio::io_context& asio, P2P& p2p, Options&& options)
      : m_asio(asio),
        m_p2p(p2p),
        m_options(std::move(options)),
        m_timer(m_asio) {
    m_helloMessage = CreateHelloMessage();
  }

  // separate from the ctor. because of weak_from_this
  void Run() {
    m_asio.post([wptr = weak_from_this()] {
      auto self = wptr.lock();
      if (self) {
        self->OnTimer();
      }
    });
  }

 private:
  class CompareByIndex {
   public:
    bool operator()(const PeerInfoPtr& a, const PeerInfoPtr& b) const {
      assert(a && b);
      return a->index < b->index;
    }
  };

  using PeerSet = std::multiset<PeerInfoPtr, CompareByIndex>;

  PeerInfoPtr GetByPubkey(const PubKey& pubKey) const override {
    auto it = m_peersByPubkey.find(pubKey);
    if (it != m_peersByPubkey.end()) {
      return it->second;
    }
    return {};
  }

  std::vector<PeerInfoPtr> GetByRole(Role role) const override {
    std::vector<PeerInfoPtr> result;
    auto i = static_cast<size_t>(role);
    if (i < m_peersByRoles.size()) {
      const auto& set = m_peersByRoles[i];
      result.reserve(set.size());
      std::copy(set.begin(), set.end(), std::back_inserter(result));
    }
    return result;
  }

  bool Execute(const zbytes& message, unsigned int offset, const Peer& from,
               const unsigned char& startByte) override {
    auto instruction = message[offset];
    switch (instruction) {
      case HELLO:
        return OnHello(message, offset + 1, from);
      case SNAPSHOT:
        return OnSnapshot(message, offset + 1, from);
      default:
        break;
    }

    // TODO LOG

    return false;
  }

  bool OnHello(const zbytes& message, unsigned offset, const Peer& from) {
    PeerInfo pi;
    pi.peer = from;
    uint64_t snapshotRequestTime = 0;
    if (!ReadHelloMessage(message, offset, pi, snapshotRequestTime)) {
      // TODO LOG
      // Blacklist
      return false;
    }
    auto replyTo = pi.peer;
    AddPeer(std::move(pi));
    auto reply = TakeSnapshot(snapshotRequestTime);
    m_p2p.SendMessage(replyTo, reply);
    return true;
  }

  bool OnSnapshot(const zbytes& message, unsigned offset, const Peer& from) {
    PeerInfo sender;
    sender.peer = from;
    if (!ReadHelloMessageBody(message, offset, sender)) {
      // TODO LOG
      // Blacklist
      return false;
    }

    while (offset < message.size()) {
      PeerInfo pi;
      if (!ReadPeerInfo(message, offset, pi)) {
        // TODO LOG
        // Blacklist
        return false;
      }
      AddPeer(std::move(pi));
    }

    AddPeer(std::move(sender));
    return true;
  }

  void AddPeer(PeerInfo&& p) {
    auto it = m_peersByPubkey.find(p.pubKey);
    if (it != m_peersByPubkey.end()) {
      auto& previous = it->second;
      if (previous->peer == p.peer && previous->role == p.role &&
          previous->index == p.index) {
        // we know this peer
        return;
      }
      m_peersByRoles[static_cast<size_t>(previous->role)].erase(previous);
      m_peersByPubkey.erase(it);
    }
    Insert(std::move(p));
  }

  void Insert(PeerInfo&& p) {
    auto ptr = std::make_shared<const PeerInfo>(std::move(p));
    m_peersByPubkey[p.pubKey] = ptr;

    auto i = static_cast<size_t>(ptr->role);
    assert(i < m_peersByRoles.size());
    m_peersByRoles[i].insert(ptr);

    m_peersByTime.insert(std::make_pair(Clock(), Serialize(*ptr)));

    LOG_GENERAL(INFO, "Added peer "
                          << RoleAndIndexToString(ptr->role, ptr->index)
                          << " at " << ptr->peer);
  }

  static constexpr size_t SERIALIZED_PEER_INFO_SIZE = 64;

  zbytes Serialize(const PeerInfo& p) {
    zbytes packet;
    packet.reserve(SERIALIZED_PEER_INFO_SIZE);

    // pub key
    std::ignore = p.pubKey.Serialize(packet, 0);

    // address
    std::ignore = p.peer.Serialize(packet, packet.size());

    // role
    Serializable::SetNumber(packet, packet.size(),
                            static_cast<uint8_t>(p.role));

    // index within the role
    Serializable::SetNumber(packet, packet.size(), p.index);

    return packet;
  }

  bool ReadPeerInfo(const zbytes& message, unsigned& offset, PeerInfo& pi) {
    // pub key
    if (!pi.pubKey.Deserialize(message, offset)) {
      // TODO log
      return false;
    }
    offset += 33;  // TODO check it ????

    if (message.size() < offset + 20 + 1 + 4) {
      // TODO log
      return false;
    }

    // address
    if (pi.peer.Deserialize(message, offset) < 0) {
      // TODO log
      return false;
    }
    offset += 20;

    auto role = Serializable::GetNumber<uint8_t>(message, offset);
    if (role >= static_cast<uint8_t>(Role::_ARRAY_SIZE_)) {
      // TODO log
      return false;
    }
    pi.role = static_cast<Role>(role);
    offset += 1;

    pi.index = Serializable::GetNumber<uint32_t>(message, offset);
    offset += 4;
    return true;
  }

  zbytes TakeSnapshot(uint64_t from_time_ago) {
    static constexpr Milliseconds LATENCY_FACTOR(3000);

    zbytes packet = m_helloMessage;

    // Self hello w/o snapshot request goes first
    packet.resize(packet.size() - 8);
    packet[1] = SNAPSHOT;

    if (from_time_ago == 0) {
      packet.reserve(packet.size() +
                     SERIALIZED_PEER_INFO_SIZE * m_peersByPubkey.size());
      for (const auto& [_, p] : m_peersByPubkey) {
        auto ser = Serialize(*p);
        packet.insert(packet.end(), ser.begin(), ser.end());
      }
    } else {
      auto t = Clock();
      if (uint64_t(t.count()) > from_time_ago) {
        t -= Milliseconds(from_time_ago);
      }
      if (t > LATENCY_FACTOR) {
        t -= LATENCY_FACTOR;
      }
      for (auto it = m_peersByTime.lower_bound(t); it != m_peersByTime.end();
           ++it) {
        packet.insert(packet.end(), it->second.begin(), it->second.end());
      }
    }

    return packet;
  }

  zbytes CreateHelloMessage() {
    zbytes packet{PEER, HELLO};
    packet.reserve(SERIALIZED_PEER_INFO_SIZE);

    // pub key
    std::ignore = m_options.selfPubKey.Serialize(packet, 0);

    // listen port
    Serializable::SetNumber(packet, packet.size(),
                            static_cast<uint16_t>(m_options.selfPort));

    // role
    Serializable::SetNumber(packet, packet.size(),
                            static_cast<uint8_t>(m_options.selfRole));

    // peer index within the role
    Serializable::SetNumber(packet, packet.size(), m_options.selfPeerIndex);

    // snapshot request time
    Serializable::SetNumber<uint64_t>(packet, packet.size(), 0ull);

    return packet;
  }

  bool ReadHelloMessage(const zbytes& packet, unsigned offset, PeerInfo& pi,
                        uint64_t& snapshotRequestTime) {
    if (!ReadHelloMessageBody(packet, offset, pi) ||
        packet.size() < offset + 8) {
      // TODO log
      return false;
    }
    snapshotRequestTime = Serializable::GetNumber<uint64_t>(packet, offset);
    return true;
  }

  bool ReadHelloMessageBody(const zbytes& packet, unsigned& offset,
                            PeerInfo& pi) {
    bool ok = pi.pubKey.Deserialize(packet, offset);
    if (!ok) {
      // TODO log
      return false;
    }
    offset += 33;  // TODO check it ????
    if (packet.size() < offset + 2 + 1 + 4) {
      // TODO log
      return false;
    }
    pi.peer.m_listenPortHost =
        Serializable::GetNumber<uint16_t>(packet, offset);
    offset += 2;
    uint8_t role = Serializable::GetNumber<uint8_t>(packet, offset);
    if (role >= static_cast<uint8_t>(Role::_ARRAY_SIZE_)) {
      // TODO log
      return false;
    }
    pi.role = static_cast<Role>(role);

    offset += 1;
    pi.index = Serializable::GetNumber<uint32_t>(packet, offset);
    offset += 4;
    return true;
  }

  void UpdateHelloMessage(Milliseconds ts) {
    assert(m_helloMessage.size() > 8);

    uint64_t t = 0;
    if (ts != Milliseconds(0)) {
      auto now = Clock();
      if (now > ts) {
        t = uint64_t((now - ts).count());
      }
    }
    Serializable::SetNumber<uint64_t>(m_helloMessage, m_helloMessage.size() - 8,
                                      t);
  }

  class RNG {
   public:
    RNG() : m_generator(m_rd()) {}

    zbytes GenRandomNonce() {
      static constexpr size_t NONCE_SIZE = 16;
      zbytes nonce;
      nonce.resize(NONCE_SIZE);
      std::uniform_int_distribution<uint8_t> bytes_distr;
      std::generate(nonce.begin(), nonce.end(), [this, &bytes_distr]() {
        return bytes_distr(m_generator);
      });
      return nonce;
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

  void OnTimer() {
    auto now = Clock();

    auto& registeredLookups = m_peersByRoles[static_cast<size_t>(Role::LOOKUP)];
    if (registeredLookups.empty()) {
      UpdateHelloMessage(Milliseconds(0));
      m_p2p.SendBroadcastMessage(m_options.lookups, m_helloMessage);
    } else {
      auto idx = m_rng.GenRandomIndex(registeredLookups.size());
      auto it = registeredLookups.begin();
      for (size_t i = 0; i < idx; ++i) {
        ++it;
      }
      auto& time = m_lastRequestSent[(*it)->pubKey];
      UpdateHelloMessage(time);
      time = now;
      m_p2p.SendMessage((*it)->peer, m_helloMessage);
    }

    if (!m_peersByTime.empty() && now > m_options.historyExpiration) {
      auto expireTime = now - m_options.historyExpiration;
      m_peersByTime.erase(m_peersByTime.begin(),
                          m_peersByTime.upper_bound(expireTime));
    }

    WaitTimer(m_timer, m_options.timerInterval, this,
              &PeerDiscoveryImpl::OnTimer);
  }

  boost::asio::io_context& m_asio;
  P2P& m_p2p;
  Options m_options;
  zbytes m_helloMessage;
  std::unordered_map<PubKey, PeerInfoPtr> m_peersByPubkey;
  std::array<PeerSet, static_cast<size_t>(Role::_ARRAY_SIZE_)> m_peersByRoles;
  std::multimap<Milliseconds, zbytes> m_peersByTime;
  std::unordered_map<PubKey, Milliseconds> m_lastRequestSent;
  SteadyTimer m_timer;
  // TODO remove non-responding peers std::optional<PubKey>
  // m_waitingForReplyFrom;
  RNG m_rng;
};

}  // namespace zil::p2p
