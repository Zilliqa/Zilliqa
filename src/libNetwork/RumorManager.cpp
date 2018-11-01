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
      m_rumorIdRumorBimap(),
      m_tmpRumorHashSet(),
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
    std::unique_lock<std::mutex> guard(m_continueRoundMutex);
    m_continueRound = true;
    while (true) {
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
        return;
      }
    }
  })
      .detach();
}

void RumorManager::StopRounds() {
  LOG_MARKER();
  std::this_thread::sleep_for(std::chrono::milliseconds(ROUND_TIME_IN_MS * 2));
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    m_continueRound = false;
  }
  m_condStopRound.notify_all();
  std::this_thread::sleep_for(std::chrono::milliseconds(ROUND_TIME_IN_MS));
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
  m_rumorIdRumorBimap.clear();
  m_peerIdSet.clear();
  m_selfPeer = myself;
  m_tmpRumorHashSet.clear();

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

bool RumorManager::AddRumor(const RumorManager::RawBytes& message) {
  LOG_MARKER();
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    if (!m_continueRound) {  // Seems logical error. Round should have started.
      if (message.size() > 0) {
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(message);  // raw_message hash
        LOG_GENERAL(WARNING,
                    "Round is not running. So won't initiate the rumor. MyIP:"
                        << m_selfPeer << ". [Gossip_Message_Hash: "
                        << DataConversion::Uint8VecToHexStr(sha256.Finalize())
                               .substr(0, 6)
                        << " ]");
      }
    }
  }

  std::lock_guard<std::mutex> guard(m_mutex);  // critical section

  if (m_peerIdSet.size() == 0) {
    return true;
  }

  auto it = m_rumorIdRumorBimap.right.find(message);
  if (it == m_rumorIdRumorBimap.right.end()) {
    m_rumorIdRumorBimap.insert(
        RumorIdRumorBimap::value_type(++m_rumorIdGenerator, message));

    if (message.size() > 0) {
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
      sha256.Update(message);  // raw_message hash
      LOG_PAYLOAD(INFO,
                  "New Gossip message initiated by me ("
                      << m_selfPeer << "): [ RumorId: " << m_rumorIdGenerator
                      << ", Current Round: 0, Gossip_Message_Hash: "
                      << DataConversion::Uint8VecToHexStr(sha256.Finalize())
                             .substr(0, 6)
                      << " ]",
                  message, Logger::MAX_BYTES_TO_DISPLAY);
    }

    return m_rumorHolder->addRumor(m_rumorIdGenerator);
  } else {
    LOG_GENERAL(DEBUG, "This Rumor was already received. No problem.");
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
      if (message.size() >
          0)  // if we receive EMPTY_PULL/EMPTY_PUSH SHA will assert fail
      {
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(message);  // raw_message hash
        std::string hash = DataConversion::Uint8VecToHexStr(sha256.Finalize());
        {
          std::lock_guard<std::mutex> guard(m_mutex);
          // Is it old rumor message. If so then we have already dispatched it.
          // Don't do it again.
          auto it = m_rumorIdRumorBimap.right.find(message);
          if (it == m_rumorIdRumorBimap.right.end()) {
            // Now that round is not running , And i may receive same message
            // multiple times from my other peers. So avoid sending it to
            // dispatcher multiple times.
            if (m_tmpRumorHashSet.insert(hash).second) {
              LOG_GENERAL(
                  WARNING,
                  "Round is not running. Will accept the msg received from "
                      << from
                      << ", but will not "
                         "gossip it further. [Gossip_Message_Hash: "
                      << hash.substr(0, 6) << " ]");
              return true;
            } else {
              LOG_GENERAL(WARNING,
                          "Ignoring duplicate mesage.[Gossip_Message_Hash: "
                              << hash.substr(0, 6));
            }
          }
        }
      }
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
  } else {
    auto it = m_rumorIdRumorBimap.right.find(message);
    if (it == m_rumorIdRumorBimap.right.end()) {
      recvdRumorId = ++m_rumorIdGenerator;

      m_rumorIdRumorBimap.insert(
          RumorIdRumorBimap::value_type(recvdRumorId, message));

      if (message.size() > 0)  // if someone malaciously sends empty message of
                               // type PUSH/PULL),sha2 will assert fail
      {
        SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
        sha256.Update(message);  // raw_message hash
        LOG_PAYLOAD(INFO,
                    "New Gossip message received from Peer: "
                        << from << ". [ RumorId: " << recvdRumorId
                        << ", Current Round: " << round
                        << ", Gossip_Message_Hash: "
                        << DataConversion::Uint8VecToHexStr(sha256.Finalize())
                               .substr(0, 6)
                        << " ]",
                    message, Logger::MAX_BYTES_TO_DISPLAY);
      }

      toBeDispatched = true;
    } else  // already received , pass it on to member for state calculations
    {
      recvdRumorId = it->second;
      LOG_GENERAL(DEBUG, "Old Gossip message received from "
                             << from << ". [ RumorId: " << recvdRumorId
                             << ", Current Round: " << round);
    }
  }

  RRS::Message recvMsg(t, recvdRumorId, round);

  std::pair<int, std::vector<RRS::Message>> pullMsgs =
      m_rumorHolder->receivedMessage(recvMsg, p->second);

  LOG_GENERAL(DEBUG, "Sending " << pullMsgs.second.size() << " PULL Messages");

  SendMessages(from, pullMsgs.second);

  return toBeDispatched;
}

