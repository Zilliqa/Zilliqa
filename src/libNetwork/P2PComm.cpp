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

/* TCP error code:
 * https://www.gnu.org/software/libc/manual/html_node/Error-Codes.html */

#include <arpa/inet.h>
#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event-config.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/thread.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "Blacklist.h"
#include "P2PComm.h"
#include "SendJobs.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/SafeMath.h"
#include "common/Messages.h"
#include "libMetrics/Api.h"

using namespace std;
using namespace boost::multiprecision;

using zil::p2p::HASH_LEN;
using zil::p2p::HDR_LEN;

const unsigned int GOSSIP_MSGTYPE_LEN = 1;
const unsigned int GOSSIP_ROUND_LEN = 4;
const unsigned int GOSSIP_SNDR_LISTNR_PORT_LEN = 4;

namespace zil {
namespace local {

class P2PVariables {
  std::atomic<int> broadcastReceived = 0;
  std::atomic<int> gossipReceived = 0;
  std::atomic<int> normalReceived = 0;
  std::atomic<int> gossipReceivedForward = 0;
  std::atomic<int> eventCallback = 0;
  std::atomic<int> eventCallbackTooFewBytes = 0;
  std::atomic<int> eventCallbackFailure = 0;
  std::atomic<int> eventCallbackServerSeed = 0;
  std::atomic<int> newConnections = 0;

 public:
  std::unique_ptr<Z_I64GAUGE> temp;

  void AddBroadcastReceived(int count) {
    Init();
    broadcastReceived += count;
  }

  void AddGossipReceived(int count) {
    Init();
    gossipReceived += count;
  }

  void AddNormalReceived(int count) {
    Init();
    normalReceived += count;
  }

  void AddGossipReceivedForward(int count) {
    Init();
    gossipReceivedForward += count;
  }

  void AddEventCallback(int count) {
    Init();
    eventCallback += count;
  }

  void AddEventCallbackTooFewBytes(int count) {
    Init();
    eventCallbackTooFewBytes += count;
  }

  void AddEventCallbackFailure(int count) {
    Init();
    eventCallbackFailure += count;
  }

  void AddEventCbServerSeed(int count) {
    Init();
    eventCallbackServerSeed += count;
  }

  void AddNewConnections(int count) {
    Init();
    newConnections += count;
  }

  void Init() {
    if (!temp) {
      temp = std::make_unique<Z_I64GAUGE>(Z_FL::BLOCKS, "p2p.gauge",
                                          "P2P metrics", "calls", true);

      temp->SetCallback([this](auto&& result) {
        result.Set(broadcastReceived.load(), {{"counter", "BroadcastReceived"}});
        result.Set(gossipReceived.load(), {{"counter", "GossipReceived"}});
        result.Set(normalReceived.load(), {{"counter", "NormalReceived"}});
        result.Set(gossipReceivedForward.load(), {{"counter", "GossipReceivedForward"}});
        result.Set(eventCallback.load(), {{"counter", "EventCallback"}});
        result.Set(eventCallbackTooFewBytes.load(), {{"counter", "EventCallbackTooFewBytes"}});
        result.Set(eventCallbackFailure.load(), {{"counter", "EventCallbackFailure"}});
        result.Set(eventCallbackServerSeed.load(), {{"counter", "EventCallbackServerSeed"}});
        result.Set(newConnections.load(), {{"counter", "NewConnections"}});
      });
    }
  }
};

static P2PVariables variables{};

}  // namespace local
}  // namespace zil

zil::p2p::Dispatcher P2PComm::m_dispatcher;
std::mutex P2PComm::m_mutexPeerConnectionCount;
std::map<uint128_t, uint16_t> P2PComm::m_peerConnectionCount;
std::mutex P2PComm::m_mutexBufferEvent;
std::map<std::string, struct bufferevent*> P2PComm::m_bufferEventMap;


/// Comparison operator for ordering the list of message hashes.
struct HashCompare {
  bool operator()(const zbytes& l, const zbytes& r) {
    return equal(l.begin(), l.end(), r.begin(), r.end());
  }
};

static bool comparePairSecond(
    const pair<zbytes, chrono::time_point<chrono::system_clock>>& a,
    const pair<zbytes, chrono::time_point<chrono::system_clock>>& b) {
  return a.second < b.second;
}

P2PComm::P2PComm() {
  // set libevent m_base to NULL
  m_base = NULL;
  auto func = [this]() -> void {
    zbytes emptyHash;

    while (true) {
      this_thread::sleep_for(chrono::seconds(BROADCAST_INTERVAL));
      lock(m_broadcastToRemoveMutex, m_broadcastHashesMutex);
      lock_guard<mutex> g(m_broadcastToRemoveMutex, adopt_lock);
      lock_guard<mutex> g2(m_broadcastHashesMutex, adopt_lock);

      if (m_broadcastToRemove.empty() ||
          m_broadcastToRemove.front().second >
              chrono::system_clock::now() - chrono::seconds(BROADCAST_EXPIRY)) {
        continue;
      }

      auto up = upper_bound(
          m_broadcastToRemove.begin(), m_broadcastToRemove.end(),
          make_pair(emptyHash, chrono::system_clock::now() -
                                   chrono::seconds(BROADCAST_EXPIRY)),
          comparePairSecond);

      for (auto it = m_broadcastToRemove.begin(); it != up; ++it) {
        m_broadcastHashes.erase(it->first);
      }

      m_broadcastToRemove.erase(m_broadcastToRemove.begin(), up);
    }
  };

  DetachedFunction(1, func);
}

P2PComm::~P2PComm() { m_base = NULL; }

P2PComm& P2PComm::GetInstance() {
  static P2PComm comm;
  return comm;
}

void P2PComm::ClearBroadcastHashAsync(const zbytes& message_hash) {
  LOG_MARKER();
  lock_guard<mutex> guard(m_broadcastToRemoveMutex);
  m_broadcastToRemove.emplace_back(message_hash, chrono::system_clock::now());
}

namespace {

inline std::shared_ptr<zil::p2p::Message> MakeMsg(zbytes msg, Peer peer,
                                                  uint8_t startByte,
                                                  std::string& traceContext) {
  auto r = std::make_shared<zil::p2p::Message>();
  r->msg = std::move(msg);
  r->traceContext = std::move(traceContext);
  r->from = std::move(peer);
  r->startByte = startByte;
  return r;
}

}  // namespace

