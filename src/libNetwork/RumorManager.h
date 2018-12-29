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

#ifndef __RUMORMANAGER_H__
#define __RUMORMANAGER_H__

#include <boost/bimap.hpp>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <unordered_map>

#include "Peer.h"
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

  // MEMBERS
  std::shared_ptr<RRS::RumorHolder> m_rumorHolder;
  PeerIdPeerBiMap m_peerIdPeerBimap;
  std::unordered_set<int> m_peerIdSet;
  RumorIdRumorBimap m_rumorIdHashBimap;
  RumorHashRumorBiMap m_rumorHashRawMsgBimap;
  RumorHashesPeersMap m_hashesSubscriberMap;
  Peer m_selfPeer;
  std::vector<RawBytes> m_bufferRawMsg;
  RumorRawMsgTimestampDeque m_rumorRawMsgTimestamp;

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
  bool Initialize(const std::vector<Peer>& peers, const Peer& myself);

  bool AddRumor(const RawBytes& message);

  void SpreadBufferedRumors();

  bool RumorReceived(uint8_t type, int32_t round, const RawBytes& message,
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

  // CONST METHODS
  const RumorIdRumorBimap& rumors() const;
};

#endif  //__RUMORMANAGER_H__