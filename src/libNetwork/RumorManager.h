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

#ifndef __RUMORMANAGER_H__
#define __RUMORMANAGER_H__

#include <boost/bimap.hpp>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>

#include "Peer.h"
#include "ShardStruct.h"
#include "libCrypto/Schnorr.h"
#include "libRumorSpreading/RumorHolder.h"

enum RRSMessageOffset : unsigned int {
  R_TYPE = 0,
  R_ROUNDS = 1,
};

const unsigned int RETRY_COUNT = 3;

class RumorManager {
 public:
  // TYPES
  typedef bytes RawBytes;

 private:
  // TYPES
  typedef boost::bimap<int, Peer> PeerIdPeerBiMap;
  typedef boost::bimap<int, RawBytes> RumorIdRumorBimap;
  typedef boost::bimap<RawBytes, RawBytes> RumorHashRumorBiMap;
  typedef std::map<RawBytes, std::set<Peer>> RumorHashesPeersMap;
  typedef std::deque<std::pair<RumorHashRumorBiMap::iterator,
                               std::chrono::high_resolution_clock::time_point>>
      RumorRawMsgTimestampDeque;
  typedef boost::bimap<PubKey, Peer> PubKeyPeerBiMap;

  // MEMBERS
  std::shared_ptr<RRS::RumorHolder> m_rumorHolder;
  PeerIdPeerBiMap m_peerIdPeerBimap;
  PubKeyPeerBiMap m_pubKeyPeerBiMap;
  std::unordered_set<int> m_peerIdSet;
  RumorIdRumorBimap m_rumorIdHashBimap;
  RumorHashRumorBiMap m_rumorHashRawMsgBimap;
  RumorHashesPeersMap m_hashesSubscriberMap;
  Peer m_selfPeer;
  PairOfKey m_selfKey;
  std::vector<RawBytes> m_bufferRawMsg;
  RumorRawMsgTimestampDeque m_rumorRawMsgTimestamp;
  std::vector<PubKey> m_fullNetworkKeys;

  int64_t m_rumorIdGenerator;
  std::mutex m_mutex;
  std::mutex m_continueRoundMutex;
  std::atomic<bool> m_continueRound;
  std::condition_variable m_condStopRound;

  int32_t m_rawMessageExpiryInMs;

  void SendMessages(const Peer& toPeer,
                    const std::vector<RRS::Message>& messages);

  void SendMessage(const Peer& toPeer, const RRS::Message& message);

  RawBytes GenerateGossipForwardMessage(const RawBytes& message);

 public:
  // CREATORS
  RumorManager();
  ~RumorManager();

  // METHODS
  bool Initialize(const VectorOfNode& peers, const Peer& myself,
                  const PairOfKey& myKeys,
                  const std::vector<PubKey>& fullNetworkKeys);

  bool AddRumor(const RawBytes& message);

  bool AddForeignRumor(const RawBytes& message);

  void SpreadBufferedRumors();

  std::pair<bool, RawBytes> RumorReceived(uint8_t type, int32_t round,
                                          const RawBytes& message,
                                          const Peer& from);

  void StartRounds();
  void StopRounds();

  void SendRumorToForeignPeer(const Peer& toForeignPeer,
                              const RawBytes& message);

  void SendRumorToForeignPeers(const std::deque<Peer>& toForeignPeers,
                               const RawBytes& message);

  void SendRumorToForeignPeers(const std::vector<Peer>& toForeignPeers,
                               const RawBytes& message);

  void PrintStatistics();

  void CleanUp();

  std::pair<bool, RumorManager::RawBytes> VerifyMessage(
      const RawBytes& message, const RRS::Message::Type& t, const Peer& from);

  void AppendKeyAndSignature(RawBytes& result, const RawBytes& messageToSig);
  // CONST METHODS
  const RumorIdRumorBimap& rumors() const;
};

#endif  //__RUMORMANAGER_H__