void RumorManager::SendMessages(const Peer& toPeer,
                                const std::vector<RRS::Message>& messages) {
  for (auto& k : messages) {
    // Add round and type to outgoing message
    RawBytes cmd = {(unsigned char)k.type()};
    unsigned int cur_offset = RRSMessageOffset::R_ROUNDS;

    Serializable::SetNumber<uint32_t>(cmd, cur_offset, k.rounds(),
                                      sizeof(uint32_t));

    cur_offset += sizeof(uint32_t);

    Serializable::SetNumber<uint32_t>(
        cmd, cur_offset, m_selfPeer.m_listenPortHost, sizeof(uint32_t));

    // Get the raw messages based on rumor ids.
    auto m = m_rumorIdRumorBimap.left.find(k.rumorId());
    if (m != m_rumorIdRumorBimap.left.end()) {
      // Add raw message to outgoing message
      cmd.insert(cmd.end(), m->second.begin(), m->second.end());
      LOG_GENERAL(DEBUG, "Sending Non Empty - Gossip Message: "
                             << k << " To Peer : " << toPeer);
    }

    // Send the message to peer .
    P2PComm::GetInstance().SendMessage(toPeer, cmd, START_BYTE_GOSSIP);
  }
}

// PUBLIC CONST METHODS
const RumorManager::RumorIdRumorBimap& RumorManager::rumors() const {
  return m_rumorIdRumorBimap;
}

void RumorManager::PrintStatistics() {
  LOG_MARKER();
  // we use hash of message to uniquely identify message across different nodes
  // in network.
  for (const auto& i : m_rumorHolder->rumorsMap()) {
    uint32_t rumorId = i.first;
    auto it = m_rumorIdRumorBimap.left.find(rumorId);
    if (it != m_rumorIdRumorBimap.left.end()) {
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha256;
      sha256.Update(it->second);  // raw_message hash
      std::vector<unsigned char> this_msg_hash = sha256.Finalize();

      const RRS::RumorStateMachine& state = i.second;
      LOG_GENERAL(
          INFO, "[ RumorId: " << rumorId << " , Gossip_Message_Hash: "
                              << DataConversion::Uint8VecToHexStr(this_msg_hash)
                                     .substr(0, 6)
                              << " ], " << state);
    }
  }
}