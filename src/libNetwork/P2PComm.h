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
#include <deque>
#include <functional>
#include <mutex>
#include <set>

#include "P2PMessage.h"
#include "RumorManager.h"
#include "common/Constants.h"

struct evconnlistener;

namespace zil::p2p {
class SendJobs;
}

/// Provides network layer functionality.
class P2PComm {
  std::set<zbytes> m_broadcastHashes;
  std::mutex m_broadcastHashesMutex;
  std::deque<
      std::pair<zbytes, std::chrono::time_point<std::chrono::system_clock>>>
      m_broadcastToRemove;
  std::mutex m_broadcastToRemoveMutex;
  RumorManager m_rumorManager;

  struct event_base* m_base{};

  void ClearBroadcastHashAsync(const zbytes& message_hash);
  void SendMsgToSeedNodeOnWire(const Peer& peer, const Peer& fromPeer,
                               const zbytes& message,
                               const unsigned char& startByteType);
  void WriteMsgOnBufferEvent(struct bufferevent* bev, const zbytes& message,
                             const unsigned char& startByteType);

  P2PComm();
  ~P2PComm();

  // Singleton should not implement these
  P2PComm(P2PComm const&) = delete;
  void operator=(P2PComm const&) = delete;

  using ShaMessage = zbytes;
  static ShaMessage shaMessage(const zbytes& message);

  Peer m_selfPeer;
  PairOfKey m_selfKey;

  static std::mutex m_mutexPeerConnectionCount;
  static std::map<uint128_t, uint16_t> m_peerConnectionCount;

  std::shared_ptr<zil::p2p::SendJobs> m_sendJobs;

  static void ProcessBroadCastMsg(zbytes& message, zbytes& hash,
                                  const Peer& from, std::string& traceInfo);
  static void ProcessGossipMsg(zbytes& message, Peer& from,
                               std::string& traceInfo);

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

  using BroadcastListFunc = std::function<VectorOfPeer(
      unsigned char msg_type, unsigned char ins_type, const Peer&)>;

  void InitializeRumorManager(const VectorOfNode& peers,
                              const std::vector<PubKey>& fullNetworkKeys);

  void UpdatePeerInfoInRumorManager(const Peer& peers, const PubKey& pubKey);

  static void ClearPeerConnectionCount();
  static void RemoveBevFromMap(const Peer& peer);
  static void RemoveBevAndCloseP2PConnServer(const Peer& peer,
                                             const unsigned& startByteType);

 private:
  static zil::p2p::Dispatcher m_dispatcher;
  static BroadcastListFunc m_broadcast_list_retriever;

 public:
  /// Listens for incoming socket connections.
  void StartMessagePump(zil::p2p::Dispatcher dispatcher);

  void EnableListener(uint32_t listenPort, bool startSeedNodeListener = false);
  // start event loop
  void EnableConnect();

  /// Multicasts message to specified list of peers.
  void SendMessage(const VectorOfPeer& peers, const zbytes& message,
                   unsigned char startByteType = zil::p2p::START_BYTE_NORMAL,
                   bool inject_trace_context = false);

  /// Multicasts message to specified list of peers.
  void SendMessage(const std::deque<Peer>& peers, const zbytes& message,
                   unsigned char startByteType = zil::p2p::START_BYTE_NORMAL,
                   bool inject_trace_context = false,
                   bool bAllowSendToRelaxedBlacklist = false);

  /// Sends normal message to specified peer.
  void SendMessage(const Peer& peer, const zbytes& message,
                   unsigned char startByteType = zil::p2p::START_BYTE_NORMAL,
                   bool inject_trace_context = false);

  // Overloadeded version of SendMessage for p2pseed comm.
  void SendMessage(const Peer& msgPeer, const Peer& fromPeer,
                   const zbytes& message,
                   unsigned char startByteType = zil::p2p::START_BYTE_NORMAL,
                   bool inject_trace_context = false);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const VectorOfPeer& peers, const zbytes& message,
                            bool inject_trace_context = false);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const std::deque<Peer>& peers,
                            const zbytes& message,
                            bool inject_trace_context = false);

  void SendMessageNoQueue(
      const Peer& peer, const zbytes& message,
      unsigned char startByteType = zil::p2p::START_BYTE_NORMAL);

  void SetSelfPeer(const Peer& self);

  void SetSelfKey(const PairOfKey& self);

  bool SpreadRumor(const zbytes& message);

  bool SpreadForeignRumor(const zbytes& message);

  void SendRumorToForeignPeer(const Peer& foreignPeer, const zbytes& message);

  void SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                               const zbytes& message);

  void SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                               const zbytes& message);

  Signature SignMessage(const zbytes& message);

  bool VerifyMessage(const zbytes& message, const Signature& toverify,
                     const PubKey& pubKey);
};

#endif  // ZILLIQA_SRC_LIBNETWORK_P2PCOMM_H_
