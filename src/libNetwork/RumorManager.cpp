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

#include "RumorManager.h"

#include <chrono>
#include <thread>

#include "P2PComm.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include "libUtils/HashUtils.h"

namespace {
RRS::Message::Type convertType(uint8_t type) {
  if (type > 0 && type < static_cast<int>(RRS::Message::Type::NUM_TYPES)) {
    return static_cast<RRS::Message::Type>(type);
  } else {
    return RRS::Message::Type::UNDEFINED;
  }
}

}  // anonymous namespace

// CONSTRUCTORS
RumorManager::RumorManager()
    : m_peerIdPeerBimap(),
      m_peerIdSet(),
      m_rumorIdHashBimap(),
      m_rumorHashRawMsgBimap(),
      m_selfPeer(),
      m_rumorIdGenerator(0),
      m_mutex(),
      m_continueRoundMutex(),
      m_continueRound(false),
      m_condStopRound() {}

RumorManager::~RumorManager() {}

// PRIVATE METHODS

void RumorManager::StartRounds() {
  LOG_MARKER();

  // To make sure we always have m_continueRound set at start of round.
  {
    std::unique_lock<std::mutex> guard(m_continueRoundMutex);
    m_continueRound = true;
  }

  std::thread([&]() {
    while (true) {
      std::unique_lock<std::mutex> guard(m_continueRoundMutex);
      m_continueRound = true;
      {  // critical section
        std::lock_guard<std::mutex> guard(m_mutex);
        std::pair<std::vector<int>, std::vector<RRS::Message>> result =
            m_rumorHolder->advanceRound();

        LOG_GENERAL(DEBUG, "Sending " << result.second.size()
                                      << " push messages to "
                                      << result.first.size() << " peers");

        // Get the corresponding Peer to which to send Push Messages if any.
        for (const auto& i : result.first) {
          auto l = m_peerIdPeerBimap.left.find(i);
          if (l != m_peerIdPeerBimap.left.end()) {
            SendMessages(l->second, result.second);
          }
        }
      }  // end critical section
      if (m_condStopRound.wait_for(guard,
                                   std::chrono::milliseconds(ROUND_TIME_IN_MS),
                                   [&] { return !m_continueRound; })) {
        LOG_GENERAL(INFO, "Stopping round now..");
        return;
      }
    }
  })
      .detach();
}

void RumorManager::StopRounds() {
  LOG_MARKER();
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    m_continueRound = false;
  }
  m_condStopRound.notify_all();
}

// PUBLIC METHODS
bool RumorManager::Initialize(const std::vector<Peer>& peers,
                              const Peer& myself) {
  LOG_MARKER();
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    if (m_continueRound) {
      // Seems logical error. Round should have been already Stopped.
      LOG_GENERAL(WARNING,
                  "Round is still running.. So won't re-initialize the "
                  "rumor manager.");
      return false;
    }
  }

  std::lock_guard<std::mutex> guard(m_mutex);  // critical section

  if (m_rumorIdGenerator) {
    PrintStatistics();
  }

  m_rumorIdGenerator = 0;
  m_peerIdPeerBimap.clear();
  m_rumorIdHashBimap.clear();
  m_peerIdSet.clear();
  m_selfPeer = myself;
  m_rumorHashRawMsgBimap.clear();

  int peerIdGenerator = 0;
  for (const auto& p : peers) {
    if (p.m_listenPortHost != 0) {
      ++peerIdGenerator;
      m_peerIdPeerBimap.insert(PeerIdPeerBiMap::value_type(peerIdGenerator, p));
      m_peerIdSet.insert(peerIdGenerator);
    }
  }

  // Now create the one and only RumorHolder
  if (GOSSIP_CUSTOM_ROUNDS_SETTINGS) {
    m_rumorHolder.reset(new RRS::RumorHolder(
        m_peerIdSet, MAX_ROUNDS_IN_BSTATE, MAX_ROUNDS_IN_CSTATE,
        MAX_TOTAL_ROUNDS, MAX_NEIGHBORS_PER_ROUND, 0));
  } else {
    m_rumorHolder.reset(new RRS::RumorHolder(m_peerIdSet, 0));
  }

  return true;
}

void RumorManager::SpreadBufferedRumors() {
  LOG_MARKER();
  if (m_continueRound) {
    for (const auto& i : m_bufferRawMsg) {
      AddRumor(i);
    }
    m_bufferRawMsg.clear();
  }
}

