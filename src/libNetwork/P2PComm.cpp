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
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "Blacklist.h"
#include "P2PComm.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

using namespace std;
using namespace boost::multiprecision;

const unsigned char START_BYTE_NORMAL = 0x11;
const unsigned char START_BYTE_BROADCAST = 0x22;
const unsigned char START_BYTE_GOSSIP = 0x33;
const unsigned char START_BYTE_SEED_TO_SEED_REQUEST = 0x44;
const unsigned char START_BYTE_SEED_TO_SEED_RESPONSE = 0x55;

const unsigned int HDR_LEN = 8;
const unsigned int HASH_LEN = 32;
const unsigned int GOSSIP_MSGTYPE_LEN = 1;
const unsigned int GOSSIP_ROUND_LEN = 4;
const unsigned int GOSSIP_SNDR_LISTNR_PORT_LEN = 4;

P2PComm::Dispatcher P2PComm::m_dispatcher;
std::mutex P2PComm::m_mutexPeerConnectionCount;
std::map<uint128_t, uint16_t> P2PComm::m_peerConnectionCount;
std::mutex P2PComm::m_mutexBufferEvent;
std::map<std::string, struct bufferevent*> P2PComm::m_bufferEventMap;

/// Comparison operator for ordering the list of message hashes.
struct HashCompare {
  bool operator()(const bytes& l, const bytes& r) {
    return equal(l.begin(), l.end(), r.begin(), r.end());
  }
};

static void close_socket(int* cli_sock) {
  if (cli_sock != NULL) {
    shutdown(*cli_sock, SHUT_RDWR);
    close(*cli_sock);
  }
}

static bool comparePairSecond(
    const pair<bytes, chrono::time_point<chrono::system_clock>>& a,
    const pair<bytes, chrono::time_point<chrono::system_clock>>& b) {
  return a.second < b.second;
}

P2PComm::P2PComm() : m_sendQueue(SENDQUEUE_SIZE) {
  // set libevent m_base to NULL
  m_base = NULL;
  auto func = [this]() -> void {
    bytes emptyHash;

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

P2PComm::~P2PComm() {
  SendJob* job = NULL;
  while (m_sendQueue.pop(job)) {
    delete job;
  }
  m_base = NULL;
}

P2PComm& P2PComm::GetInstance() {
  static P2PComm comm;
  return comm;
}

uint32_t SendJob::writeMsg(const void* buf, int cli_sock, const Peer& from,
                           const uint32_t message_length) {
  uint32_t written_length = 0;

  while (written_length < message_length) {
    ssize_t n = write(cli_sock, (unsigned char*)buf + written_length,
                      message_length - written_length);
    LOG_GENERAL(DEBUG, "Sent chunk of " << n << " bytes");
    if (n <= 0) {
      if (P2PComm::IsHostHavingNetworkIssue()) {
        if (Blacklist::GetInstance().IsWhitelistedSeed(from.m_ipAddress)) {
          LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                   << errno << " (" << std::strerror(errno)
                                   << "). Adding seed "
                                   << from.GetPrintableIPAddress()
                                   << " as relaxed blacklisted");
          // Add this seed node to relaxed blacklist even if it is whitelisted
          // in general.
          Blacklist::GetInstance().Add(from.m_ipAddress, false, true);
        } else {
          LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                   << errno << " (" << std::strerror(errno)
                                   << "). Adding "
                                   << from.GetPrintableIPAddress()
                                   << " as strictly blacklisted");
          Blacklist::GetInstance().Add(from.m_ipAddress);  // strict
        }
      } else if (P2PComm::IsNodeNotRunning()) {
        LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                 << errno << " (" << std::strerror(errno)
                                 << "). Adding " << from.GetPrintableIPAddress()
                                 << " as relaxed blacklisted");
        Blacklist::GetInstance().Add(from.m_ipAddress, false);  // relaxed
      } else if (errno == EPIPE) {
        LOG_GENERAL(WARNING, " SIGPIPE detected. Error No: "
                                 << errno << " Desc: " << std::strerror(errno));
        // No retry as it is likely the other end terminate the conn due to
        // duplicated msg.
      } else {
        LOG_GENERAL(WARNING, "Socket write failed in message header. Code = "
                                 << errno << " Desc: " << std::strerror(errno)
                                 << ". IP address:" << from);
      }
      return written_length;
    }

    written_length += n;
  }

  if (written_length > 1000000) {
    LOG_GENERAL(INFO, "DEBUG: Sent a total of " << written_length << " bytes");
  }

  return written_length;
}

