/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <errno.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event-config.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <memory>

#include "Blacklist.h"
#include "P2PComm.h"
#include "PeerStore.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

const unsigned char START_BYTE_NORMAL = 0x11;
const unsigned char START_BYTE_BROADCAST = 0x22;
const unsigned char START_BYTE_GOSSIP = 0x33;
const unsigned int HDR_LEN = 6;
const unsigned int HASH_LEN = 32;
const unsigned int GOSSIP_MSGTYPE_LEN = 1;
const unsigned int GOSSIP_ROUND_LEN = 4;
const unsigned int GOSSIP_SNDR_LISTNR_PORT_LEN = 4;

P2PComm::Dispatcher P2PComm::m_dispatcher;
P2PComm::BroadcastListFunc P2PComm::m_broadcast_list_retriever;

/// Comparison operator for ordering the list of message hashes.
struct hash_compare {
  bool operator()(const vector<unsigned char>& l,
                  const vector<unsigned char>& r) {
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
    const pair<vector<unsigned char>, chrono::time_point<chrono::system_clock>>&
        a,
    const pair<vector<unsigned char>, chrono::time_point<chrono::system_clock>>&
        b) {
  return a.second < b.second;
}

P2PComm::P2PComm() : m_sendQueue(SENDQUEUE_SIZE) {
  auto func = [this]() -> void {
    std::vector<unsigned char> emptyHash;

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

    if (errno == EPIPE) {
      LOG_GENERAL(WARNING, " SIGPIPE detected. Error No: "
                               << errno << " Desc: " << std::strerror(errno));
      return written_length;
      // No retry as it is likely the other end terminate the conn due to
      // duplicated msg.
    }

    if (n <= 0) {
      LOG_GENERAL(WARNING, "Socket write failed in message header. Code = "
                               << errno << " Desc: " << std::strerror(errno)
                               << ". IP address:" << from);
      return written_length;
    }

    written_length += n;
  }

  if (written_length > 1000000) {
    LOG_GENERAL(INFO, "DEBUG: Sent a total of " << written_length << " bytes");
  }

  return written_length;
}

bool SendJob::SendMessageSocketCore(const Peer& peer,
                                    const std::vector<unsigned char>& message,
                                    unsigned char start_byte,
                                    const vector<unsigned char>& msg_hash) {
  // LOG_MARKER();
  LOG_PAYLOAD(DEBUG, "Sending message to " << peer, message,
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

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = peer.m_ipAddress.convert_to<unsigned long>();
    serv_addr.sin_port = htons(peer.m_listenPortHost);

    if (connect(cli_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) <
        0) {
      LOG_GENERAL(WARNING, "Socket connect failed. Code = "
                               << errno << " Desc: " << std::strerror(errno)
                               << ". IP address: " << peer);
      return false;
    }

    // Transmission format:
    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x11 - start byte
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
    // <message>

    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x22 - start byte (broadcast)
    // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
    // <32-byte hash> <message>

    // 0x01 ~ 0xFF - version, defined in constant file
    // 0x33 - start byte (report)
    // 0x00 0x00 0x00 0x01 - 4-byte length of message
    // 0x00
    uint32_t length = message.size();

    if (start_byte == START_BYTE_BROADCAST) {
      length += HASH_LEN;
    }

    unsigned char buf[HDR_LEN] = {(unsigned char)(MSG_VERSION & 0xFF),
                                  start_byte,
                                  (unsigned char)((length >> 24) & 0xFF),
                                  (unsigned char)((length >> 16) & 0xFF),
                                  (unsigned char)((length >> 8) & 0xFF),
                                  (unsigned char)(length & 0xFF)};

    if (HDR_LEN != writeMsg(buf, cli_sock, peer, HDR_LEN)) {
      LOG_GENERAL(INFO, "DEBUG: not written_length == " << HDR_LEN);
    }

    if (start_byte != START_BYTE_BROADCAST) {
      writeMsg(&message.at(0), cli_sock, peer, length);
      return true;
    }

    if (HASH_LEN != writeMsg(&msg_hash.at(0), cli_sock, peer, HASH_LEN)) {
      LOG_GENERAL(WARNING, "Wrong message hash length.");
      return false;
    }

    length -= HASH_LEN;
    writeMsg(&message.at(0), cli_sock, peer, length);
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Error with write socket." << ' ' << e.what());
    return false;
  }
  return true;
}

void SendJob::SendMessageCore(const Peer& peer,
                              const vector<unsigned char> message,
                              unsigned char startbyte,
                              const vector<unsigned char> hash) {
  uint32_t retry_counter = 0;
  while (!SendMessageSocketCore(peer, message, startbyte, hash)) {
    retry_counter++;
    LOG_GENERAL(WARNING, "Socket connect failed " << retry_counter << "/"
                                                  << MAXRETRYCONN
                                                  << ". IP address: " << peer);

    if (retry_counter > MAXRETRYCONN) {
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
    LOG_GENERAL(INFO, "The node "
                          << m_peer
                          << " is in black list, block all message to it.");
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

  if ((m_startbyte == START_BYTE_BROADCAST) && (m_selfPeer != Peer())) {
    LOG_STATE(
        "[BROAD][" << std::setw(15) << std::left
                   << m_selfPeer.GetPrintableIPAddress() << "]["
                   << DataConversion::Uint8VecToHexStr(m_hash).substr(0, 6)
                   << "] BEGN");
  }

  for (vector<unsigned int>::const_iterator curr = indexes.begin();
       curr < indexes.end(); curr++) {
    const Peer& peer = m_peers.at(*curr);

    /// TBD: Update the container dynamically when blacklist is updated
    if (Blacklist::GetInstance().Exist(peer.m_ipAddress)) {
      LOG_GENERAL(INFO, "The node "
                            << peer
                            << " is in black list, block all message to it.");
      continue;
    }

    SendMessageCore(peer, m_message, m_startbyte, m_hash);
  }

  if ((m_startbyte == START_BYTE_BROADCAST) && (m_selfPeer != Peer())) {
    LOG_STATE(
        "[BROAD][" << std::setw(15) << std::left
                   << m_selfPeer.GetPrintableIPAddress() << "]["
                   << DataConversion::Uint8VecToHexStr(m_hash).substr(0, 6)
                   << "] DONE");
  }
}

void P2PComm::ProcessSendJob(SendJob* job) {
  auto funcSendMsg = [job]() mutable -> void {
    job->DoSend();
    delete job;
  };
  m_SendPool.AddJob(funcSendMsg);
}

void P2PComm::ClearBroadcastHashAsync(
    const vector<unsigned char>& message_hash) {
  LOG_MARKER();
  lock_guard<mutex> guard(m_broadcastToRemoveMutex);
  m_broadcastToRemove.emplace_back(message_hash, chrono::system_clock::now());
}

/*static*/ void P2PComm::ProcessBroadCastMsg(
    std::vector<unsigned char>& message, const uint32_t messageLength,
    const Peer& from) {
  vector<unsigned char> msg_hash(message.begin() + HDR_LEN,
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
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
      sha256.Update(message, HDR_LEN + HASH_LEN,
                    message.size() - HDR_LEN - HASH_LEN);
      vector<unsigned char> this_msg_hash = sha256.Finalize();

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
    LOG_GENERAL(INFO, "Discarding duplicate broadcast message.");
    return;
  }

  unsigned char msg_type = 0xFF;
  unsigned char ins_type = 0xFF;
  if (messageLength - HASH_LEN > MessageOffset::INST) {
    msg_type = message.at(HDR_LEN + HASH_LEN + MessageOffset::TYPE);
    ins_type = message.at(HDR_LEN + HASH_LEN + MessageOffset::INST);
  }

  vector<Peer> broadcast_list =
      m_broadcast_list_retriever(msg_type, ins_type, from);

  if (broadcast_list.size() > 0) {
    p2p.RebroadcastMessage(broadcast_list, message, msg_hash);
  }

  p2p.ClearBroadcastHashAsync(msg_hash);

  LOG_STATE(
      "[BROAD][" << std::setw(15) << std::left << p2p.m_selfPeer << "]["
                 << DataConversion::Uint8VecToHexStr(msg_hash).substr(0, 6)
                 << "] RECV");

  // Move the shared_ptr message to raw pointer type
  pair<vector<unsigned char>, Peer>* raw_message =
      new pair<vector<unsigned char>, Peer>(
          vector<unsigned char>(message.begin() + HDR_LEN + HASH_LEN,
                                message.end()),
          from);
  LOG_GENERAL(INFO, "Size of broadcast message: " << message.size());

  // Queue the message
  m_dispatcher(raw_message);
}

/*static*/ void P2PComm::ProcessGossipMsg(std::vector<unsigned char>& message,
                                          Peer& from) {
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
    LOG_GENERAL(INFO, "Received Gossip of type - FORWARD from Peer :" << from);

    if (p2p.SpreadRumor(rumor_message)) {
      std::pair<vector<unsigned char>, Peer>* raw_message =
          new pair<vector<unsigned char>, Peer>(rumor_message, from);

      LOG_GENERAL(INFO, "Size of rumor message: " << rumor_message.size());

      // Queue the message
      m_dispatcher(raw_message);
    }
  } else if (p2p.m_rumorManager.RumorReceived((unsigned int)gossipMsgTyp,
                                              gossipMsgRound, rumor_message,
                                              from)) {
    std::pair<vector<unsigned char>, Peer>* raw_message =
        new pair<vector<unsigned char>, Peer>(rumor_message, from);

    LOG_GENERAL(INFO, "Size of rumor message: " << rumor_message.size());

    // Queue the message
    m_dispatcher(raw_message);
  }
}

void P2PComm::EventCallback(struct bufferevent* bev, short events,
                            [[gnu::unused]] void* ctx) {
  unique_ptr<struct bufferevent, decltype(&bufferevent_free)> socket_closer(
      bev, bufferevent_free);

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
  struct sockaddr_in cli_addr;
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
  vector<unsigned char> message(len);
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
  // 0x11 - start byte
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
  // <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0x22 - start byte (broadcast)
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of hash + message
  // <32-byte hash> <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0x33 - start byte (gossip)
  // 0xLL 0xLL 0xLL 0xLL - 4-byte length of message
  // 0x01 ~ 0x04 - Gossip_Message_Type
  // <4-byte Age> <message>

  // 0x01 ~ 0xFF - version, defined in constant file
  // 0x33 - start byte (report)
  // 0x00 0x00 0x00 0x01 - 4-byte length of message
  // 0x00

  // Check for minimum message size
  if (message.size() <= HDR_LEN) {
    LOG_GENERAL(WARNING, "Empty message received.");
    return;
  }

  const unsigned char version = message[0];
  const unsigned char startByte = message[1];

  // Check for version requirement
  if (version != (unsigned char)(MSG_VERSION & 0xFF)) {
    LOG_GENERAL(WARNING, "Header version wrong, received ["
                             << version - 0x00 << "] while expected ["
                             << MSG_VERSION << "].");
    return;
  }

  const uint32_t messageLength =
      (message[2] << 24) + (message[3] << 16) + (message[4] << 8) + message[5];

  // Check for length consistency
  if (messageLength != message.size() - HDR_LEN) {
    LOG_GENERAL(WARNING, "Incorrect message length.");
    return;
  }

  if (startByte == START_BYTE_BROADCAST) {
    LOG_PAYLOAD(INFO, "Incoming broadcast message from " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    if ((messageLength - HDR_LEN) <= HASH_LEN) {
      LOG_GENERAL(WARNING,
                  "Hash missing or empty broadcast message (messageLength = "
                      << messageLength << ")");
      return;
    }

    ProcessBroadCastMsg(message, messageLength, from);
  } else if (startByte == START_BYTE_NORMAL) {
    LOG_PAYLOAD(INFO, "Incoming normal message from " << from, message,
                Logger::MAX_BYTES_TO_DISPLAY);

    // Move the shared_ptr message to raw pointer type
    pair<vector<unsigned char>, Peer>* raw_message =
        new pair<vector<unsigned char>, Peer>(
            vector<unsigned char>(message.begin() + HDR_LEN, message.end()),
            from);
    LOG_GENERAL(INFO, "Size of normal message: " << message.size());

    // Queue the message
    m_dispatcher(raw_message);
  } else if (startByte == START_BYTE_GOSSIP) {
    if (messageLength <
        GOSSIP_MSGTYPE_LEN + GOSSIP_ROUND_LEN + GOSSIP_SNDR_LISTNR_PORT_LEN) {
      LOG_GENERAL(WARNING,
                  "Gossip Msg Type and/or Gossip Round and/or SNDR LISTNR "
                  "Port is missing (messageLength = "
                      << messageLength << ")");
      return;
    }

    ProcessGossipMsg(message, from);
  } else {
    // Unexpected start byte. Drop this message
    LOG_GENERAL(WARNING, "Incorrect start byte.");
  }
}

void P2PComm::AcceptConnectionCallback([[gnu::unused]] evconnlistener* listener,
                                       evutil_socket_t cli_sock,
                                       struct sockaddr* cli_addr,
                                       [[gnu::unused]] int socklen,
                                       [[gnu::unused]] void* arg) {
  Peer from(uint128_t(((struct sockaddr_in*)cli_addr)->sin_addr.s_addr),
            ((struct sockaddr_in*)cli_addr)->sin_port);

  LOG_GENERAL(DEBUG, "Incoming message from " << from);

  if (Blacklist::GetInstance().Exist(from.m_ipAddress)) {
    LOG_GENERAL(INFO, "The node "
                          << from
                          << " is in black list, block all message from it.");

    // Close the socket
    evutil_closesocket(cli_sock);

    return;
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

  bufferevent_setcb(bev, NULL, NULL, EventCallback, NULL);
  bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void P2PComm::StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
                               BroadcastListFunc broadcast_list_retriever) {
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

  int serv_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (serv_sock < 0) {
    LOG_GENERAL(WARNING, "Socket creation failed. Code = "
                             << errno << " Desc: " << std::strerror(errno));
    return;
  }

  int enable = 1;
  if (setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) <
      0) {
    LOG_GENERAL(WARNING, "Socket set option SO_REUSEADDR failed. Code = "
                             << errno << " Desc: " << std::strerror(errno));
    return;
  }

  m_dispatcher = dispatcher;
  m_broadcast_list_retriever = broadcast_list_retriever;

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(struct sockaddr_in));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(listen_port_host);
  serv_addr.sin_addr.s_addr = INADDR_ANY;

  // Create the listener
  struct event_base* base = event_base_new();
  struct evconnlistener* listener = evconnlistener_new_bind(
      base, AcceptConnectionCallback, nullptr,
      LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE, -1,
      (struct sockaddr*)&serv_addr, sizeof(struct sockaddr_in));
  event_base_dispatch(base);
  evconnlistener_free(listener);
  event_base_free(base);
}

void P2PComm::SendMessage(const vector<Peer>& peers,
                          const vector<unsigned char>& message,
                          const unsigned char& startByteType) {
  // LOG_MARKER();

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

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }
}

void P2PComm::SendMessage(const deque<Peer>& peers,
                          const vector<unsigned char>& message,
                          const unsigned char& startByteType) {
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

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }
}