bool RumorManager::AddRumor(const RumorManager::RawBytes& message) {
  LOG_MARKER();
  if (message.size() > 0) {
    RawBytes hash = HashUtils::BytesToHash(message);

    {
      std::lock_guard<std::mutex> guard(m_continueRoundMutex);
      if (!m_continueRound) {
        LOG_GENERAL(WARNING,
                    "Round is not running. So won't initiate the rumor. "
                    "Instead will buffer it. MyIP:"
                        << m_selfPeer << ". [Gossip_Message_Hash: "
                        << DataConversion::Uint8VecToHexStr(hash).substr(0, 6)
                        << " ]");

        m_bufferRawMsg.push_back(message);
        return false;
      }
    }

    std::lock_guard<std::mutex> guard(m_mutex);  // critical section

    if (m_peerIdSet.empty()) {
      return true;
    }

    auto it = m_rumorIdHashBimap.right.find(hash);
    if (it == m_rumorIdHashBimap.right.end()) {
      m_rumorIdHashBimap.insert(
          RumorIdRumorBimap::value_type(++m_rumorIdGenerator, hash));

      m_rumorHashRawMsgBimap.insert(
          RumorHashRumorBiMap::value_type(hash, message));

      LOG_PAYLOAD(INFO,
                  "New Gossip message initiated by me ("
                      << m_selfPeer << "): [ RumorId: " << m_rumorIdGenerator
                      << ", Current Round: 0, Gossip_Message_Hash: "
                      << DataConversion::Uint8VecToHexStr(hash).substr(0, 6)
                      << " ]",
                  message, Logger::MAX_BYTES_TO_DISPLAY);

      return m_rumorHolder->addRumor(m_rumorIdGenerator);
    } else {
      LOG_GENERAL(DEBUG, "This Rumor was already received. No problem.");
    }
  }

  return false;
}

RumorManager::RawBytes RumorManager::GenerateGossipForwardMessage(
    const RawBytes& message) {
  // Add round and type to outgoing message
  RawBytes cmd = {(unsigned char)RRS::Message::Type::FORWARD};
  unsigned int cur_offset = RRSMessageOffset::R_ROUNDS;

  Serializable::SetNumber<uint32_t>(cmd, cur_offset, 0, sizeof(uint32_t));

  cur_offset += sizeof(uint32_t);

  Serializable::SetNumber<uint32_t>(
      cmd, cur_offset, m_selfPeer.m_listenPortHost, sizeof(uint32_t));

  cmd.insert(cmd.end(), message.begin(), message.end());

  return cmd;
}

void RumorManager::SendRumorToForeignPeers(
    const std::deque<Peer>& toForeignPeers, const RawBytes& message) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO,
              "New message to be gossiped is forwarded to Foreign Peers by me"
                  << m_selfPeer,
              message, Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Foreign Peers: ");
  for (auto& i : toForeignPeers) {
    LOG_GENERAL(INFO, "             " << i);
  }

  RawBytes cmd = GenerateGossipForwardMessage(message);

  P2PComm::GetInstance().SendMessage(toForeignPeers, cmd, START_BYTE_GOSSIP);
}

void RumorManager::SendRumorToForeignPeers(
    const std::vector<Peer>& toForeignPeers, const RawBytes& message) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO,
              "New message to be gossiped is forwarded to Foreign Peers by me"
                  << m_selfPeer,
              message, Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Foreign Peers: ");
  for (auto& i : toForeignPeers) {
    LOG_GENERAL(INFO, "             " << i);
  }

  RawBytes cmd = GenerateGossipForwardMessage(message);

  P2PComm::GetInstance().SendMessage(toForeignPeers, cmd, START_BYTE_GOSSIP);
}

void RumorManager::SendRumorToForeignPeer(const Peer& toForeignPeer,
                                          const RawBytes& message) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO,
              "New message to be gossiped forwarded to Foreign Peer:"
                  << toForeignPeer << "by me:" << m_selfPeer,
              message, Logger::MAX_BYTES_TO_DISPLAY);

  RawBytes cmd = GenerateGossipForwardMessage(message);

  P2PComm::GetInstance().SendMessage(toForeignPeer, cmd, START_BYTE_GOSSIP);
}