bool SendJob::SendMessageSocketCore(const Peer& peer, const bytes& message,
                                    unsigned char start_byte,
                                    const bytes& msg_hash) {
  // LOG_MARKER();
  LOG_PAYLOAD(DEBUG, "Sending to " << peer, message,
              Logger::MAX_BYTES_TO_DISPLAY);

  if (peer.m_ipAddress == 0 && peer.m_listenPortHost == 0) {
    LOG_GENERAL(INFO,
                "I am sending to 0.0.0.0 at port 0. Don't send anything.");
    return true;
  } else if (peer.m_listenPortHost == 0) {
    LOG_GENERAL(INFO, "I am sending to " << peer.GetPrintableIPAddress()
                                         << " at port 0. Investigate why!");
    return true;
  }

  try {
    int cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    unique_ptr<int, void (*)(int*)> cli_sock_closer(&cli_sock, close_socket);

    // LINUX HAS NO SO_NOSIGPIPE
    // int set = 1;
    // setsockopt(cli_sock, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set,
    // sizeof(int));
    signal(SIGPIPE, SIG_IGN);
    if (cli_sock < 0) {
      LOG_GENERAL(WARNING, "Socket creation failed. Code = "
                               << errno << " Desc: " << std::strerror(errno)
                               << ". IP address: " << peer);
      return false;
    }

    struct sockaddr_in serv_addr {};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = peer.m_ipAddress.convert_to<unsigned long>();
    serv_addr.sin_port = htons(peer.m_listenPortHost);

    if (connect(cli_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <
        0) {
      LOG_GENERAL(WARNING, "Socket connect failed. Code = "
                               << errno << " Desc: " << std::strerror(errno)
                               << ". IP address: " << peer);
      if (P2PComm::IsHostHavingNetworkIssue()) {
        if (Blacklist::GetInstance().IsWhitelistedSeed(peer.m_ipAddress)) {
          LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                   << errno << " (" << std::strerror(errno)
                                   << "). Adding seed "
                                   << peer.GetPrintableIPAddress()
                                   << " as relaxed blacklisted");
          // Add this seed node to relaxed blacklist even if it is whitelisted
          // in general.
          Blacklist::GetInstance().Add(peer.m_ipAddress, false, true);
        } else {
          LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                   << errno << " (" << std::strerror(errno)
                                   << "). Adding "
                                   << peer.GetPrintableIPAddress()
                                   << " as strictly blacklisted");
          Blacklist::GetInstance().Add(peer.m_ipAddress);  // strict
        }
      } else if (P2PComm::IsNodeNotRunning()) {
        LOG_GENERAL(WARNING, "[blacklist] Encountered "
                                 << errno << " (" << std::strerror(errno)
                                 << "). Adding " << peer.GetPrintableIPAddress()
                                 << " as relaxed blacklisted");
        Blacklist::GetInstance().Add(peer.m_ipAddress, false);
      }

      return false;
    }
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
    uint32_t length = message.size();

    if (start_byte == START_BYTE_BROADCAST) {
      length += HASH_LEN;
    }

    unsigned char buf[HDR_LEN] = {(unsigned char)(MSG_VERSION & 0xFF),
                                  (unsigned char)((NETWORK_ID >> 8) & 0XFF),
                                  (unsigned char)(NETWORK_ID & 0xFF),
                                  start_byte,
                                  (unsigned char)((length >> 24) & 0xFF),
                                  (unsigned char)((length >> 16) & 0xFF),
                                  (unsigned char)((length >> 8) & 0xFF),
                                  (unsigned char)(length & 0xFF)};

    uint32_t written = writeMsg(buf, cli_sock, peer, HDR_LEN);
    if (HDR_LEN != written) {
      LOG_CHECK_FAIL("Failed to write header bytes", written, HDR_LEN);
      return true;
    }

    if (start_byte != START_BYTE_BROADCAST) {
      writeMsg(&message.at(0), cli_sock, peer, length);
      return true;
    }

    if (HASH_LEN != writeMsg(&msg_hash.at(0), cli_sock, peer, HASH_LEN)) {
      LOG_GENERAL(WARNING, "Wrong message hash length.");
      return true;
    }

    length -= HASH_LEN;
    writeMsg(&message.at(0), cli_sock, peer, length);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with write socket." << ' ' << e.what());
    return false;
  }
  return true;
}

