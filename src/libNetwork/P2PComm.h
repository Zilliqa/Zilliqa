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

#ifndef __P2PCOMM_H__
#define __P2PCOMM_H__

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

class SendJob {
 protected:
  static uint32_t writeMsg(const void* buf, int cli_sock, const Peer& from,
                           const uint32_t message_length);
  static bool SendMessageSocketCore(const Peer& peer, const bytes& message,
                                    unsigned char start_byte,
                                    const bytes& msg_hash);

 public:
  Peer m_selfPeer;
  unsigned char m_startbyte;
  bytes m_message;
  bytes m_hash;

  static void SendMessageCore(const Peer& peer, const bytes message,
                              unsigned char startbyte, const bytes hash);

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

  void ClearBroadcastHashAsync(const bytes& message_hash);

  P2PComm();
  ~P2PComm();

  // Singleton should not implement these
  P2PComm(P2PComm const&) = delete;
  void operator=(P2PComm const&) = delete;

  using ShaMessage = bytes;
  static ShaMessage shaMessage(const bytes& message);

  Peer m_selfPeer;
  std::pair<PrivKey, PubKey> m_selfKey;

  ThreadPool m_SendPool{MAXMESSAGE, "SendPool"};

  boost::lockfree::queue<SendJob*> m_sendQueue;
  void ProcessSendJob(SendJob* job);

  static void ProcessBroadCastMsg(bytes& message, const uint32_t messageLength,
                                  const Peer& from);
  static void ProcessGossipMsg(bytes& message, Peer& from);

  static void EventCallback(struct bufferevent* bev, short events, void* ctx);
  static void AcceptConnectionCallback(evconnlistener* listener,
                                       evutil_socket_t cli_sock,
                                       struct sockaddr* cli_addr, int socklen,
                                       void* arg);

 public:
  /// Returns the singleton P2PComm instance.
  static P2PComm& GetInstance();

  using Dispatcher = std::function<void(std::pair<bytes, Peer>*)>;

  using BroadcastListFunc = std::function<std::vector<Peer>(
      unsigned char msg_type, unsigned char ins_type, const Peer&)>;

  void InitializeRumorManager(const std::vector<std::pair<PubKey, Peer>>& peers,
                              const std::vector<PubKey>& fullNetworkKeys);
  inline static bool IsHostHavingNetworkIssue();

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
  void StartMessagePump(uint32_t listen_port_host, Dispatcher dispatcher,
                        BroadcastListFunc broadcast_list_retriever);

  /// Multicasts message to specified list of peers.
  void SendMessage(const std::vector<Peer>& peers, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  /// Multicasts message to specified list of peers.
  void SendMessage(const std::deque<Peer>& peers, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  /// Sends normal message to specified peer.
  void SendMessage(const Peer& peer, const bytes& message,
                   const unsigned char& startByteType = START_BYTE_NORMAL);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const std::vector<Peer>& peers,
                            const bytes& message);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const std::deque<Peer>& peers,
                            const bytes& message);

  void RebroadcastMessage(const std::vector<Peer>& peers, const bytes& message,
                          const bytes& msg_hash);

  void SendMessageNoQueue(
      const Peer& peer, const bytes& message,
      const unsigned char& startByteType = START_BYTE_NORMAL);

  void SetSelfPeer(const Peer& self);

  void SetSelfKey(const std::pair<PrivKey, PubKey>& self);

  bool SpreadRumor(const bytes& message);

  bool SpreadForeignRumor(const bytes& message);

  void SendRumorToForeignPeer(const Peer& foreignPeer, const bytes& message);

  void SendRumorToForeignPeers(const std::vector<Peer>& foreignPeers,
                               const bytes& message);

  void SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                               const bytes& message);

  Signature SignMessage(const bytes& message);

  bool VerifyMessage(const bytes& message, const Signature& toverify,
                     const PubKey& pubKey);
};

#endif  // __P2PCOMM_H__