bool RumorManager::RumorReceived(uint8_t type, int32_t round,
                                 const RawBytes& message, const Peer& from) {
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    if (!m_continueRound) {
      LOG_GENERAL(WARNING, "Round is not running. Ignoring message!!")
      return false;
    }
  }

  std::lock_guard<std::mutex> guard(m_mutex);

  auto p = m_peerIdPeerBimap.right.find(from);
  if (p == m_peerIdPeerBimap.right.end()) {
    // I dont know this peer, missing in my peerlist.
    LOG_GENERAL(DEBUG, "Received Rumor from peer : "
                           << from << " which does not exist in my peerlist.");
    return false;
  }

  int64_t recvdRumorId = -1;
  RRS::Message::Type t = convertType(type);
  bool toBeDispatched = false;
  if (RRS::Message::Type::EMPTY_PUSH == t ||
      RRS::Message::Type::EMPTY_PULL == t) {
    /* Don't add it to local RumorMap because it's not the rumor itself */
    LOG_GENERAL(DEBUG, "Received empty message of type: "
                           << RRS::Message::s_enumKeyToString[t]);
  } else if (RRS::Message::Type::LAZY_PUSH == t ||
             RRS::Message::Type::LAZY_PULL == t) {
    auto it = m_rumorIdHashBimap.right.find(message);
    if (it == m_rumorIdHashBimap.right.end()) {
      recvdRumorId = ++m_rumorIdGenerator;

      m_rumorIdHashBimap.insert(
          RumorIdRumorBimap::value_type(recvdRumorId, message));

      // Now that's the new hash message. So we dont have the real message.
      // So lets ask the sender for it.
      RRS::Message pullMsg(RRS::Message::Type::PULL, recvdRumorId, -1);
      SendMessage(from, pullMsg);
    } else {
      recvdRumorId = it->second;
      LOG_GENERAL(DEBUG, "Old Gossip hash message received from "
                             << from << ". [ RumorId: " << recvdRumorId
                             << ", Current Round: " << round);
      // check if we have received the real message for this old rumor.
      auto it = m_rumorHashRawMsgBimap.left.find(message);
      if (it == m_rumorHashRawMsgBimap.left.end()) {
        // didn't receive real message (PUSH) yet :( Lets ask this peer.
        RRS::Message pullMsg(RRS::Message::Type::PULL, recvdRumorId, -1);
        SendMessage(from, pullMsg);
      }
    }
  } else if (RRS::Message::Type::PULL == t) {
    // Now that sender wants the real message, lets send it to him.
    auto it1 = m_rumorHashRawMsgBimap.left.find(message);
    if (it1 != m_rumorHashRawMsgBimap.left.end()) {
      auto it2 = m_rumorIdHashBimap.right.find(message);
      if (it2 != m_rumorIdHashBimap.right.end()) {
        recvdRumorId = it2->second;
        RRS::Message pushMsg(RRS::Message::Type::PUSH, recvdRumorId, -1);
        SendMessage(from, pushMsg);
      }
    } else  // I dont have it as of now. Add this peer to subscriber list for
            // this hash message.
    {
      auto it2 = m_hashesSubscriberMap.find(message);
      if (it2 == m_hashesSubscriberMap.end()) {
        m_hashesSubscriberMap.insert(
            RumorHashesPeersMap::value_type(message, std::set<Peer>()));
      }
      m_hashesSubscriberMap[message].insert(from);
    }
    return false;
  } else if (RRS::Message::Type::PUSH == t) {
    // I got it from my peer for what i asked him
    RawBytes hash;
    if (message.size() >
        0)  // if someone malaciously sends empty message, sha2 will assert fail
    {
      hash = HashUtils::BytesToHash(message);

      auto it1 = m_rumorIdHashBimap.right.find(hash);
      if (it1 != m_rumorIdHashBimap.right.end()) {
        recvdRumorId = it1->second;
      } else {
        // I have not asked for this raw message.. so ignoring
        return false;
      }

      // toBeDispatched
      if (m_rumorHashRawMsgBimap
              .insert(RumorHashRumorBiMap::value_type(hash, message))
              .second) {
        LOG_PAYLOAD(INFO,
                    "New Gossip Raw message received from Peer: "
                        << from << ", Gossip_Message_Hash: "
                        << DataConversion::Uint8VecToHexStr(hash).substr(0, 6)
                        << " ]",
                    message, Logger::MAX_BYTES_TO_DISPLAY);
        toBeDispatched = true;
      } else {
        LOG_PAYLOAD(DEBUG,
                    "Old Gossip Raw message received from Peer: "
                        << from << ", Gossip_Message_Hash: "
                        << DataConversion::Uint8VecToHexStr(hash).substr(0, 6)
                        << " ]",
                    message, Logger::MAX_BYTES_TO_DISPLAY);
      }

      // Do i have any peers subscribed with me for this hash.
      auto it2 = m_hashesSubscriberMap.find(hash);
      if (it2 != m_hashesSubscriberMap.end()) {
        // Send PUSH
        LOG_GENERAL(
            DEBUG,
            "Sending Gossip Raw Message to subscribers of Gossip_Message_Hash: "
                << DataConversion::Uint8VecToHexStr(hash).substr(0, 6));
        for (auto& p : it2->second) {
          // avoid un-neccessarily sending again back to sender itself
          if (p == from) {
            continue;
          }
          RRS::Message pushMsg(RRS::Message::Type::PUSH, recvdRumorId, -1);
          SendMessage(p, pushMsg);
        }
        m_hashesSubscriberMap.erase(hash);
      }
    }
    return toBeDispatched;
  } else {
    LOG_GENERAL(WARNING, "Unknown message type received");
    return false;
  }

  RRS::Message recvMsg(t, recvdRumorId, round);

  std::pair<int, std::vector<RRS::Message>> pullMsgs =
      m_rumorHolder->receivedMessage(recvMsg, p->second);

  LOG_GENERAL(DEBUG, "Sending " << pullMsgs.second.size()
                                << " EMPTY_PULL or LAZY_PULL Messages");

  SendMessages(from, pullMsgs.second);

  return toBeDispatched;
}