void P2PComm::ProcessBroadCastMsg(zbytes& message, zbytes& hash,
                                  const Peer& from, std::string& traceInfo) {
  P2PComm& p2p = P2PComm::GetInstance();

  // Check if this message has been received before
  bool found = false;
  zil::local::variables.AddBroadcastReceived(1);
  {
    lock_guard<mutex> guard(p2p.m_broadcastHashesMutex);

    found = (p2p.m_broadcastHashes.find(hash) != p2p.m_broadcastHashes.end());
    // While we have the lock, we should quickly add the hash
    if (!found) {
      SHA256Calculator sha256;
      sha256.Update(message);
      zbytes this_msg_hash = sha256.Finalize();

      if (this_msg_hash == hash) {
        p2p.m_broadcastHashes.insert(this_msg_hash);
      } else {
        LOG_GENERAL(WARNING, "Incorrect message hash.");
        return;
      }
    }
  }

  if (found) {
    // We already sent and/or received this message before -> discard
    LOG_GENERAL(INFO, "Discarding duplicate");
    return;
  }

  p2p.ClearBroadcastHashAsync(hash);

  string msgHashStr;
  if (!DataConversion::Uint8VecToHexStr(hash, msgHashStr)) {
    return;
  }

  LOG_STATE("[BROAD][" << std::setw(15) << std::left << p2p.m_selfPeer << "]["
                       << msgHashStr.substr(0, 6) << "] RECV");

  // Queue the message
  m_dispatcher(MakeMsg(std::move(message), from, zil::p2p::START_BYTE_BROADCAST,
                       traceInfo));
}

/*static*/ void P2PComm::ProcessGossipMsg(zbytes& message, Peer& from,
                                          std::string& traceInfo) {
  unsigned char gossipMsgTyp = message.at(0);

  zil::local::variables.AddGossipReceived(1);

  const uint32_t gossipMsgRound = (message.at(GOSSIP_MSGTYPE_LEN) << 24) +
                                  (message.at(GOSSIP_MSGTYPE_LEN + 1) << 16) +
                                  (message.at(GOSSIP_MSGTYPE_LEN + 2) << 8) +
                                  message.at(GOSSIP_MSGTYPE_LEN + 3);

  const uint32_t gossipSenderPort =
      (message.at(GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN) << 24) +
      (message.at(GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 1) << 16) +
      (message.at(GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 2) << 8) +
      message.at(GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 3);
  from.m_listenPortHost = gossipSenderPort;

  RumorManager::RawBytes rumor_message(message.begin() + GOSSIP_MSGTYPE_LEN +
                                           GOSSIP_ROUND_LEN +
                                           GOSSIP_SNDR_LISTNR_PORT_LEN,
                                       message.end());

  P2PComm& p2p = P2PComm::GetInstance();
  if (gossipMsgTyp == (uint8_t)RRS::Message::Type::FORWARD) {
    LOG_GENERAL(INFO, "Gossip type FORWARD from " << from);
    zil::local::variables.AddGossipReceivedForward(1);

    if (p2p.SpreadForeignRumor(rumor_message)) {
      // skip the keys and signature.
      zbytes tmp(rumor_message.begin() + PUB_KEY_SIZE +
                     SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE,
                 rumor_message.end());

      LOG_GENERAL(INFO, "Rumor size: " << tmp.size());

      // Queue the message
      m_dispatcher(MakeMsg(std::move(tmp), from, zil::p2p::START_BYTE_GOSSIP,
                           traceInfo));
    }
  } else {
    auto resp = p2p.m_rumorManager.RumorReceived(
        (unsigned int)gossipMsgTyp, gossipMsgRound, rumor_message, from);
    if (resp.first) {
      LOG_GENERAL(INFO, "Rumor size: " << rumor_message.size());

      // Queue the message
      m_dispatcher(MakeMsg(std::move(resp.second), from,
                           zil::p2p::START_BYTE_GOSSIP, traceInfo));
    }
  }
}

