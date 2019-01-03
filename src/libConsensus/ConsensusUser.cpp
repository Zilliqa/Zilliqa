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

#include "ConsensusUser.h"
#include "common/Messages.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;

bool ConsensusUser::ProcessSetLeader(const bytes& message, unsigned int offset,
                                     [[gnu::unused]] const Peer& from) {
  // Message = 2-byte ID of leader (0 to num nodes - 1)

  LOG_MARKER();

  if ((m_consensus != nullptr) &&
      (m_consensus->GetState() != ConsensusCommon::State::DONE) &&
      (m_consensus->GetState() != ConsensusCommon::State::ERROR)) {
    LOG_GENERAL(WARNING,
                "You're trying to set me again but my consensus is "
                "still not finished");
    return false;
  }

  uint16_t leader_id =
      Serializable::GetNumber<uint16_t>(message, offset, sizeof(uint16_t));

  uint32_t dummy_consensus_id = 0xFACEFACE;
  uint64_t dummy_block_number = 12345678;

  bytes dummy_block_hash(BLOCK_HASH_SIZE);
  fill(dummy_block_hash.begin(), dummy_block_hash.end(), 0x88);

  // For this test class, we assume the committee = everyone in the peer store

  // The peer store is sorted by PubKey
  // This means everyone can have a consistent view of a sorted list of public
  // keys and IP addresses for this committee We can then assign IDs to each one
  // of us, starting from 0 to num nodes - 1 But first I need to add my own
  // PubKey into the peer store so it can be sorted with the rest

  // In real usage, we don't expect to use the peerstore to assemble the list of
  // pub keys The DS block should have the info we need, and the peerstore only
  // needs to be used to get the IP addresses

  PeerStore& peerstore = PeerStore::GetStore();
  peerstore.AddPeerPair(m_selfKey.second,
                        Peer());  // Add myself, but with dummy IP info

  vector<pair<PubKey, Peer>> tmp = peerstore.GetAllPeerPairs();
  deque<pair<PubKey, Peer>> peerList(tmp.size());
  copy(tmp.begin(), tmp.end(),
       peerList.begin());                  // This will be sorted by PubKey
  peerstore.RemovePeer(m_selfKey.second);  // Remove myself

  // Now I need to find my index in the sorted list (this will be my ID for the
  // consensus)
  uint16_t my_id = 0;
  for (auto const& i : peerList) {
    if (i.first == m_selfKey.second) {
      LOG_GENERAL(INFO, "My node ID for this consensus is " << my_id);
      break;
    }
    my_id++;
  }

  LOG_GENERAL(INFO, "The leader is using " << peerList.at(leader_id).second);

  m_leaderOrBackup = (leader_id != my_id);

  if (!m_leaderOrBackup)  // Leader
  {
    m_consensus.reset(new ConsensusLeader(
        dummy_consensus_id, dummy_block_number, dummy_block_hash, my_id,
        m_selfKey.first, peerList,
        static_cast<uint8_t>(MessageType::CONSENSUSUSER),
        static_cast<uint8_t>(InstructionType::CONSENSUS),
        std::function<bool(const bytes& errorMsg, unsigned int,
                           const Peer& from)>(),
        std::function<bool(map<unsigned int, bytes>)>()));
  } else  // Backup
  {
    auto func = [this](const bytes& message, bytes& errorMsg) mutable -> bool {
      return MyMsgValidatorFunc(message, errorMsg);
    };

    m_consensus.reset(new ConsensusBackup(
        dummy_consensus_id, dummy_block_number, dummy_block_hash, my_id,
        leader_id, m_selfKey.first, peerList,
        static_cast<uint8_t>(MessageType::CONSENSUSUSER),
        static_cast<uint8_t>(InstructionType::CONSENSUS), func));
  }

  if (m_consensus == nullptr) {
    LOG_GENERAL(WARNING, "Consensus object creation failed");
    return false;
  }

  return true;
}

bool ConsensusUser::ProcessStartConsensus(const bytes& message,
                                          unsigned int offset,
                                          [[gnu::unused]] const Peer& from) {
  // Message = [message for consensus]

  LOG_MARKER();

  if (m_consensus == nullptr) {
    LOG_GENERAL(WARNING, "You didn't set me yet");
    return false;
  }

  if (m_consensus->GetState() != ConsensusCommon::State::INITIAL) {
    LOG_GENERAL(WARNING, "You already called me before. Set me again first.");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensus.get());
  if (cl == NULL) {
    LOG_GENERAL(WARNING,
                "I'm a backup, you can't start consensus "
                "(announcement) thru me");
    return false;
  }

  bytes m(message.size() - offset);
  copy(message.begin() + offset, message.end(), m.begin());

  cl->StartConsensus(m, m.size());

  return true;
}

bool ConsensusUser::ProcessConsensusMessage(const bytes& message,
                                            unsigned int offset,
                                            const Peer& from) {
  LOG_MARKER();

  if (m_consensus == nullptr) {
    LOG_GENERAL(WARNING, "m_consensus is not yet initialize");
    return false;
  }

  std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
  if (cv_processConsensusMessage.wait_for(
          cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
          [this, message, offset]() -> bool {
            return m_consensus->CanProcessMessage(message, offset);
          })) {
    // order preserved
  } else {
    LOG_GENERAL(
        WARNING,
        "Timeout while waiting for correct order of consensus messages");
    return false;
  }

  bool result = m_consensus->ProcessMessage(message, offset, from);

  if (m_consensus->GetState() == ConsensusCommon::State::DONE) {
    LOG_GENERAL(INFO, "Consensus is DONE!!!");

    bytes tmp;
    m_consensus->GetCS2().Serialize(tmp, 0);
    LOG_PAYLOAD(INFO, "Final collective signature", tmp, 100);

    tmp.clear();
    BitVector::SetBitVector(tmp, 0, m_consensus->GetB2());
    LOG_PAYLOAD(INFO, "Final collective signature bitmap", tmp, 100);
  } else {
    cv_processConsensusMessage.notify_all();
  }

  return result;
}

ConsensusUser::ConsensusUser(const pair<PrivKey, PubKey>& key, const Peer& peer)
    : m_selfKey(key),
      m_selfPeer(peer),
      m_leaderOrBackup(false),
      m_consensus(nullptr) {}

ConsensusUser::~ConsensusUser() {}

bool ConsensusUser::Execute(const bytes& message, unsigned int offset,
                            const Peer& from) {
  // LOG_MARKER();

  bool result = false;

  typedef bool (ConsensusUser::*InstructionHandler)(const bytes&, unsigned int,
                                                    const Peer&);

  InstructionHandler ins_handlers[] = {&ConsensusUser::ProcessSetLeader,
                                       &ConsensusUser::ProcessStartConsensus,
                                       &ConsensusUser::ProcessConsensusMessage};

  const unsigned char ins_byte = message.at(offset);

  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);

    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_GENERAL(WARNING, "Unknown instruction byte "
                             << hex << (unsigned int)ins_byte << " from "
                             << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
  }

  return result;
}

bool ConsensusUser::MyMsgValidatorFunc(const bytes& message,
                                       [[gnu::unused]] bytes& errorMsg) {
  LOG_MARKER();
  LOG_PAYLOAD(INFO, "Message", message, Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Message is valid. ");

  return true;
}