void SendJob::SendMessageCore(const Peer& peer, const bytes& message,
                              unsigned char startbyte, const bytes& hash) {
  uint32_t retry_counter = 0;
  while (!SendMessageSocketCore(peer, message, startbyte, hash)) {
    if (Blacklist::GetInstance().Exist(peer.m_ipAddress)) {
      return;
    }

    LOG_GENERAL(WARNING, "Socket connect failed " << retry_counter << "/"
                                                  << MAXRETRYCONN
                                                  << ". IP address: " << peer);

    if (++retry_counter > MAXRETRYCONN) {
      LOG_GENERAL(WARNING,
                  "Socket connect failed over " << MAXRETRYCONN << " times.");
      return;
    }
    this_thread::sleep_for(
        chrono::milliseconds(rand() % PUMPMESSAGE_MILLISECONDS + 1));
  }
}

void SendJobPeer::DoSend() {
  if (Blacklist::GetInstance().Exist(m_peer.m_ipAddress)) {
    LOG_GENERAL(INFO, m_peer << " is blacklisted - blocking all messages");
    return;
  }

  SendMessageCore(m_peer, m_message, m_startbyte, m_hash);
}

template <class T>
void SendJobPeers<T>::DoSend() {
  vector<unsigned int> indexes(m_peers.size());

  for (unsigned int i = 0; i < indexes.size(); i++) {
    indexes.at(i) = i;
  }
  random_shuffle(indexes.begin(), indexes.end());

  string hashStr;
  if ((m_startbyte == START_BYTE_BROADCAST) && (m_selfPeer != Peer())) {
    if (!DataConversion::Uint8VecToHexStr(m_hash, hashStr)) {
      return;
    }
    LOG_STATE("[BROAD][" << std::setw(15) << std::left
                         << m_selfPeer.GetPrintableIPAddress() << "]["
                         << hashStr.substr(0, 6) << "] BEGN");
  }

  for (vector<unsigned int>::const_iterator curr = indexes.begin();
       curr < indexes.end(); ++curr) {
    const Peer& peer = m_peers.at(*curr);

    /// TBD: Update the container dynamically when blacklist is updated
    if (Blacklist::GetInstance().Exist(peer.m_ipAddress,
                                       !m_allowSendToRelaxedBlacklist)) {
      LOG_GENERAL(INFO, peer << " is blacklisted - blocking all messages");
      continue;
    }

    SendMessageCore(peer, m_message, m_startbyte, m_hash);
  }

  if ((m_startbyte == START_BYTE_BROADCAST) && (m_selfPeer != Peer())) {
    LOG_STATE("[BROAD][" << std::setw(15) << std::left
                         << m_selfPeer.GetPrintableIPAddress() << "]["
                         << hashStr.substr(0, 6) << "] DONE");
  }
}