void P2PComm::CloseAndFreeBufferEvent(struct bufferevent* bufev) {
  int fd = bufferevent_getfd(bufev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  uint128_t ipAddr = cli_addr.sin_addr.s_addr;
  {
    std::unique_lock<std::mutex> lock(m_mutexPeerConnectionCount);
    if (m_peerConnectionCount[ipAddr] > 0) {
      m_peerConnectionCount[ipAddr]--;
    }
  }
  bufferevent_free(bufev);
}

void P2PComm::CloseAndFreeBevP2PSeedConnServer(struct bufferevent* bufev) {
  lock(m_mutexPeerConnectionCount, m_mutexBufferEvent);
  unique_lock<mutex> lock(m_mutexPeerConnectionCount, adopt_lock);
  lock_guard<mutex> g(m_mutexBufferEvent, adopt_lock);
  int fd = bufferevent_getfd(bufev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  char* strAdd = inet_ntoa(cli_addr.sin_addr);
  int port = cli_addr.sin_port;
  // TODO Remove log
  LOG_GENERAL(DEBUG, "P2PSeed CloseAndFreeBevP2PSeedConnServer ip="
                         << strAdd << " port=" << port << " bev=" << bufev);
  uint128_t ipAddr = cli_addr.sin_addr.s_addr;
  {
    if (m_peerConnectionCount[ipAddr] > 0) {
      m_peerConnectionCount[ipAddr]--;
      // TODO Remove log
      LOG_GENERAL(DEBUG, "P2PSeed decrementing connection count for ipaddr="
                             << ipAddr << " m_peerConnectionCount="
                             << m_peerConnectionCount[ipAddr]);
    }
  }
  bufferevent_setcb(bufev, NULL, NULL, NULL, NULL);
  bufferevent_free(bufev);
}

void P2PComm::CloseAndFreeBevP2PSeedConnClient(struct bufferevent* bufev,
                                               void* ctx) {
  lock(m_mutexPeerConnectionCount, m_mutexBufferEvent);
  unique_lock<mutex> lock(m_mutexPeerConnectionCount, adopt_lock);
  lock_guard<mutex> g(m_mutexBufferEvent, adopt_lock);
  int fd = bufferevent_getfd(bufev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  char* strAdd = inet_ntoa(cli_addr.sin_addr);
  int port = cli_addr.sin_port;
  // TODO Remove log
  LOG_GENERAL(DEBUG, "P2PSeed CloseAndFreeBevP2PSeedConnClient ip="
                         << strAdd << " port=" << port << " bev=" << bufev);
  uint128_t ipAddr = cli_addr.sin_addr.s_addr;
  {
    if (m_peerConnectionCount[ipAddr] > 0) {
      m_peerConnectionCount[ipAddr]--;
      // TODO Remove log
      LOG_GENERAL(DEBUG, "P2PSeed decrementing connection count for ipaddr="
                             << ipAddr << " m_peerConnectionCount="
                             << m_peerConnectionCount[ipAddr]);
    }
  }
  // free request msg memory
  zbytes* destBytes = (zbytes*)ctx;
  if (destBytes != NULL) {
    LOG_GENERAL(DEBUG, "P2PSeed Deleting ctx len=" << destBytes->size());
    delete destBytes;
    destBytes = NULL;
  }
  bufferevent_setcb(bufev, NULL, NULL, NULL, NULL);
  bufferevent_free(bufev);
}

void P2PComm::ClearPeerConnectionCount() {
  std::unique_lock<std::mutex> lock(m_mutexPeerConnectionCount);
  m_peerConnectionCount.clear();
}

void P2PComm::EventCallback(struct bufferevent* bev, short events,
                            [[gnu::unused]] void* ctx) {

  zil::local::variables.AddEventCallback(1);
  struct AutoClose {
    ~AutoClose() {
      if (bev) {
        CloseAndFreeBufferEvent(bev);
      }
    }
    struct bufferevent* bev;
  } auto_close{bev};

  if (events & BEV_EVENT_ERROR) {
    LOG_GENERAL(WARNING, "Error from bufferevent.");
    return;
  }

  // Not all bytes read out
  if (!(events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))) {
    LOG_GENERAL(WARNING, "Unknown error from bufferevent.");
    return;
  }

  // Get the IP info
  int fd = bufferevent_getfd(bev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  Peer from(cli_addr.sin_addr.s_addr, cli_addr.sin_port);

  // Get the data stored in buffer
  struct evbuffer* input = bufferevent_get_input(bev);
  if (input == NULL) {
    LOG_GENERAL(WARNING, "bufferevent_get_input failure.");
    return;
  }

  size_t len = evbuffer_get_length(input);
  if (len < zil::p2p::HDR_LEN) {
    // not enough bytes received, wait for the next callback
    auto_close.bev = nullptr;
    zil::local::variables.AddEventCallbackTooFewBytes(1);
    return;
  }

  const uint8_t* data = evbuffer_pullup(input, len);
  if (!data) {
    LOG_GENERAL(WARNING, "evbuffer_pullup failure.");
    return;
  }

  zil::p2p::ReadMessageResult result;
  auto state = zil::p2p::TryReadMessage(data, len, result);

  if (state == zil::p2p::ReadState::NOT_ENOUGH_DATA) {
    // not enough bytes received, wait for the next callback
    LOG_GENERAL(DEBUG, "not enough data");
    auto_close.bev = nullptr;
    return;
  }

  if (state != zil::p2p::ReadState::SUCCESS) {
    return;
  }

  std::ignore = evbuffer_drain(input, result.totalMessageBytes);

  if (result.startByte == zil::p2p::START_BYTE_BROADCAST) {
    LOG_PAYLOAD(INFO, "Incoming broadcast " << from, result.message,
                Logger::MAX_BYTES_TO_DISPLAY);

    if (result.message.size() <= HASH_LEN) {
      LOG_GENERAL(WARNING,
                  "Hash missing or empty broadcast message (messageLength = "
                      << result.message.size() << ")");
      return;
    }

    ProcessBroadCastMsg(result.message, result.hash, from, result.traceInfo);
  } else if (result.startByte == zil::p2p::START_BYTE_NORMAL) {
    LOG_PAYLOAD(INFO, "Incoming normal " << from, result.message,
                Logger::MAX_BYTES_TO_DISPLAY);

    zil::local::variables.AddNormalReceived(1);
    // Queue the message
    m_dispatcher(MakeMsg(std::move(result.message), from,
                         zil::p2p::START_BYTE_NORMAL, result.traceInfo));
  } else if (result.startByte == zil::p2p::START_BYTE_GOSSIP) {
    // Check for the maximum gossiped-message size
    if (result.message.size() >= MAX_GOSSIP_MSG_SIZE_IN_BYTES) {
      LOG_GENERAL(WARNING,
                  "Gossip message received [Size:"
                      << result.message.size() << "] is unexpectedly large [ >"
                      << MAX_GOSSIP_MSG_SIZE_IN_BYTES
                      << " ]. Will be strictly blacklisting the sender");
      Blacklist::GetInstance().Add(
          from.m_ipAddress);  // so we don't spend cost sending any data to this
                              // sender as well.
      return;
    }
    if (result.message.size() <
        GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + GOSSIP_SNDR_LISTNR_PORT_LEN) {
      LOG_GENERAL(
          WARNING,
          "Gossip Msg Type and/or Gossip Round and/or SNDR LISTNR is missing "
          "(messageLength = "
              << result.message.size() << ")");
      zil::local::variables.AddEventCallbackFailure(1);
      return;
    }

    ProcessGossipMsg(result.message, from, result.traceInfo);
  } else {
    // Unexpected start byte. Drop this message
    zil::local::variables.AddEventCallbackFailure(1);
    LOG_GENERAL(WARNING, "Incorrect start byte " << result.startByte);
  }
}

void P2PComm::EventCbServerSeed(struct bufferevent* bev, short events,
                                [[gnu::unused]] void* ctx) {
  zil::local::variables.AddEventCbServerSeed(1);
  int fd = bufferevent_getfd(bev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  Peer peer(cli_addr.sin_addr.s_addr, cli_addr.sin_port);
  LOG_GENERAL(DEBUG, "P2PSeed EventCbServer peer=" << peer << " bev=" << bev);

  // TODO Remove all if conditions except last one. For now debugging purpose
  // only
  if (DEBUG_LEVEL == 4) {
    if (events & BEV_EVENT_CONNECTED) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_CONNECTED");
    }
    if (events & BEV_EVENT_ERROR) {
      LOG_GENERAL(WARNING, "Error: P2PSeed BEV_EVENT_ERROR");
    }
    if (events & BEV_EVENT_READING) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_READING");
    }
    if (events & BEV_EVENT_WRITING) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_WRITING ");
    }
    if (events & BEV_EVENT_EOF) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_EOF");
    }
    if (events & BEV_EVENT_TIMEOUT) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_TIMEOUT");
    }
  }
  if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT)) {
    RemoveBevFromMap(peer);
    CloseAndFreeBevP2PSeedConnServer(bev);
  }
}

void P2PComm::ReadCallback(struct bufferevent* bev, [[gnu::unused]] void* ctx) {
  struct evbuffer* input = bufferevent_get_input(bev);

  size_t len = evbuffer_get_length(input);
  if (len >= MAX_READ_WATERMARK_IN_BYTES) {
    // Get the IP info
    int fd = bufferevent_getfd(bev);
    struct sockaddr_in cli_addr {};
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
    Peer from(cli_addr.sin_addr.s_addr, cli_addr.sin_port);
    LOG_GENERAL(WARNING, "[blacklist] Encountered data of size: "
                             << len << " being received."
                             << " Adding sending node "
                             << from.GetPrintableIPAddress()
                             << " as strictly blacklisted");
    Blacklist::GetInstance().Add(from.m_ipAddress);
    bufferevent_free(bev);
  }
}

