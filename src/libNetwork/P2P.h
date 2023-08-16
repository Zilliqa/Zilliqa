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

#ifndef ZILLIQA_SRC_LIBNETWORK_P2P_H_
#define ZILLIQA_SRC_LIBNETWORK_P2P_H_

#include <chrono>
#include <condition_variable>
#include <optional>
#include <thread>
#include <set>

#include "P2PMessage.h"
#include "ShardStruct.h"

namespace boost::asio {
class io_context;
}

class RumorManager;

namespace zil::p2p {

class SendJobs;
class P2PServer;

/// To be deprecated - singleton usage of P2P
class P2P& GetInstance();

/// Verifies a Schnorr signature of message
bool VerifyMessage(const zbytes& message, const Signature& toverify,
                   const PubKey& pubKey);

/// P2P communicaions
class P2P {
 public:
  P2P();

  ~P2P();

  void SetSelfIdentity(const Peer& selfPeer, const PairOfKey& selfKeys);

  /// Signs a message by Schnorr
  std::optional<Signature> SignMessage(const zbytes& message);

  /// Starts P2P server. This fn may throw on failures or invalid args
  void StartServer(boost::asio::io_context& asio, uint16_t port,
                   uint16_t additionalPort, Dispatcher dispatcher);

  void InitializeRumorManager(const VectorOfNode& peers,
                              const std::vector<PubKey>& fullNetworkKeys);

  /// Multicasts message to specified list of peers.
  void SendMessage(const VectorOfPeer& peers, const zbytes& message,
                   unsigned char startByteType = START_BYTE_NORMAL,
                   bool inject_trace_context = false);

  /// Multicasts message to specified list of peers.
  void SendMessage(const std::deque<Peer>& peers, const zbytes& message,
                   unsigned char startByteType = START_BYTE_NORMAL,
                   bool inject_trace_context = false,
                   bool bAllowSendToRelaxedBlacklist = false);

  /// Sends normal message to specified peer.
  void SendMessage(const Peer& peer, const zbytes& message,
                   unsigned char startByteType = START_BYTE_NORMAL,
                   bool inject_trace_context = false);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const VectorOfPeer& peers, const zbytes& message,
                            bool inject_trace_context = false);

  /// Multicasts message of type=broadcast to specified list of peers.
  void SendBroadcastMessage(const std::deque<Peer>& peers,
                            const zbytes& message,
                            bool inject_trace_context = false);

  /// Special case for cmd line utilities only - blocking
  void SendMessageNoQueue(const Peer& peer, const zbytes& message,
                          unsigned char startByteType = START_BYTE_NORMAL);

  bool SpreadRumor(const zbytes& message);

  bool SpreadForeignRumor(const zbytes& message);

  void SendRumorToForeignPeer(const Peer& foreignPeer, const zbytes& message);

  void SendRumorToForeignPeers(const VectorOfPeer& foreignPeers,
                               const zbytes& message);

  void SendRumorToForeignPeers(const std::deque<Peer>& foreignPeers,
                               const zbytes& message);

  void UpdatePeerInfoInRumorManager(const Peer& peer, const PubKey& pubKey);

 private:
  /// Broadcast cleanup thread periodic job
  void BroadcastCleanupJob();

  /// Dispatches P2P message, returns true to keep the connection alive
  bool DispatchMessage(const Peer& from, ReadMessageResult& readResult);

  void ProcessBroadCastMsg(zbytes& message, zbytes& hash, const Peer& from,
                           std::string& traceInfo);

  void ProcessGossipMsg(zbytes& message, const Peer& from,
                        std::string& traceInfo);

  std::optional<Peer> m_selfPeer;
  std::optional<PairOfKey> m_selfKey;
  Dispatcher m_dispatcher;
  std::shared_ptr<SendJobs> m_sendJobs;
  std::shared_ptr<P2PServer> m_server;
  std::shared_ptr<P2PServer> m_additionalServer;
  std::set<zbytes> m_broadcastHashes;
  std::deque<
      std::pair<zbytes, std::chrono::time_point<std::chrono::steady_clock>>>
      m_broadcastToRemove;
  std::shared_ptr<RumorManager> m_rumorManager;
  std::thread m_broadcastCleanupThread;
  std::mutex m_mutex;
  std::condition_variable m_condition;
  bool m_stopped = false;
};

}  // namespace zil::p2p

#endif  // ZILLIQA_SRC_LIBNETWORK_P2P_H_