void RumorManager::SendMessage(const Peer& toPeer,
                               const RRS::Message& message) {
  // Add round and type to outgoing message
  RRS::Message::Type t = message.type();
  RawBytes cmd = {(unsigned char)t};
  unsigned int cur_offset = RRSMessageOffset::R_ROUNDS;

  Serializable::SetNumber<uint32_t>(cmd, cur_offset, message.rounds(),
                                    sizeof(uint32_t));

  cur_offset += sizeof(uint32_t);

  Serializable::SetNumber<uint32_t>(
      cmd, cur_offset, m_selfPeer.m_listenPortHost, sizeof(uint32_t));

  if (!(RRS::Message::Type::EMPTY_PUSH == t ||
        RRS::Message::Type::EMPTY_PULL == t)) {
    // Get the hash messages based on rumor id.
    auto it1 = m_rumorIdHashBimap.left.find(message.rumorId());
    if (it1 != m_rumorIdHashBimap.left.end()) {
      if (RRS::Message::Type::PUSH == t) {
        // Get the raw message based on hash
        auto it2 = m_rumorHashRawMsgBimap.left.find(it1->second);
        if (it2 != m_rumorHashRawMsgBimap.left.end()) {
          // Add raw message to outgoing message
          cmd.insert(cmd.end(), it2->second.begin(), it2->second.end());
          LOG_GENERAL(
              INFO,
              "Sending Gossip Raw Message of Gossip_Message_Hash : ["
                  << DataConversion::Uint8VecToHexStr(it1->second).substr(0, 6)
                  << "] To Peer : " << toPeer);
        } else {
          // Nothing to send.
          return;
        }
      } else if (RRS::Message::Type::LAZY_PUSH == t ||
                 RRS::Message::Type::LAZY_PULL == t ||
                 RRS::Message::Type::PULL == t) {
        // Add hash message to outgoing message for types
        // LAZY_PULL/LAZY_PUSH/PULL
        cmd.insert(cmd.end(), it1->second.begin(), it1->second.end());
        LOG_GENERAL(DEBUG, "Sending Gossip Hash Message: "
                               << message << " To Peer : " << toPeer);
      } else {
        return;
      }
    }
  }

  // Send the message to peer .
  if (SIMULATED_NETWORK_DELAY_IN_MS > 0) {
    std::this_thread::sleep_for(
        std::chrono::milliseconds(SIMULATED_NETWORK_DELAY_IN_MS));
  }
  P2PComm::GetInstance().SendMessage(toPeer, cmd, START_BYTE_GOSSIP);
}

void RumorManager::SendMessages(const Peer& toPeer,
                                const std::vector<RRS::Message>& messages) {
  for (auto& k : messages) {
    SendMessage(toPeer, k);
  }
}

// PUBLIC CONST METHODS
const RumorManager::RumorIdRumorBimap& RumorManager::rumors() const {
  return m_rumorIdHashBimap;
}

void RumorManager::PrintStatistics() {
  LOG_MARKER();
  // we use hash of message to uniquely identify message across different nodes
  // in network.
  for (const auto& i : m_rumorHolder->rumorsMap()) {
    uint32_t rumorId = i.first;
    auto it = m_rumorIdHashBimap.left.find(rumorId);
    if (it != m_rumorIdHashBimap.left.end()) {
      bytes this_msg_hash = HashUtils::BytesToHash(it->second);
      const RRS::RumorStateMachine& state = i.second;
      LOG_GENERAL(
          INFO, "[ RumorId: " << rumorId << " , Gossip_Message_Hash: "
                              << DataConversion::Uint8VecToHexStr(this_msg_hash)
                                     .substr(0, 6)
                              << " ], " << state);
    }
  }
}