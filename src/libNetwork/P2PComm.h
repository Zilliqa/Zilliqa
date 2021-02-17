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

#ifndef ZILLIQA_SRC_LIBNETWORK_P2PCOMM_H_
#define ZILLIQA_SRC_LIBNETWORK_P2PCOMM_H_

#include <event2/util.h>
#include <boost/lockfree/queue.hpp>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <vector>

#include "Peer.h"
#include "RumorManager.h"
#include "common/BaseType.h"
#include "common/Constants.h"
#include "libUtils/Logger.h"
#include "libUtils/ThreadPool.h"

struct evconnlistener;

extern const unsigned char START_BYTE_NORMAL;
extern const unsigned char START_BYTE_GOSSIP;
extern const unsigned char START_BYTE_SEED_TO_SEED_REQUEST;
extern const unsigned char START_BYTE_SEED_TO_SEED_RESPONSE;

class SendJob {
 protected:
  static uint32_t writeMsg(const void* buf, int cli_sock, const Peer& from,
                           const uint32_t message_length);
  static bool SendMessageSocketCore(const Peer& peer, const bytes& message,
                                    unsigned char start_byte,
                                    const bytes& msg_hash);

 public:
  Peer m_selfPeer;
  unsigned char m_startbyte{};
  bytes m_message;
  bytes m_hash;
  bool m_allowSendToRelaxedBlacklist{};

  static void SendMessageCore(const Peer& peer, const bytes& message,
                              unsigned char startbyte, const bytes& hash);

  virtual ~SendJob() {}
  virtual void DoSend() = 0;
};

class SendJobPeer : public SendJob {
 public:
  Peer m_peer;
  void DoSend();
};

template <class T>
class SendJobPeers : public SendJob {
 public:
  T m_peers;
  void DoSend();
};

/// Provides network layer functionality.
class P2PComm {
  std::set<bytes> m_broadcastHashes;
  std::mutex m_broadcastHashesMutex;
  std::deque<
      std::pair<bytes, std::chrono::time_point<std::chrono::system_clock>>>
      m_broadcastToRemove;
  std::mutex m_broadcastToRemoveMutex;
  RumorManager m_rumorManager;

  const static uint32_t MAXPUMPMESSAGE = 128;

  struct event_base* m_base{};

  void ClearBroadcastHashAsync(const bytes& message_hash);
  void SendMsgToSeedNodeOnWire(const Peer& peer, const Peer& fromPeer,
                               const bytes& message,
                               const unsigned char& startByteType);
  void WriteMsgOnBufferEvent(struct bufferevent* bev, const bytes& message,
                             const unsigned char& startByteType);

  P2PComm();
  ~P2PComm();

  // Singleton should not implement these
  P2PComm(P2PComm const&) = delete;
  void operator=(P2PComm const&) = delete;

  using ShaMessage = bytes;
  static ShaMessage shaMessage(const bytes& message);

  Peer m_selfPeer;
  PairOfKey m_selfKey;

  static std::mutex m_mutexPeerConnectionCount;
  static std::map<uint128_t, uint16_t> m_peerConnectionCount;

  ThreadPool m_SendPool{MAXSENDMESSAGE, "SendPool"};

  boost::lockfree::queue<SendJob*> m_sendQueue;
  void ProcessSendJob(SendJob* job);

  static void ProcessBroadCastMsg(bytes& message, const Peer& from);
  static void ProcessGossipMsg(bytes& message, Peer& from);

  static void EventCallback(struct bufferevent* bev, short events, void* ctx);
  static void EventCbServerSeed(struct bufferevent* bev, short events,
                                [[gnu::unused]] void* ctx);
  static void EventCbClientSeed([[gnu::unused]] struct bufferevent* bev,
                                short events, void* ctx);
  static void ReadCallback(struct bufferevent* bev, void* ctx);
  static void ReadCbServerSeed(struct bufferevent* bev,
                               [[gnu::unused]] void* ctx);
  static void ReadCbClientSeed(struct bufferevent* bev, void* ctx);

