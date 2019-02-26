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

#include "RumorManager.h"

#include <chrono>
#include <string>
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
      m_selfKey(),
      m_rumorRawMsgTimestamp(),
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
    unsigned int rounds = 0;
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
        if (++rounds % KEEP_RAWMSG_FROM_LAST_N_ROUNDS == 0) {
          CleanUp();
          rounds = 0;
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
bool RumorManager::Initialize(const VectorOfNode& peers, const Peer& myself,
                              const PairOfKey& myKeys,
                              const std::vector<PubKey>& fullNetworkKeys) {
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
  m_selfKey = myKeys;
  m_rumorHashRawMsgBimap.clear();
  m_rumorRawMsgTimestamp.clear();
  m_fullNetworkKeys.clear();
  m_pubKeyPeerBiMap.clear();
  m_hashesSubscriberMap.clear();

  int peerIdGenerator = 0;
  for (const auto& p : peers) {
    if (p.second.m_listenPortHost != 0) {
      ++peerIdGenerator;
      m_peerIdPeerBimap.insert(
          PeerIdPeerBiMap::value_type(peerIdGenerator, p.second));
      m_pubKeyPeerBiMap.insert(PubKeyPeerBiMap::value_type(p.first, p.second));
      m_peerIdSet.insert(peerIdGenerator);
    }
  }
  m_fullNetworkKeys = fullNetworkKeys;

  // Now create the one and only RumorHolder
  if (GOSSIP_CUSTOM_ROUNDS_SETTINGS) {
    m_rumorHolder.reset(new RRS::RumorHolder(
        m_peerIdSet, MAX_ROUNDS_IN_BSTATE, MAX_ROUNDS_IN_CSTATE,
        MAX_TOTAL_ROUNDS, MAX_NEIGHBORS_PER_ROUND, 0));
  } else {
    m_rumorHolder.reset(new RRS::RumorHolder(m_peerIdSet, 0));
  }

  // RawMessage older than below expiry will be cleared.
  // Its calculated as (last KEEP_RAWMSG_FROM_LAST_N_ROUNDS rounds X each ROUND
  // time)
  m_rawMessageExpiryInMs = (KEEP_RAWMSG_FROM_LAST_N_ROUNDS < MAX_TOTAL_ROUNDS)
                               ? MAX_TOTAL_ROUNDS * 3 * (ROUND_TIME_IN_MS)
                               : KEEP_RAWMSG_FROM_LAST_N_ROUNDS *
                                     (ROUND_TIME_IN_MS);  // milliseconds

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

bool RumorManager::AddForeignRumor(const RumorManager::RawBytes& message) {
  // verify if the pubkey is from with-in our network
  PubKey senderPubKey;
  if (senderPubKey.Deserialize(message, 0) != 0) {
    return false;
  }

  if (find(m_fullNetworkKeys.begin(), m_fullNetworkKeys.end(), senderPubKey) ==
      m_fullNetworkKeys.end()) {
    LOG_GENERAL(WARNING,
                "Sender not from known network peer list. so ignoring message");
    return false;
  }

  // verify if signature matches the one in message.
  Signature toVerify;
  if (toVerify.Deserialize(message, PUB_KEY_SIZE) != 0) {
    return false;
  }

  bytes raw_message(message.begin() + PUB_KEY_SIZE + SIGNATURE_CHALLENGE_SIZE +
                        SIGNATURE_RESPONSE_SIZE,
                    message.end());

  if (!P2PComm::GetInstance().VerifyMessage(raw_message, toVerify,
                                            senderPubKey)) {
    LOG_GENERAL(WARNING, "Signature verification failed. so ignoring message");
    return false;
  }

  // All checks passed. Good to spread this rumor
  return AddRumor(raw_message);
}

bool RumorManager::AddRumor(const RumorManager::RawBytes& message) {
  LOG_MARKER();
  if (message.size() > 0 && message.size() <= MAX_GOSSIP_MSG_SIZE_IN_BYTES) {
    RawBytes hash = HashUtils::BytesToHash(message);
    std::string output;
    if (!DataConversion::Uint8VecToHexStr(hash, output)) {
      return false;
    }

    {
      std::lock_guard<std::mutex> guard(m_continueRoundMutex);
      if (!m_continueRound) {
        LOG_GENERAL(WARNING,
                    "Round is not running. So won't initiate the rumor. "
                    "Instead will buffer it. MyIP:"
                        << m_selfPeer << ". [Gossip_Message_Hash: "
                        << output.substr(0, 6) << " ]");

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

      auto result = m_rumorHashRawMsgBimap.insert(
          RumorHashRumorBiMap::value_type(hash, message));

      if (result.second) {
        // add the timestamp for this raw rumor message
        m_rumorRawMsgTimestamp.push_back(std::make_pair(
            result.first, std::chrono::high_resolution_clock::now()));

        std::string output;
        if (!DataConversion::Uint8VecToHexStr(hash, output)) {
          return false;
        }
        LOG_PAYLOAD(INFO,
                    "Initiated msg ("
                        << m_selfPeer << "): [ RumorId: " << m_rumorIdGenerator
                        << ", Round: 0, Hash: " << output.substr(0, 6) << " ]",
                    message, 10);

        return m_rumorHolder->addRumor(m_rumorIdGenerator);
      }
    } else {
      LOG_GENERAL(DEBUG, "This Rumor was already received. No problem.");
    }
  } else {
    LOG_GENERAL(WARNING, "Ignore msg. Msg Size :"
                             << message.size() << ", Expected Range: 1 - "
                             << MAX_GOSSIP_MSG_SIZE_IN_BYTES);
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

  // Add pubkey and signature before message body
  RawBytes tmp;
  m_selfKey.second.Serialize(tmp, 0);

  Signature sig = P2PComm::GetInstance().SignMessage(message);
  sig.Serialize(tmp, PUB_KEY_SIZE);

  cmd.insert(cmd.end(), tmp.begin(), tmp.end());

  cmd.insert(cmd.end(), message.begin(), message.end());

  return cmd;
}

void RumorManager::SendRumorToForeignPeers(
    const std::deque<Peer>& toForeignPeers, const RawBytes& message) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO,
              "Forwarding new gossip to foreign peers. My IP = " << m_selfPeer,
              message, Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Foreign Peers: ");
  for (auto& i : toForeignPeers) {
    LOG_GENERAL(INFO, i);
  }

  RawBytes cmd = GenerateGossipForwardMessage(message);

  P2PComm::GetInstance().SendMessage(toForeignPeers, cmd, START_BYTE_GOSSIP);
}

void RumorManager::SendRumorToForeignPeers(
    const std::vector<Peer>& toForeignPeers, const RawBytes& message) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO,
              "Forwarding new gossip to foreign peers. My IP = " << m_selfPeer,
              message, Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Foreign Peers: ");
  for (auto& i : toForeignPeers) {
    LOG_GENERAL(INFO, i);
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

std::pair<bool, RumorManager::RawBytes> RumorManager::VerifyMessage(
    const RawBytes& message, const RRS::Message::Type& t, const Peer& from) {
  bytes message_wo_keysig;

  if (((RRS::Message::Type::EMPTY_PUSH == t ||
        RRS::Message::Type::EMPTY_PULL == t) &&
       SIGN_VERIFY_EMPTY_MSGTYP) ||
      ((RRS::Message::Type::LAZY_PUSH == t ||
        RRS::Message::Type::LAZY_PULL == t || RRS::Message::Type::PUSH == t ||
        RRS::Message::Type::PULL == t) &&
       SIGN_VERIFY_NONEMPTY_MSGTYP)) {
    // verify if the pubkey is from with-in our network
    PubKey senderPubKey;
    if (senderPubKey.Deserialize(message, 0) != 0) {
      return {false, {}};
    }

    // Verify if the pub key of sender (myview) is same as pubkey received in
    // message
    auto k = m_pubKeyPeerBiMap.right.find(from);
    if (k == m_pubKeyPeerBiMap.right.end()) {
      // I dont know this peer, missing in my peerlist.
      LOG_GENERAL(DEBUG, "Received Rumor from peer : "
                             << from
                             << " whose pubkey does not exist in my store");
      return {false, {}};
    } else if (!(k->second == senderPubKey)) {
      LOG_GENERAL(WARNING,
                  "Public Key of sender does not exist in my list. so ignoring "
                  "message");
      return {false, {}};
    }

    // verify if signature matches the one in message.
    Signature toVerify;
    if (toVerify.Deserialize(message, PUB_KEY_SIZE) != 0) {
      return {false, {}};
    }

    message_wo_keysig.insert(message_wo_keysig.end(),
                             message.begin() + PUB_KEY_SIZE +
                                 SIGNATURE_CHALLENGE_SIZE +
                                 SIGNATURE_RESPONSE_SIZE,
                             message.end());

    if (!P2PComm::GetInstance().VerifyMessage(message_wo_keysig, toVerify,
                                              senderPubKey)) {
      LOG_GENERAL(WARNING,
                  "Signature verification failed. so ignoring message");
      return {false, {}};
    }
  } else {
    message_wo_keysig = message;
  }
  return {true, message_wo_keysig};
}

std::pair<bool, RumorManager::RawBytes> RumorManager::RumorReceived(
    uint8_t type, int32_t round, const RawBytes& message, const Peer& from) {
  {
    std::lock_guard<std::mutex> guard(m_continueRoundMutex);
    if (!m_continueRound) {
      // LOG_GENERAL(WARNING, "Round is not running. Ignoring message!!")
      return {false, {}};
    }
  }

  std::lock_guard<std::mutex> guard(m_mutex);

  auto p = m_peerIdPeerBimap.right.find(from);
  if (p == m_peerIdPeerBimap.right.end()) {
    // I dont know this peer, missing in my peerlist.
    LOG_GENERAL(DEBUG, "Received Rumor from peer : "
                           << from << " which does not exist in my peerlist.");
    return {false, {}};
  }

  int64_t recvdRumorId = -1;
  RRS::Message::Type t = convertType(type);
  bool toBeDispatched = false;

  auto result = VerifyMessage(message, t, from);
  if (!result.first) {
    return {false, {}};
  }
  bytes message_wo_keysig(result.second);

  // All checks passed. Good to accept this rumor

  if (RRS::Message::Type::EMPTY_PUSH == t ||
      RRS::Message::Type::EMPTY_PULL == t) {
    /* Don't add it to local RumorMap because it's not the rumor itself */
    LOG_GENERAL(DEBUG, "Received empty message of type: "
                           << RRS::Message::s_enumKeyToString[t]);
  } else if (RRS::Message::Type::LAZY_PUSH == t ||
             RRS::Message::Type::LAZY_PULL == t) {
    auto it = m_rumorIdHashBimap.right.find(message_wo_keysig);
    if (it == m_rumorIdHashBimap.right.end()) {
      recvdRumorId = ++m_rumorIdGenerator;

      m_rumorIdHashBimap.insert(
          RumorIdRumorBimap::value_type(recvdRumorId, message_wo_keysig));

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
      auto it = m_rumorHashRawMsgBimap.left.find(message_wo_keysig);
      if (it == m_rumorHashRawMsgBimap.left.end()) {
        // didn't receive real message (PUSH) yet :( Lets ask this peer.
        RRS::Message pullMsg(RRS::Message::Type::PULL, recvdRumorId, -1);
        SendMessage(from, pullMsg);
      }
    }
  } else if (RRS::Message::Type::PULL == t) {
    // Now that sender wants the real message, lets send it to him.
    auto it1 = m_rumorHashRawMsgBimap.left.find(message_wo_keysig);
    if (it1 != m_rumorHashRawMsgBimap.left.end()) {
      auto it2 = m_rumorIdHashBimap.right.find(message_wo_keysig);
      if (it2 != m_rumorIdHashBimap.right.end()) {
        recvdRumorId = it2->second;
        RRS::Message pushMsg(RRS::Message::Type::PUSH, recvdRumorId, -1);
        SendMessage(from, pushMsg);
      }
    } else  // I dont have it as of now. Add this peer to subscriber list for
            // this hash message.
    {
      auto it2 = m_hashesSubscriberMap.find(message_wo_keysig);
      if (it2 == m_hashesSubscriberMap.end()) {
        m_hashesSubscriberMap.insert(RumorHashesPeersMap::value_type(
            message_wo_keysig, std::set<Peer>()));
      }
      m_hashesSubscriberMap[message_wo_keysig].insert(from);
    }
    return {false, {}};
  } else if (RRS::Message::Type::PUSH == t) {
    // I got it from my peer for what i asked him
    RawBytes hash;
    if (message_wo_keysig.size() >
        0)  // if someone malaciously sends empty message, sha2 will assert fail
    {
      hash = HashUtils::BytesToHash(message_wo_keysig);
      std::string hashStr;
      DataConversion::Uint8VecToHexStr(hash, hashStr);

      auto it1 = m_rumorIdHashBimap.right.find(hash);
      if (it1 != m_rumorIdHashBimap.right.end()) {
        recvdRumorId = it1->second;
      } else {
        // I have not asked for this raw message.. so ignoring
        return {false, {}};
      }

      // toBeDispatched
      auto result = m_rumorHashRawMsgBimap.insert(
          RumorHashRumorBiMap::value_type(hash, message_wo_keysig));
      if (result.second) {
        LOG_PAYLOAD(
            INFO,
            "New msg for hash [" << hashStr.substr(0, 6) << "] from " << from,
            message_wo_keysig, Logger::MAX_BYTES_TO_DISPLAY);
        toBeDispatched = true;
        // add the timestamp for this raw rumor message
        m_rumorRawMsgTimestamp.push_back(std::make_pair(
            result.first, std::chrono::high_resolution_clock::now()));
      } else {
        LOG_PAYLOAD(DEBUG,
                    "Old Gossip Raw message received from Peer: "
                        << from << ", Gossip_Message_Hash: "
                        << hashStr.substr(0, 6) << " ]",
                    message_wo_keysig, Logger::MAX_BYTES_TO_DISPLAY);
      }

      // Do i have any peers subscribed with me for this hash.
      auto it2 = m_hashesSubscriberMap.find(hash);
      if (it2 != m_hashesSubscriberMap.end()) {
        // Send PUSH
        LOG_GENERAL(
            DEBUG,
            "Sending Gossip Raw Message to subscribers of Gossip_Message_Hash: "
                << hashStr.substr(0, 6));
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
    return {toBeDispatched, message_wo_keysig};
  } else {
    LOG_GENERAL(WARNING, "Unknown message type received");
    return {false, {}};
  }

  RRS::Message recvMsg(t, recvdRumorId, round);

  std::pair<int, std::vector<RRS::Message>> pullMsgs =
      m_rumorHolder->receivedMessage(recvMsg, p->second);

  LOG_GENERAL(DEBUG, "Sending " << pullMsgs.second.size()
                                << " EMPTY_PULL or LAZY_PULL Messages");

  SendMessages(from, pullMsgs.second);

  return {toBeDispatched, message_wo_keysig};
}

void RumorManager::AppendKeyAndSignature(RawBytes& result,
                                         const RawBytes& messageToSig) {
  // Add pubkey and signature before message body
  RawBytes tmp;
  m_selfKey.second.Serialize(tmp, 0);

  Signature sig = P2PComm::GetInstance().SignMessage(messageToSig);
  sig.Serialize(tmp, PUB_KEY_SIZE);

  result.insert(result.end(), tmp.begin(), tmp.end());
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
          if (SIGN_VERIFY_NONEMPTY_MSGTYP) {
            // Add pubkey and signature before message body
            AppendKeyAndSignature(cmd, it2->second);
          }

          // Add raw message to outgoing message
          cmd.insert(cmd.end(), it2->second.begin(), it2->second.end());
          std::string gossipHashStr;
          if (!DataConversion::Uint8VecToHexStr(it1->second, gossipHashStr)) {
            return;
          }
          LOG_GENERAL(INFO, "Sending [" << gossipHashStr.substr(0, 6) << "] to "
                                        << toPeer);
        } else {
          // Nothing to send.
          return;
        }
      } else if (RRS::Message::Type::LAZY_PUSH == t ||
                 RRS::Message::Type::LAZY_PULL == t ||
                 RRS::Message::Type::PULL == t) {
        if (SIGN_VERIFY_NONEMPTY_MSGTYP) {
          // Add pubkey and signature before message body
          AppendKeyAndSignature(cmd, it1->second);
        }

        // Add hash message to outgoing message for types
        // LAZY_PULL/LAZY_PUSH/PULL
        cmd.insert(cmd.end(), it1->second.begin(), it1->second.end());
        LOG_GENERAL(DEBUG, "Sending Gossip Hash Message: "
                               << message << " To Peer : " << toPeer);
      } else {
        return;
      }
    }
  } else {  // EMPTY_PULL/ EMPTY_PUSH
    if (SIGN_VERIFY_EMPTY_MSGTYP) {
      // Add pubkey and signature before message body
      RawBytes dummyMsg = {'D', 'U', 'M', 'M', 'Y'};
      AppendKeyAndSignature(cmd, dummyMsg);
      // Add dummy message to outgoing message
      cmd.insert(cmd.end(), dummyMsg.begin(), dummyMsg.end());
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
      std::string gossipHashStr;
      if (!DataConversion::Uint8VecToHexStr(this_msg_hash, gossipHashStr)) {
        continue;
      }
      LOG_GENERAL(INFO, "[ RumorId: " << rumorId << ", Hash: "
                                      << gossipHashStr.substr(0, 6) << " ], "
                                      << state);
    }
  }
}

void RumorManager::CleanUp() {
  int count = 0;
  auto now = std::chrono::high_resolution_clock::now();
  while (!m_rumorRawMsgTimestamp.empty()) {
    auto elapsed_milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - m_rumorRawMsgTimestamp.front().second)
            .count();
    LOG_GENERAL(DEBUG, "elapsed_milliseconds:" << elapsed_milliseconds
                                               << " , m_rawMessageExpiryInMs:"
                                               << m_rawMessageExpiryInMs);
    if (elapsed_milliseconds > m_rawMessageExpiryInMs) {  // older
      auto hash = m_rumorRawMsgTimestamp.front().first->left;
      m_rumorHashRawMsgBimap.erase(m_rumorRawMsgTimestamp.front().first);

      m_rumorIdHashBimap.right.erase(hash);
      m_rumorRawMsgTimestamp.pop_front();
      count++;
    } else {
      break;  // other in deque are definately not older. so quit loop
    }
  }
  if (count != 0) {
    LOG_GENERAL(INFO, "Cleaned " << count << " messages");
  }
}