void P2PComm::ProcessSendJob(SendJob* job) {
  auto funcSendMsg = [job]() mutable -> void {
    job->DoSend();
    delete job;
  };
  m_SendPool.AddJob(funcSendMsg);
}

void P2PComm::ClearBroadcastHashAsync(const bytes& message_hash) {
  LOG_MARKER();
  lock_guard<mutex> guard(m_broadcastToRemoveMutex);
  m_broadcastToRemove.emplace_back(message_hash, chrono::system_clock::now());
}

void P2PComm::ProcessBroadCastMsg(bytes& message, const Peer& from) {
  bytes msg_hash(message.begin() + HDR_LEN,
                 message.begin() + HDR_LEN + HASH_LEN);

  P2PComm& p2p = P2PComm::GetInstance();

  // Check if this message has been received before
  bool found = false;
  {
    lock_guard<mutex> guard(p2p.m_broadcastHashesMutex);

    found =
        (p2p.m_broadcastHashes.find(msg_hash) != p2p.m_broadcastHashes.end());
    // While we have the lock, we should quickly add the hash
    if (!found) {
      SHA2<HashType::HASH_VARIANT_256> sha256;
      sha256.Update(message, HDR_LEN + HASH_LEN,
                    message.size() - HDR_LEN - HASH_LEN);
      bytes this_msg_hash = sha256.Finalize();

      if (this_msg_hash == msg_hash) {
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

  p2p.ClearBroadcastHashAsync(msg_hash);

  string msgHashStr;
  if (!DataConversion::Uint8VecToHexStr(msg_hash, msgHashStr)) {
    return;
  }

  LOG_STATE("[BROAD][" << std::setw(15) << std::left << p2p.m_selfPeer << "]["
                       << msgHashStr.substr(0, 6) << "] RECV");

  // Move the shared_ptr message to raw pointer type
  pair<bytes, std::pair<Peer, const unsigned char>>* raw_message =
      new pair<bytes, std::pair<Peer, const unsigned char>>(
          bytes(message.begin() + HDR_LEN + HASH_LEN, message.end()),
          std::make_pair(from, START_BYTE_BROADCAST));

  // Queue the message
  m_dispatcher(raw_message);
}

/*static*/ void P2PComm::ProcessGossipMsg(bytes& message, Peer& from) {
  unsigned char gossipMsgTyp = message.at(HDR_LEN);

  const uint32_t gossipMsgRound =
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN) << 24) +
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + 1) << 16) +
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + 2) << 8) +
      message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + 3);

  const uint32_t gossipSenderPort =
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN) << 24) +
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 1) << 16) +
      (message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 2) << 8) +
      message.at(HDR_LEN + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + 3);
  from.m_listenPortHost = gossipSenderPort;

  RumorManager::RawBytes rumor_message(
      message.begin() + HDR_LEN + GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN +
          GOSSIP_SNDR_LISTNR_PORT_LEN,
      message.end());

  P2PComm& p2p = P2PComm::GetInstance();
  if (gossipMsgTyp == (uint8_t)RRS::Message::Type::FORWARD) {
    LOG_GENERAL(INFO, "Gossip type FORWARD from " << from);

    if (p2p.SpreadForeignRumor(rumor_message)) {
      // skip the keys and signature.
      bytes tmp(rumor_message.begin() + PUB_KEY_SIZE +
                    SIGNATURE_CHALLENGE_SIZE + SIGNATURE_RESPONSE_SIZE,
                rumor_message.end());
      std::pair<bytes, std::pair<Peer, const unsigned char>>* raw_message =
          new pair<bytes, std::pair<Peer, const unsigned char>>(
              tmp, make_pair(from, START_BYTE_GOSSIP));

      LOG_GENERAL(INFO, "Rumor size: " << tmp.size());

      // Queue the message
      m_dispatcher(raw_message);
    }
  } else {
    auto resp = p2p.m_rumorManager.RumorReceived(
        (unsigned int)gossipMsgTyp, gossipMsgRound, rumor_message, from);
    if (resp.first) {
      std::pair<bytes, std::pair<Peer, const unsigned char>>* raw_message =
          new pair<bytes, std::pair<Peer, const unsigned char>>(
              resp.second, make_pair(from, START_BYTE_GOSSIP));

      LOG_GENERAL(INFO, "Rumor size: " << rumor_message.size());

      // Queue the message
      m_dispatcher(raw_message);
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
  bytes* destBytes = (bytes*)ctx;
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
  unique_ptr<struct bufferevent, decltype(&CloseAndFreeBufferEvent)>
      socket_closer(bev, CloseAndFreeBufferEvent);

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
  if (len == 0) {
    LOG_GENERAL(WARNING, "evbuffer_get_length failure.");
    return;
  }
  bytes message(len);
  if (evbuffer_copyout(input, message.data(), len) !=
      static_cast<ev_ssize_t>(len)) {
    LOG_GENERAL(WARNING, "evbuffer_copyout failure.");
    return;
  }
  if (evbuffer_drain(input, len) != 0) {
    LOG_GENERAL(WARNING, "evbuffer_drain failure.");
    return;
  }

  // Reception format:
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
  // 0x33 - start byte (gossip)
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
  // 0x01 ~ 0x04 - Gossip_Message_Type
  // <4-byte Age> <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0xLL 0xLL - 2-byte NETWORK_ID, defined in constant file
  // 0x33 - start byte (report)
  // 0x00 0x00 0x00 0x01 - 4-byte length of message
  // 0x00

  // Check for minimum message size
  if (message.size() <= HDR_LEN) {
    LOG_GENERAL(WARNING, "Empty message received.");
    return;
  }

  const unsigned char version = message[0];

  // Check for version requirement
  if (version != (unsigned char)(MSG_VERSION & 0xFF)) {
    LOG_GENERAL(WARNING, "Header version wrong, received ["
                             << version - 0x00 << "] while expected ["
                             << MSG_VERSION << "].");
    return;
  }

  const uint16_t networkid = (message[1] << 8) + message[2];
  if (networkid != NETWORK_ID) {
    LOG_GENERAL(WARNING, "Header networkid wrong, received ["
                             << networkid << "] while expected [" << NETWORK_ID
                             << "].");
    return;
  }

  const unsigned char startByte = message[3];

  const uint32_t messageLength =
      (message[4] << 24) + (message[5] << 16) + (message[6] << 8) + message[7];

  {
    // Check for length consistency
    uint32_t res;

    if (!SafeMath<uint32_t>::sub(message.size(), HDR_LEN, res)) {
      LOG_GENERAL(WARNING, "Unexpected subtraction operation!");
      return;
    }

    if (messageLength != res) {
      LOG_GENERAL(WARNING, "Incorrect message length.");
      return;
    }
  }

  if (startByte == START_BYTE_BROADCAST) {
    LOG_PAYLOAD(INFO, "Incoming broadcast " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    if (messageLength <= HASH_LEN) {
      LOG_GENERAL(WARNING,
                  "Hash missing or empty broadcast message (messageLength = "
                      << messageLength << ")");
      return;
    }

    ProcessBroadCastMsg(message, from);
  } else if (startByte == START_BYTE_NORMAL) {
    LOG_PAYLOAD(INFO, "Incoming normal " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    // Move the shared_ptr message to raw pointer type
    pair<bytes, std::pair<Peer, const unsigned char>>* raw_message =
        new pair<bytes, std::pair<Peer, const unsigned char>>(
            bytes(message.begin() + HDR_LEN, message.end()),
            std::make_pair(from, START_BYTE_NORMAL));

    // Queue the message
    m_dispatcher(raw_message);
  } else if (startByte == START_BYTE_GOSSIP) {
    // Check for the maximum gossiped-message size
    if (message.size() >= MAX_GOSSIP_MSG_SIZE_IN_BYTES) {
      LOG_GENERAL(WARNING,
                  "Gossip message received [Size:"
                      << message.size() << "] is unexpectedly large [ >"
                      << MAX_GOSSIP_MSG_SIZE_IN_BYTES
                      << " ]. Will be strictly blacklisting the sender");
      Blacklist::GetInstance().Add(
          from.m_ipAddress);  // so we dont spend cost sending any data to this
                              // sender as well.
      return;
    }
    if (messageLength <
        GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + GOSSIP_SNDR_LISTNR_PORT_LEN) {
      LOG_GENERAL(
          WARNING,
          "Gossip Msg Type and/or Gossip Round and/or SNDR LISTNR is missing "
          "(messageLength = "
              << messageLength << ")");
      return;
    }

    ProcessGossipMsg(message, from);
  } else {
    // Unexpected start byte. Drop this message
    LOG_GENERAL(WARNING, "Incorrect start byte.");
  }
}

void P2PComm::EventCbServerSeed(struct bufferevent* bev, short events,
                                [[gnu::unused]] void* ctx) {
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

  bytes message(len);
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

  if (startByte == START_BYTE_SEED_TO_SEED_REQUEST) {
    LOG_PAYLOAD(INFO, "Incoming request from ext seed " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    pair<bytes, pair<Peer, const unsigned char>>* raw_message =
        new pair<bytes, pair<Peer, const unsigned char>>(
            bytes(message.begin() + HDR_LEN, message.end()),
            std::make_pair(from, START_BYTE_SEED_TO_SEED_REQUEST));

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
    m_dispatcher(raw_message);
  } else {
    // Unexpected start byte. Drop this message
    LOG_CHECK_FAIL("Start byte", startByte, START_BYTE_SEED_TO_SEED_REQUEST);
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
      for (const auto& it : m_bufferEventMap) {
        LOG_GENERAL(DEBUG, " P2PSeed m_bufferEventMap key = "
                               << it.first << " bev = " << it.second);
      }
    }
    m_bufferEventMap.erase(it);
  }
}

void P2PComm::RemoveBevAndCloseP2PConnServer(const Peer& peer,
                                             const unsigned& startByteType) {
  LOG_MARKER();
  if (startByteType == START_BYTE_SEED_TO_SEED_REQUEST) {
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
        for (const auto& it : m_bufferEventMap) {
          LOG_GENERAL(DEBUG, " P2PSeed m_bufferEventMap key = "
                                 << it.first << " bev = " << it.second);
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
  bytes* destBytes = (bytes*)ctx;
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

  bytes message(len);
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

  if (startByte == START_BYTE_SEED_TO_SEED_RESPONSE) {
    LOG_PAYLOAD(INFO, "Incoming normal response from server seed " << from,
                message, Logger::MAX_BYTES_TO_DISPLAY);

    pair<bytes, std::pair<Peer, const unsigned char>>* raw_message =
        new pair<bytes, std::pair<Peer, const unsigned char>>(
            bytes(message.begin() + HDR_LEN, message.end()),
            make_pair(from, START_BYTE_SEED_TO_SEED_RESPONSE));

    // Queue the message
    m_dispatcher(raw_message);
  } else {
    // Unexpected start byte. Drop this message
    LOG_CHECK_FAIL("Start byte", startByte, START_BYTE_SEED_TO_SEED_RESPONSE);
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

inline bool P2PComm::IsHostHavingNetworkIssue() {
  return (errno == EHOSTUNREACH || errno == ETIMEDOUT);
}

inline bool P2PComm::IsNodeNotRunning() {
  return (errno == EHOSTDOWN || errno == ECONNREFUSED);
}

void P2PComm::StartMessagePump(Dispatcher dispatcher) {
  LOG_MARKER();

  // Launch the thread that reads messages from the send queue
  auto funcCheckSendQueue = [this]() mutable -> void {
    SendJob* job = NULL;
    while (true) {
      while (m_sendQueue.pop(job)) {
        ProcessSendJob(job);
      }
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  };
  DetachedFunction(1, funcCheckSendQueue);

  m_dispatcher = move(dispatcher);
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
      LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
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
    struct evconnlistener* listener2 = evconnlistener_new_bind(
        m_base, AcceptCbServerSeed, nullptr,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
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
                                    const bytes& message,
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
  bytes destMsg(std::begin(buf), std::end(buf));
  destMsg.insert(destMsg.end(), message.begin(), message.end());
  LOG_GENERAL(DEBUG, "P2PSeed msg len=" << length + HDR_LEN << " bev=" << bev
                                        << " destMsg size=" << destMsg.size());
  if (bufferevent_write(bev, &destMsg.at(0), HDR_LEN + length) < 0) {
    LOG_GENERAL(WARNING, "Error: P2PSeed bufferevent_write failed !!!");
    return;
  }
}

void P2PComm::SendMsgToSeedNodeOnWire(const Peer& peer, const Peer& fromPeer,
                                      const bytes& message,
                                      const unsigned char& startByteType) {
  lock_guard<mutex> g(m_mutexBufferEvent);
  if (startByteType == START_BYTE_SEED_TO_SEED_REQUEST) {
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
                                    START_BYTE_SEED_TO_SEED_REQUEST,
                                    (unsigned char)((length >> 24) & 0xFF),
                                    (unsigned char)((length >> 16) & 0xFF),
                                    (unsigned char)((length >> 8) & 0xFF),
                                    (unsigned char)(length & 0xFF)};
      bytes destMsg(std::begin(buf), std::end(buf));
      destMsg.insert(destMsg.end(), message.begin(), message.end());
      LOG_GENERAL(DEBUG,
                  "P2PSeed msg len=" << length + HDR_LEN << " bev=" << bev
                                     << " destMsg size=" << destMsg.size());
      bytes* destBytes = new bytes(std::move(destMsg));
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
                              START_BYTE_SEED_TO_SEED_RESPONSE);
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

void P2PComm::SendMessage(const vector<Peer>& peers, const bytes& message,
                          const unsigned char& startByteType) {
  if (peers.empty()) {
    return;
  }

  // Make job
  SendJob* job = new SendJobPeers<vector<Peer>>;
  dynamic_cast<SendJobPeers<vector<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = startByteType;
  job->m_message = message;
  job->m_hash.clear();
  job->m_allowSendToRelaxedBlacklist = false;

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }
}

void P2PComm::SendMessage(const deque<Peer>& peers, const bytes& message,
                          const unsigned char& startByteType,
                          const bool bAllowSendToRelaxedBlacklist) {
  // LOG_MARKER();
  if (peers.empty()) {
    return;
  }

  // Make job
  SendJob* job = new SendJobPeers<deque<Peer>>;
  dynamic_cast<SendJobPeers<deque<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = startByteType;
  job->m_message = message;
  job->m_hash.clear();
  job->m_allowSendToRelaxedBlacklist = bAllowSendToRelaxedBlacklist;

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }
}

void P2PComm::SendMessage(const Peer& peer, const bytes& message,
                          const unsigned char& startByteType) {
  // Make job
  SendJob* job = new SendJobPeer;
  dynamic_cast<SendJobPeer*>(job)->m_peer = peer;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = startByteType;
  job->m_message = message;
  job->m_hash.clear();
  job->m_allowSendToRelaxedBlacklist = false;

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }
}

// Overloaded for p2pseed as we need actual socket port coming in from
// parameter. Seedpubs lookup will call this overloaded function
void P2PComm::SendMessage(const Peer& peer, const Peer& fromPeer,
                          const bytes& message,
                          const unsigned char& startByteType) {
  if (ENABLE_SEED_TO_SEED_COMMUNICATION &&
      startByteType == START_BYTE_SEED_TO_SEED_REQUEST) {
    SendMsgToSeedNodeOnWire(peer, fromPeer, message, startByteType);
    return;
  }

  // Make job
  SendJob* job = new SendJobPeer;
  dynamic_cast<SendJobPeer*>(job)->m_peer = peer;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = startByteType;
  job->m_message = message;
  job->m_hash.clear();
  job->m_allowSendToRelaxedBlacklist = false;

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }
}

void P2PComm::SendBroadcastMessage(const vector<Peer>& peers,
                                   const bytes& message) {
  LOG_MARKER();

  if (peers.empty()) {
    return;
  }

  SHA2<HashType::HASH_VARIANT_256> sha256;
  sha256.Update(message);

  // Make job
  SendJob* job = new SendJobPeers<vector<Peer>>;
  dynamic_cast<SendJobPeers<vector<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = START_BYTE_BROADCAST;
  job->m_message = message;
  job->m_hash = sha256.Finalize();
  job->m_allowSendToRelaxedBlacklist = false;

  bytes hashCopy(job->m_hash);

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }

  lock_guard<mutex> guard(m_broadcastHashesMutex);
  m_broadcastHashes.insert(hashCopy);
}

void P2PComm::SendBroadcastMessage(const deque<Peer>& peers,
                                   const bytes& message) {
  LOG_MARKER();

  if (peers.empty()) {
    return;
  }

  SHA2<HashType::HASH_VARIANT_256> sha256;
  sha256.Update(message);

  // Make job
  SendJob* job = new SendJobPeers<deque<Peer>>;
  dynamic_cast<SendJobPeers<deque<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = START_BYTE_BROADCAST;
  job->m_message = message;
  job->m_hash = sha256.Finalize();
  job->m_allowSendToRelaxedBlacklist = false;

  bytes hashCopy(job->m_hash);

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
    delete job;
  }

  lock_guard<mutex> guard(m_broadcastHashesMutex);
  m_broadcastHashes.insert(hashCopy);
}

void P2PComm::SendMessageNoQueue(const Peer& peer, const bytes& message,
                                 const unsigned char& startByteType) {
  // LOG_MARKER();

  if (Blacklist::GetInstance().Exist(peer.m_ipAddress)) {
    LOG_GENERAL(INFO, "The node "
                          << peer
                          << " is in black list, block all message to it.");
    return;
  }

  SendJob::SendMessageCore(peer, message, startByteType, {});
}

bool P2PComm::SpreadRumor(const bytes& message) {
  LOG_MARKER();
  return m_rumorManager.AddRumor(message);
}

bool P2PComm::SpreadForeignRumor(const bytes& message) {
  LOG_MARKER();
  return m_rumorManager.AddForeignRumor(message);
}

void P2PComm::SendRumorToForeignPeer(const Peer& foreignPeer,
                                     const bytes& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeer(foreignPeer, message);
}

void P2PComm::SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                                      const bytes& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeers(foreignPeers, message);
}

void P2PComm::SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                                      const bytes& message) {
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

Signature P2PComm::SignMessage(const bytes& message) {
  // LOG_MARKER();

  Signature signature;
  bool result = Schnorr::Sign(message, 0, message.size(), m_selfKey.first,
                              m_selfKey.second, signature);
  if (!result) {
    return Signature();
  }
  return signature;
}

bool P2PComm::VerifyMessage(const bytes& message, const Signature& toverify,
                            const PubKey& pubKey) {
  // LOG_MARKER();
  bool result = Schnorr::Verify(message, 0, message.size(), toverify, pubKey);

  if (!result) {
    LOG_GENERAL(INFO, "Failed to verify message. Pubkey: " << pubKey);
  }
  return result;
}