  static void AcceptConnectionCallback(evconnlistener* listener,
                                       evutil_socket_t cli_sock,
                                       struct sockaddr* cli_addr, int socklen,
                                       void* arg);
  static void AcceptCbServerSeed(evconnlistener* listener,
                                 evutil_socket_t cli_sock,
                                 struct sockaddr* cli_addr, int socklen,
                                 void* arg);
  static void CloseAndFreeBufferEvent(struct bufferevent* bufev);
  static void CloseAndFreeBevP2PSeedConnServer(struct bufferevent* bufev);
  static void CloseAndFreeBevP2PSeedConnClient(struct bufferevent* bufev,
                                               void* ctx);

 public:
  static std::mutex m_mutexBufferEvent;
  static std::map<std::string, struct bufferevent*> m_bufferEventMap;
  /// Returns the singleton P2PComm instance.
  static P2PComm& GetInstance();

  using Dispatcher = std::function<void(
      std::pair<bytes, std::pair<Peer, const unsigned char>>*)>;

  using BroadcastListFunc = std::function<VectorOfPeer(
      unsigned char msg_type, unsigned char ins_type, const Peer&)>;

  void InitializeRumorManager(const VectorOfNode& peers,
                              const std::vector<PubKey>& fullNetworkKeys);

  void UpdatePeerInfoInRumorManager(const Peer& peers, const PubKey& pubKey);

  inline static bool IsHostHavingNetworkIssue();
  inline static bool IsNodeNotRunning();
  static void ClearPeerConnectionCount();
  static void RemoveBevFromMap(const Peer& peer);
  static void RemoveBevAndCloseP2PConnServer(const Peer& peer,
                                             const unsigned& startByteType);

 private:
  using SocketCloser = std::unique_ptr<int, void (*)(int*)>;
  static Dispatcher m_dispatcher;
  static BroadcastListFunc m_broadcast_list_retriever;

 public:
  /// Accept TCP connection for libevent usage
  static void ConnectionAccept(evconnlistener* listener,
                               evutil_socket_t cli_sock,
                               struct sockaddr* cli_addr, int socklen,
                               void* arg);

  /// Listens for incoming socket connections.
  void StartMessagePump(Dispatcher dispatcher);

  void EnableListener(uint32_t listenPort, bool startSeedNodeListener = false);
  // start event loop
  void EnableConnect();

  /// Multicasts message to specified list of peers.
  void SendMessage(const VectorOfPeer& peers, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  /// Multicasts message to specified list of peers.
  void SendMessage(const std::deque<Peer>& peers, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL,
                   const bool bAllowSendToRelaxedBlacklist = false);

  /// Sends normal message to specified peer.
  void SendMessage(const Peer& peer, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  // Overloadeded version of SendMessage for p2pseed comm.
  void SendMessage(const Peer& msgPeer, const Peer& fromPeer,
                   const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const VectorOfPeer& peers, const bytes& message);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const std::deque<Peer>& peers,
                            const bytes& message);

  void RebroadcastMessage(const VectorOfPeer& peers, const bytes& message,
                          const bytes& msg_hash);

  void SendMessageNoQueue(
      const Peer& peer, const bytes& message,
      const unsigned char& startByteType = START_BYTE_NORMAL);

  void SetSelfPeer(const Peer& self);

  void SetSelfKey(const PairOfKey& self);

  bool SpreadRumor(const bytes& message);

  bool SpreadForeignRumor(const bytes& message);

  void SendRumorToForeignPeer(const Peer& foreignPeer, const bytes& message);

  void SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                               const bytes& message);

  void SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                               const bytes& message);

  Signature SignMessage(const bytes& message);

  bool VerifyMessage(const bytes& message, const Signature& toverify,
                     const PubKey& pubKey);
};

#endif  // ZILLIQA_SRC_LIBNETWORK_P2PCOMM_H_