void P2PComm::SendMessage(const Peer& peer,
                          const vector<unsigned char>& message,
                          const unsigned char& startByteType) {
  // LOG_MARKER();

  // Make job
  SendJob* job = new SendJobPeer;
  dynamic_cast<SendJobPeer*>(job)->m_peer = peer;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = startByteType;
  job->m_message = message;
  job->m_hash.clear();

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }
}

void P2PComm::SendBroadcastMessage(const vector<Peer>& peers,
                                   const vector<unsigned char>& message) {
  LOG_MARKER();

  if (peers.empty()) {
    return;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
  sha256.Update(message);

  // Make job
  SendJob* job = new SendJobPeers<vector<Peer>>;
  dynamic_cast<SendJobPeers<vector<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = START_BYTE_BROADCAST;
  job->m_message = message;
  job->m_hash = sha256.Finalize();

  vector<unsigned char> hashCopy(job->m_hash);

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }

  lock_guard<mutex> guard(m_broadcastHashesMutex);
  m_broadcastHashes.insert(hashCopy);
}

void P2PComm::SendBroadcastMessage(const deque<Peer>& peers,
                                   const vector<unsigned char>& message) {
  LOG_MARKER();

  if (peers.empty()) {
    return;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
  sha256.Update(message);

  // Make job
  SendJob* job = new SendJobPeers<deque<Peer>>;
  dynamic_cast<SendJobPeers<deque<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = m_selfPeer;
  job->m_startbyte = START_BYTE_BROADCAST;
  job->m_message = message;
  job->m_hash = sha256.Finalize();

  vector<unsigned char> hashCopy(job->m_hash);

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }

  lock_guard<mutex> guard(m_broadcastHashesMutex);
  m_broadcastHashes.insert(hashCopy);
}