void P2PComm::ReadCbServerSeed(struct bufferevent* bev,
                               [[gnu::unused]] void* ctx) {
  // Get the IP info
  int fd = bufferevent_getfd(bev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  Peer from(cli_addr.sin_addr.s_addr, cli_addr.sin_port);

  // Get the data stored in buffer
  struct evbuffer* input = bufferevent_get_input(bev);
  if (input == NULL) {
    LOG_GENERAL(WARNING, "Error: bufferevent_get_input failure.");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }
  size_t len = evbuffer_get_length(input);
  if (len == 0) {
    LOG_GENERAL(WARNING, "Error: evbuffer_get_length failure.");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }
  if (len >= MAX_READ_WATERMARK_IN_BYTES) {
    LOG_GENERAL(WARNING, "[blacklist] Encountered data of size: "
                             << len << " being received."
                             << " Adding sending node "
                             << from.GetPrintableIPAddress()
                             << " as strictly blacklisted");
    Blacklist::GetInstance().Add(from.m_ipAddress);
    CloseAndFreeBevP2PSeedConnServer(bev);
  }

  zbytes message(len);
  if (evbuffer_copyout(input, message.data(), len) !=
      static_cast<ev_ssize_t>(len)) {
    LOG_GENERAL(WARNING, "Error: evbuffer_copyout failure.");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }

  if (message.size() <= HDR_LEN) {
    LOG_GENERAL(WARNING, "Error: Empty message received.");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }

  const unsigned char version = message[0];

  // Check for version requirement
  if (version != (unsigned char)(MSG_VERSION & 0xFF)) {
    LOG_GENERAL(WARNING, "Header version wrong, received ["
                             << version - 0x00 << "] while expected ["
                             << MSG_VERSION << "].");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }

  const uint16_t networkid = (message[1] << 8) + message[2];
  if (networkid != NETWORK_ID) {
    LOG_GENERAL(WARNING, "Header networkid wrong, received ["
                             << networkid << "] while expected [" << NETWORK_ID
                             << "].");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }

  const uint32_t messageLength =
      (message[4] << 24) + (message[5] << 16) + (message[6] << 8) + message[7];

  {
    // Check for length consistency
    uint32_t res;

    if (!SafeMath<uint32_t>::sub(message.size(), HDR_LEN, res)) {
      LOG_GENERAL(WARNING, "Error: Unexpected subtraction operation!");
      CloseAndFreeBevP2PSeedConnServer(bev);
      return;
    }

    if (res > messageLength) {
      LOG_GENERAL(WARNING,
                  "Error: Received msg len is greater than header msg len")
      CloseAndFreeBevP2PSeedConnServer(bev);
      return;
    } else if (res < messageLength) {
      return;
    }
  }

  if (evbuffer_drain(input, len) != 0) {
    LOG_GENERAL(WARNING, "Error: evbuffer_drain failure.");
    CloseAndFreeBevP2PSeedConnServer(bev);
    return;
  }

  const unsigned char startByte = message[3];

  if (startByte == zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST) {
    LOG_PAYLOAD(INFO, "Incoming request from ext seed " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    string bufKey = from.GetPrintableIPAddress() + ":" +
                    boost::lexical_cast<string>(from.GetListenPortHost());
    LOG_GENERAL(DEBUG, "bufferEventMap key=" << bufKey << " msg len=" << len
                                             << " bev=" << bev);

    // Add bufferevent to map
    {
      lock_guard<mutex> g(m_mutexBufferEvent);
      m_bufferEventMap[bufKey] = bev;
    }
    // Queue the message
    std::string emptyStr;
    m_dispatcher(MakeMsg(zbytes(message.begin() + HDR_LEN, message.end()), from,
                         zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST, emptyStr));
  } else {
    // Unexpected start byte. Drop this message
    LOG_CHECK_FAIL("Start byte", startByte,
                   zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST);
    CloseAndFreeBevP2PSeedConnServer(bev);
  }
}

void P2PComm::AcceptConnectionCallback([[gnu::unused]] evconnlistener* listener,
                                       evutil_socket_t cli_sock,
                                       struct sockaddr* cli_addr,
                                       [[gnu::unused]] int socklen,
                                       [[gnu::unused]] void* arg) {
  Peer from(uint128_t(((struct sockaddr_in*)cli_addr)->sin_addr.s_addr),
            ((struct sockaddr_in*)cli_addr)->sin_port);

  zil::local::variables.AddNewConnections(1);
  LOG_GENERAL(DEBUG, "Connection from " << from);

  if (Blacklist::GetInstance().Exist(from.m_ipAddress,
                                     false /* for incoming message */)) {
    LOG_GENERAL(INFO, "The node "
                          << from
                          << " is in black list, block all message from it.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
  }

  {
    std::unique_lock<std::mutex> lock(m_mutexPeerConnectionCount);
    if (m_peerConnectionCount[from.GetIpAddress()] > MAX_PEER_CONNECTION) {
      LOG_GENERAL(WARNING, "Connection ignored from " << from);
      evutil_closesocket(cli_sock);
      return;
    }
    m_peerConnectionCount[from.GetIpAddress()]++;
  }

  // Set up buffer event for this new connection
  struct event_base* base = evconnlistener_get_base(listener);
  if (base == NULL) {
    LOG_GENERAL(WARNING, "evconnlistener_get_base failure.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
  }

  struct bufferevent* bev = bufferevent_socket_new(
      base, cli_sock, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
  if (bev == NULL) {
    LOG_GENERAL(WARNING, "bufferevent_socket_new failure.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
  }

  bufferevent_setwatermark(bev, EV_READ, MIN_READ_WATERMARK_IN_BYTES,
                           MAX_READ_WATERMARK_IN_BYTES);
  bufferevent_setcb(bev, ReadCallback, NULL, EventCallback, NULL);
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void P2PComm::RemoveBevFromMap(const Peer& peer) {
  lock_guard<mutex> g(m_mutexBufferEvent);

  string bufKey = peer.GetPrintableIPAddress() + ":" +
                  boost::lexical_cast<string>(peer.GetListenPortHost());
  LOG_GENERAL(DEBUG,
              "P2PSeed RemoveBufferEvent=" << peer << " bufKey =" << bufKey);
  auto it = m_bufferEventMap.find(bufKey);
  if (it != m_bufferEventMap.end()) {
    // TODO Remove this log
    if (DEBUG_LEVEL == 4) {
      LOG_GENERAL(DEBUG, "P2PSeed clearing bufferevent for bufKey="
                             << it->first << " bev=" << it->second);
      for (const auto& it2 : m_bufferEventMap) {
        LOG_GENERAL(DEBUG, " P2PSeed m_bufferEventMap key = "
                               << it2.first << " bev = " << it2.second);
      }
    }
    m_bufferEventMap.erase(it);
  }
}

void P2PComm::RemoveBevAndCloseP2PConnServer(const Peer& peer,
                                             const unsigned& startByteType) {
  LOG_MARKER();
  if (startByteType == zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST) {
    lock(m_mutexPeerConnectionCount, m_mutexBufferEvent);
    unique_lock<mutex> lock(m_mutexPeerConnectionCount, adopt_lock);
    lock_guard<mutex> g(m_mutexBufferEvent, adopt_lock);
    string bufKey = peer.GetPrintableIPAddress() + ":" +
                    boost::lexical_cast<string>(peer.GetListenPortHost());
    LOG_GENERAL(DEBUG,
                "P2PSeed RemoveBufferEvent=" << peer << " bufKey =" << bufKey);
    auto it = m_bufferEventMap.find(bufKey);
    if (it != m_bufferEventMap.end()) {
      // TODO Remove this log
      if (DEBUG_LEVEL == 4) {
        LOG_GENERAL(DEBUG, "P2PSeed clearing bufferevent for bufKey="
                               << it->first << " bev=" << it->second);
        for (const auto& it2 : m_bufferEventMap) {
          LOG_GENERAL(DEBUG, " P2PSeed m_bufferEventMap key = "
                                 << it2.first << " bev = " << it2.second);
        }
      }
      bufferevent* bufev = it->second;
      int fd = bufferevent_getfd(bufev);
      struct sockaddr_in cli_addr {};
      socklen_t addr_size = sizeof(struct sockaddr_in);
      getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
      char* strAdd = inet_ntoa(cli_addr.sin_addr);
      int port = cli_addr.sin_port;
      // TODO Remove log
      LOG_GENERAL(DEBUG, "P2PSeed RemoveBevAndCloseP2PConnServer ip="
                             << strAdd << " port=" << port << " bev=" << bufev);
      uint128_t ipAddr = cli_addr.sin_addr.s_addr;
      if (m_peerConnectionCount[ipAddr] > 0) {
        m_peerConnectionCount[ipAddr]--;
        // TODO Remove log
        LOG_GENERAL(DEBUG, "P2PSeed decrementing connection count for ipaddr="
                               << ipAddr << " m_peerConnectionCount="
                               << m_peerConnectionCount[ipAddr]);
      }
      bufferevent_setcb(bufev, NULL, NULL, NULL, NULL);
      bufferevent_free(bufev);
      m_bufferEventMap.erase(it);
    }
  }
}

void P2PComm ::EventCbClientSeed([[gnu::unused]] struct bufferevent* bev,
                                 short events, void* ctx) {
  int fd = bufferevent_getfd(bev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  Peer peer(cli_addr.sin_addr.s_addr, cli_addr.sin_port);
  zbytes* destBytes = (zbytes*)ctx;
  LOG_GENERAL(DEBUG, "P2PSeed EventCbClient peer=" << peer << " bev=" << bev);
  // TODO Remove all if conditions except last two. For now debugging purpose
  // only
  if (DEBUG_LEVEL == 4) {
    if (events & BEV_EVENT_ERROR) {
      LOG_GENERAL(WARNING, "Error: P2PSeed BEV_EVENT_ERROR");
    }
    if (events & BEV_EVENT_READING) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_READING");
    }
    if (events & BEV_EVENT_WRITING) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_WRITING ");
    }
    if (events & BEV_EVENT_EOF) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_EOF");
    }
    if (events & BEV_EVENT_TIMEOUT) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_TIMEOUT");
    }
    if (events & BEV_EVENT_CONNECTED) {
      LOG_GENERAL(DEBUG, "P2PSeed BEV_EVENT_CONNECTED req msg len="
                             << destBytes->size());
    }
  }
  if (events & BEV_EVENT_CONNECTED) {
    if (destBytes != NULL) {
      if (bufferevent_write(bev, &(destBytes->at(0)), destBytes->size()) < 0) {
        LOG_GENERAL(WARNING, "Error: P2PSeed bufferevent_write failed !!!");
      }
      struct timeval tv = {SEED_SYNC_LARGE_PULL_INTERVAL, 0};
      bufferevent_set_timeouts(bev, &tv, NULL);
    }
  } else if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT)) {
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
  }
}

void P2PComm ::ReadCbClientSeed(struct bufferevent* bev, void* ctx) {
  int fd = bufferevent_getfd(bev);
  struct sockaddr_in cli_addr {};
  socklen_t addr_size = sizeof(struct sockaddr_in);
  getpeername(fd, (struct sockaddr*)&cli_addr, &addr_size);
  Peer from(cli_addr.sin_addr.s_addr, cli_addr.sin_port);

  // Get the data stored in buffer
  struct evbuffer* input = bufferevent_get_input(bev);
  if (input == NULL) {
    LOG_GENERAL(WARNING, "Error: bufferevent_get_input failure.");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }
  size_t len = evbuffer_get_length(input);
  if (len == 0) {
    LOG_GENERAL(WARNING, "Error: evbuffer_get_length failure.");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }
  if (len >= MAX_READ_WATERMARK_IN_BYTES) {
    LOG_GENERAL(WARNING, "[blacklist] Encountered data of size: "
                             << len << " being received."
                             << " Adding sending node "
                             << from.GetPrintableIPAddress()
                             << " as strictly blacklisted");
    Blacklist::GetInstance().Add(from.m_ipAddress);
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
  }

  zbytes message(len);
  if (evbuffer_copyout(input, message.data(), len) !=
      static_cast<ev_ssize_t>(len)) {
    LOG_GENERAL(WARNING, "Error: evbuffer_copyout failure.");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }

  if (message.size() <= HDR_LEN) {
    LOG_GENERAL(WARNING, "Error: Empty message received.");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }

  const unsigned char version = message[0];

  // Check for version requirement
  if (version != (unsigned char)(MSG_VERSION & 0xFF)) {
    LOG_GENERAL(WARNING, "Header version wrong, received ["
                             << version - 0x00 << "] while expected ["
                             << MSG_VERSION << "].");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }

  const uint16_t networkid = (message[1] << 8) + message[2];
  if (networkid != NETWORK_ID) {
    LOG_GENERAL(WARNING, "Header networkid wrong, received ["
                             << networkid << "] while expected [" << NETWORK_ID
                             << "].");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }

  const uint32_t messageLength =
      (message[4] << 24) + (message[5] << 16) + (message[6] << 8) + message[7];

  {
    // Check for length consistency
    uint32_t res;

    if (!SafeMath<uint32_t>::sub(message.size(), HDR_LEN, res)) {
      LOG_GENERAL(WARNING, "Error: Unexpected subtraction operation!");
      CloseAndFreeBevP2PSeedConnClient(bev, ctx);
      return;
    }

    if (res > messageLength) {
      LOG_GENERAL(WARNING,
                  "Error: Received msg len is greater than header msg len")
      CloseAndFreeBevP2PSeedConnClient(bev, ctx);
      return;
    } else if (res < messageLength) {
      return;
    }
  }

  if (evbuffer_drain(input, len) != 0) {
    LOG_GENERAL(WARNING, "Error: evbuffer_drain failure.");
    CloseAndFreeBevP2PSeedConnClient(bev, ctx);
    return;
  }

  const unsigned char startByte = message[3];

  if (startByte == zil::p2p::START_BYTE_SEED_TO_SEED_RESPONSE) {
    LOG_PAYLOAD(INFO, "Incoming normal response from server seed " << from,
                message, Logger::MAX_BYTES_TO_DISPLAY);

    // Queue the message
    std::string emptyStr;
    m_dispatcher(MakeMsg(zbytes(message.begin() + HDR_LEN, message.end()), from,
                         zil::p2p::START_BYTE_SEED_TO_SEED_RESPONSE, emptyStr));
  } else {
    // Unexpected start byte. Drop this message
    LOG_CHECK_FAIL("Start byte", startByte,
                   zil::p2p::START_BYTE_SEED_TO_SEED_RESPONSE);
  }
  CloseAndFreeBevP2PSeedConnClient(bev, ctx);
}

// timeout event every 2 secs
// TODO Should move to class or global fun. check ?
static void DummyTimeoutEvent([[gnu::unused]] evutil_socket_t fd,
                              [[gnu::unused]] short what,
                              [[gnu::unused]] void* args) {}

void P2PComm::AcceptCbServerSeed([[gnu::unused]] evconnlistener* listener,
                                 evutil_socket_t cli_sock,
                                 struct sockaddr* cli_addr,
                                 [[gnu::unused]] int socklen,
                                 [[gnu::unused]] void* arg) {
  Peer from(uint128_t(((struct sockaddr_in*)cli_addr)->sin_addr.s_addr),
            ((struct sockaddr_in*)cli_addr)->sin_port);

  LOG_GENERAL(DEBUG, "Connection from " << from);

  {
    std::unique_lock<std::mutex> lock(m_mutexPeerConnectionCount);
    if (m_peerConnectionCount[from.GetIpAddress()] >
        MAX_PEER_CONNECTION_P2PSEED) {
      LOG_GENERAL(WARNING, "Connection ignored from " << from);
      evutil_closesocket(cli_sock);
      return;
    }
    m_peerConnectionCount[from.GetIpAddress()]++;
    // TODO Remove the log
    LOG_GENERAL(DEBUG, "P2PSeed m_peerConnectionCount="
                           << m_peerConnectionCount[from.GetIpAddress()]);
  }

  // Set up buffer event for this new connection
  struct event_base* base = evconnlistener_get_base(listener);
  if (base == NULL) {
    LOG_GENERAL(WARNING, "Error: evconnlistener_get_base failure.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
  }

  struct bufferevent* bev = bufferevent_socket_new(
      base, cli_sock,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE);
  if (bev == NULL) {
    LOG_GENERAL(WARNING, "Error: bufferevent_socket_new failure.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
  }

  bufferevent_setwatermark(bev, EV_READ, MIN_READ_WATERMARK_IN_BYTES,
                           MAX_READ_WATERMARK_IN_BYTES);
  bufferevent_setcb(bev, ReadCbServerSeed, NULL, EventCbServerSeed, NULL);
  struct timeval tv = {P2P_SEED_SERVER_CONNECTION_TIMEOUT, 0};
  bufferevent_set_timeouts(bev, &tv, NULL);
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void P2PComm::StartMessagePump(zil::p2p::Dispatcher dispatcher) {
  LOG_MARKER();

  if (!m_sendJobs) {
    m_sendJobs = zil::p2p::SendJobs::Create();
  }

  m_dispatcher = std::move(dispatcher);
}

void P2PComm::EnableListener(uint32_t listenPort, bool startSeedNodeListener) {
  LOG_MARKER();
  struct sockaddr_in serv_addr {};
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(listenPort);
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  /*
  Enable pthread support in libevent.Because there are many sender threads
   writing to same event base when trying to send message,
   Internally libevent will lock the event base
   before doing any operation
  */
  evthread_use_pthreads();
  // Create the listener
  m_base = event_base_new();
  if (m_base == NULL) {
    LOG_GENERAL(WARNING, "event_base_new failure.");
    // fixme: should we exit here?
    return;
  }

  struct evconnlistener* listener1 = evconnlistener_new_bind(
      m_base, AcceptConnectionCallback, nullptr,
      LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 4096,
      (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));
  if (listener1 == NULL) {
    LOG_GENERAL(FATAL, "evconnlistener_new_bind failure.");
    event_base_free(m_base);
    // fixme: should we exit here?
    return;
  }
  struct evconnlistener* listener2 = NULL;
  if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP && startSeedNodeListener) {
    LOG_GENERAL(INFO, "P2PSeed Start listener on " << P2P_SEED_CONNECT_PORT);
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(P2P_SEED_CONNECT_PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    listener2 = evconnlistener_new_bind(
        m_base, AcceptCbServerSeed, nullptr,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, 4096,
        (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));

    if (listener2 == NULL) {
      LOG_GENERAL(WARNING, "evconnlistener_new_bind failure.");
      event_base_free(m_base);
      // fixme: should we exit here?
      return;
    }
  }
  event_base_dispatch(m_base);
  evconnlistener_free(listener1);
  if (listener2 != NULL) {
    evconnlistener_free(listener2);
  }
  event_base_free(m_base);
}

// Start event loop
// Timer event is added to keep event loop active always since we do not have
// listener
void P2PComm::EnableConnect() {
  LOG_MARKER();
  /*
  Enable pthread support in libevent.Because there are many sender threads
   writing to same event base when trying to send message,
   Internally libevent will lock the event base
   before doing any operation
  */
  evthread_use_pthreads();
  m_base = event_base_new();
  event* e =
      event_new(m_base, -1, EV_TIMEOUT | EV_PERSIST, DummyTimeoutEvent, NULL);
  timeval twoSec = {2, 0};
  event_add(e, &twoSec);
  event_base_dispatch(m_base);
}

void P2PComm::WriteMsgOnBufferEvent(struct bufferevent* bev,
                                    const zbytes& message,
                                    const unsigned char& startByte) {
  LOG_MARKER();
  uint32_t length = message.size();
  unsigned char buf[HDR_LEN] = {(unsigned char)(MSG_VERSION & 0xFF),
                                (unsigned char)((NETWORK_ID >> 8) & 0XFF),
                                (unsigned char)(NETWORK_ID & 0xFF),
                                startByte,
                                (unsigned char)((length >> 24) & 0xFF),
                                (unsigned char)((length >> 16) & 0xFF),
                                (unsigned char)((length >> 8) & 0xFF),
                                (unsigned char)(length & 0xFF)};
  zbytes destMsg(std::begin(buf), std::end(buf));
  destMsg.insert(destMsg.end(), message.begin(), message.end());
  LOG_GENERAL(DEBUG, "P2PSeed msg len=" << length + HDR_LEN << " bev=" << bev
                                        << " destMsg size=" << destMsg.size());
  if (bufferevent_write(bev, &destMsg.at(0), HDR_LEN + length) < 0) {
    LOG_GENERAL(WARNING, "Error: P2PSeed bufferevent_write failed !!!");
    return;
  }
}

void P2PComm::SendMsgToSeedNodeOnWire(const Peer& peer, const Peer& fromPeer,
                                      const zbytes& message,
                                      const unsigned char& startByteType) {
  lock_guard<mutex> g(m_mutexBufferEvent);
  if (startByteType == zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST) {
    if (!MULTIPLIER_SYNC_MODE) {
      // seednode request message
      LOG_GENERAL(INFO, "P2PSeed request msg peer=" << peer);
      struct bufferevent* bev = bufferevent_socket_new(
          m_base, -1,
          BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS | BEV_OPT_THREADSAFE);
      if (bev == NULL) {
        LOG_GENERAL(WARNING, "Error: Bufferevent_socket_new failure.");
        return;
      }
      uint32_t length = message.size();
      unsigned char buf[HDR_LEN] = {(unsigned char)(MSG_VERSION & 0xFF),
                                    (unsigned char)((NETWORK_ID >> 8) & 0XFF),
                                    (unsigned char)(NETWORK_ID & 0xFF),
                                    zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST,
                                    (unsigned char)((length >> 24) & 0xFF),
                                    (unsigned char)((length >> 16) & 0xFF),
                                    (unsigned char)((length >> 8) & 0xFF),
                                    (unsigned char)(length & 0xFF)};
      zbytes destMsg(std::begin(buf), std::end(buf));
      destMsg.insert(destMsg.end(), message.begin(), message.end());
      LOG_GENERAL(DEBUG,
                  "P2PSeed msg len=" << length + HDR_LEN << " bev=" << bev
                                     << " destMsg size=" << destMsg.size());
      zbytes* destBytes = new zbytes(std::move(destMsg));
      bufferevent_setwatermark(bev, EV_READ, MIN_READ_WATERMARK_IN_BYTES,
                               MAX_READ_WATERMARK_IN_BYTES);
      bufferevent_setcb(bev, ReadCbClientSeed, NULL, EventCbClientSeed,
                        (void*)destBytes);
      bufferevent_enable(bev, EV_READ | EV_WRITE);

      struct sockaddr_in serv_addr {};
      serv_addr.sin_family = AF_INET;
      serv_addr.sin_addr.s_addr = peer.m_ipAddress.convert_to<unsigned long>();
      serv_addr.sin_port = htons(peer.m_listenPortHost);
      if (bufferevent_socket_connect(bev, (struct sockaddr*)&serv_addr,
                                     sizeof(serv_addr)) < 0) {
        /* Error starting connection */
        LOG_GENERAL(WARNING, "Error: Failed to establish socket connection !!!")
        bufferevent_free(bev);
        return;
      }
    } else {
      // seedpub response message
      LOG_GENERAL(INFO, "P2PSeed response msg peer=" << fromPeer);
      Peer requestingNode;
      requestingNode.m_ipAddress = fromPeer.m_ipAddress;
      requestingNode.m_listenPortHost = fromPeer.m_listenPortHost;
      string bufKey =
          requestingNode.GetPrintableIPAddress() + ":" +
          boost::lexical_cast<string>(requestingNode.GetListenPortHost());
      auto it = m_bufferEventMap.find(bufKey);
      if (it != m_bufferEventMap.end()) {
        WriteMsgOnBufferEvent(it->second, message,
                              zil::p2p::START_BYTE_SEED_TO_SEED_RESPONSE);
        // TODO Remove log
        if (DEBUG_LEVEL == 4) {
          for (const auto& it1 : m_bufferEventMap) {
            LOG_GENERAL(DEBUG, "P2PSeed m_bufferEventMap key="
                                   << it1.first << " bev=" << it1.second);
          }
        }
        m_bufferEventMap.erase(it);
      } else {
        LOG_GENERAL(WARNING,
                    "Error: P2PSeed send msg failed.Check if bufferevent is "
                    "cleaned up already");
      }
    }
  } else {
    LOG_GENERAL(WARNING, "Error: P2PSeed Invalid startbyte");
  }
}

namespace {

template <typename PeerList>
void SendMessageImpl(const std::shared_ptr<zil::p2p::SendJobs>& sendJobs,
                     const PeerList& peers, const zbytes& message,
                     unsigned char startByteType,
                     bool bAllowSendToRelaxedBlacklist,
                     bool inject_trace_context,
                     bool ignoreBlacklist) {
  LOG_GENERAL(INFO, " bAllowSendRelaxedBlacklist = "
                        << bAllowSendToRelaxedBlacklist
                        << " inject_trace_context = " << inject_trace_context
                        << " ignoreBlacklist = " << ignoreBlacklist);
  if (message.size() <= MessageOffset::BODY) {
    return;
  }

  if (peers.empty()) {
    LOG_GENERAL(WARNING, "Error: empty peer list");
    return;
  }

  if (!sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }

  static const zbytes no_hash;
  auto raw_msg = zil::p2p::CreateMessage(message, no_hash, startByteType,
                                         inject_trace_context);

  for (const auto& peer : peers) {
    sendJobs->SendMessageToPeer(peer, raw_msg, bAllowSendToRelaxedBlacklist, ignoreBlacklist);
  }
}

}  // namespace

void P2PComm::SendMessage(const vector<Peer>& peers, const zbytes& message,
                          unsigned char startByteType,
                          bool inject_trace_context) {
  SendMessageImpl(m_sendJobs, peers, message, startByteType, false,
                  inject_trace_context, false);
}

void P2PComm::SendMessage(const deque<Peer>& peers, const zbytes& message,
                          unsigned char startByteType,
                          bool bAllowSendToRelaxedBlacklist,
                          bool inject_trace_context,
                          bool ignoreBlacklist) {
  LOG_GENERAL(INFO, "Chetan SendMessage = bAllowSendToRelaxedBlacklist = "
                        << bAllowSendToRelaxedBlacklist
                        << " inject_trace_context = " << inject_trace_context
                        << " ignoreBlacklist = " << ignoreBlacklist);
  SendMessageImpl(m_sendJobs, peers, message, startByteType,
                  bAllowSendToRelaxedBlacklist, inject_trace_context, ignoreBlacklist);
}

void P2PComm::SendMessage(const Peer& peer, const zbytes& message,
                          unsigned char startByteType,
                          bool inject_trace_context) {
  if (!m_sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }
  if (message.size() <= MessageOffset::BODY) {
    return;
  }
  m_sendJobs->SendMessageToPeer(peer, message, startByteType,
                                inject_trace_context);
}

// Overloaded for p2pseed as we need actual socket port coming in from
// parameter. Seedpubs lookup will call this overloaded function
void P2PComm::SendMessage(const Peer& peer, const Peer& fromPeer,
                          const zbytes& message, unsigned char startByteType,
                          bool inject_trace_context) {
  if (ENABLE_SEED_TO_SEED_COMMUNICATION &&
      startByteType == zil::p2p::START_BYTE_SEED_TO_SEED_REQUEST) {
    SendMsgToSeedNodeOnWire(peer, fromPeer, message, startByteType);
    return;
  }
  if (message.size() <= MessageOffset::BODY) {
    return;
  }
  SendMessage(peer, message, startByteType, inject_trace_context);
}

namespace {

template <typename PeerList>
void SendBroadcastMessageImpl(
    const std::shared_ptr<zil::p2p::SendJobs>& sendJobs, const PeerList& peers,
    const Peer& selfPeer, const zbytes& message, zbytes& hash,
    bool inject_trace_context) {
  if (message.size() <= MessageOffset::BODY) {
    return;
  }

  if (peers.empty()) {
    return;
  }

  if (!sendJobs) {
    LOG_GENERAL(WARNING, "Message pump not started");
    return;
  }

  SHA256Calculator sha256;
  sha256.Update(message);
  hash = sha256.Finalize();

  auto raw_msg = zil::p2p::CreateMessage(
      message, hash, zil::p2p::START_BYTE_BROADCAST, inject_trace_context);

  string hashStr;
  if (selfPeer != Peer()) {
    if (!DataConversion::Uint8VecToHexStr(hash, hashStr)) {
      return;
    }
    LOG_STATE("[BROAD][" << std::setw(15) << std::left
                         << selfPeer.GetPrintableIPAddress() << "]["
                         << hashStr.substr(0, 6) << "] DONE");
  }

  for (const auto& peer : peers) {
    sendJobs->SendMessageToPeer(peer, raw_msg, false);
  }
}

}  // namespace

void P2PComm::SendBroadcastMessage(const vector<Peer>& peers,
                                   const zbytes& message,
                                   bool inject_trace_context) {
  LOG_MARKER();

  zbytes hash;
  SendBroadcastMessageImpl(m_sendJobs, peers, m_selfPeer, message, hash,
                           inject_trace_context);

  if (!hash.empty()) {
    lock_guard<mutex> guard(m_broadcastHashesMutex);
    m_broadcastHashes.emplace(std::move(hash));
  }
}

void P2PComm::SendBroadcastMessage(const deque<Peer>& peers,
                                   const zbytes& message,
                                   bool inject_trace_context) {
  LOG_MARKER();

  zbytes hash;
  SendBroadcastMessageImpl(m_sendJobs, peers, m_selfPeer, message, hash,
                           inject_trace_context);

  if (!hash.empty()) {
    lock_guard<mutex> guard(m_broadcastHashesMutex);
    m_broadcastHashes.emplace(std::move(hash));
  }
}

void P2PComm::SendMessageNoQueue(const Peer& peer, const zbytes& message,
                                 unsigned char startByteType) {
  if (Blacklist::GetInstance().Exist(peer.m_ipAddress)) {
    LOG_GENERAL(INFO, "The node "
                          << peer
                          << " is in black list, block all message to it.");
    return;
  }

  if (!m_sendJobs) {
    m_sendJobs = zil::p2p::SendJobs::Create();
  }
  m_sendJobs->SendMessageToPeerSynchronous(peer, message, startByteType);
}

bool P2PComm::SpreadRumor(const zbytes& message) {
  LOG_MARKER();
  return m_rumorManager.AddRumor(message);
}

bool P2PComm::SpreadForeignRumor(const zbytes& message) {
  LOG_MARKER();
  return m_rumorManager.AddForeignRumor(message);
}

void P2PComm::SendRumorToForeignPeer(const Peer& foreignPeer,
                                     const zbytes& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeer(foreignPeer, message);
}

void P2PComm::SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                                      const zbytes& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeers(foreignPeers, message);
}

void P2PComm::SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                                      const zbytes& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeers(foreignPeers, message);
}

void P2PComm::SetSelfPeer(const Peer& self) { m_selfPeer = self; }

void P2PComm::SetSelfKey(const PairOfKey& self) { m_selfKey = self; }

void P2PComm::InitializeRumorManager(
    const VectorOfNode& peers, const std::vector<PubKey>& fullNetworkKeys) {
  LOG_MARKER();

  m_rumorManager.StopRounds();
  if (m_rumorManager.Initialize(peers, m_selfPeer, m_selfKey,
                                fullNetworkKeys)) {
    if (peers.size() != 0) {
      m_rumorManager.StartRounds();
    }
    // Spread the buffered rumors
    m_rumorManager.SpreadBufferedRumors();
  }
}

void P2PComm::UpdatePeerInfoInRumorManager(const Peer& peer,
                                           const PubKey& pubKey) {
  LOG_MARKER();

  m_rumorManager.UpdatePeerInfo(peer, pubKey);
}

Signature P2PComm::SignMessage(const zbytes& message) {
  // LOG_MARKER();

  Signature signature;
  bool result = Schnorr::Sign(message, 0, message.size(), m_selfKey.first,
                              m_selfKey.second, signature);
  if (!result) {
    return Signature();
  }
  return signature;
}

bool P2PComm::VerifyMessage(const zbytes& message, const Signature& toverify,
                            const PubKey& pubKey) {
  // LOG_MARKER();
  bool result = Schnorr::Verify(message, 0, message.size(), toverify, pubKey);

  if (!result) {
    LOG_GENERAL(INFO, "Failed to verify message. Pubkey: " << pubKey);
  }
  return result;
}