void P2PComm::RebroadcastMessage(const vector<Peer>& peers,
                                 const vector<unsigned char>& message,
                                 const vector<unsigned char>& msg_hash) {
  LOG_MARKER();

  // Make job
  SendJob* job = new SendJobPeers<vector<Peer>>;
  dynamic_cast<SendJobPeers<vector<Peer>>*>(job)->m_peers = peers;
  job->m_selfPeer = Peer();
  job->m_startbyte = START_BYTE_BROADCAST;
  copy(message.begin() + HDR_LEN + HASH_LEN, message.end(),
       back_inserter(job->m_message));
  job->m_hash = msg_hash;

  // Queue job
  if (!m_sendQueue.bounded_push(job)) {
    LOG_GENERAL(WARNING, "SendQueue is full");
  }
}

void P2PComm::SendMessageNoQueue(const Peer& peer,
                                 const std::vector<unsigned char>& message,
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

bool P2PComm::SpreadRumor(const std::vector<unsigned char>& message) {
  LOG_MARKER();
  return m_rumorManager.AddRumor(message);
}

void P2PComm::SendRumorToForeignPeer(
    const Peer& foreignPeer, const std::vector<unsigned char>& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeer(foreignPeer, message);
}

void P2PComm::SendRumorToForeignPeers(
    const std::vector<Peer>& foreignPeers,
    const std::vector<unsigned char>& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeers(foreignPeers, message);
}

void P2PComm::SendRumorToForeignPeers(
    const std::deque<Peer>& foreignPeers,
    const std::vector<unsigned char>& message) {
  LOG_MARKER();
  m_rumorManager.SendRumorToForeignPeers(foreignPeers, message);
}

void P2PComm::SetSelfPeer(const Peer& self) { m_selfPeer = self; }

void P2PComm::InitializeRumorManager(const std::vector<Peer>& peers) {
  LOG_MARKER();

  m_rumorManager.StopRounds();
  if (m_rumorManager.Initialize(peers, m_selfPeer)) {
    if (peers.size() != 0) {
      m_rumorManager.StartRounds();
    }
    // Spread the buffered rumors
    m_rumorManager.SpreadBufferedRumors();
  }
}
