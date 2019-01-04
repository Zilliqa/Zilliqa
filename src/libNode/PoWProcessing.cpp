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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

bool Node::GetLatestDSBlock() {
  unsigned int counter = 1;
  while (!m_mediator.m_lookup->m_fetchedLatestDSBlock &&
         counter <= FETCH_LOOKUP_MSG_MAX_RETRY) {
    m_synchronizer.FetchLatestDSBlocksSeed(
        m_mediator.m_lookup,
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1);

    {
      unique_lock<mutex> lock(
          m_mediator.m_lookup->m_mutexLatestDSBlockUpdation);
      if (m_mediator.m_lookup->cv_latestDSBlock.wait_for(
              lock, chrono::seconds(NEW_NODE_SYNC_INTERVAL)) ==
          std::cv_status::timeout) {
        LOG_GENERAL(WARNING, "FetchLatestDSBlocks Timeout... tried "
                                 << counter << "/" << FETCH_LOOKUP_MSG_MAX_RETRY
                                 << " times");
        counter++;
      } else {
        break;
      }
    }
  }
  if (!m_mediator.m_lookup->m_fetchedLatestDSBlock) {
    LOG_GENERAL(WARNING, "Fetch latest DS Block failed");
    return false;
  }
  m_mediator.m_lookup->m_fetchedLatestDSBlock = false;
  return true;
}

bool Node::StartPoW(const uint64_t& block_num, uint8_t ds_difficulty,
                    uint8_t difficulty,
                    const array<unsigned char, UINT256_SIZE>& rand1,
                    const array<unsigned char, UINT256_SIZE>& rand2,
                    const uint32_t lookupId) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::StartPoW not expected to be called from LookUp node.");
    return true;
  }

  if (!CheckState(STARTPOW)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Not in POW_SUBMISSION state");
    return false;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Current dsblock is " << block_num);

  m_stillMiningPrimary = true;

  lock_guard<mutex> g(m_mutexGasPrice);

  ethash_mining_result winning_result;

  uint32_t shardGuardDiff = 1;
  auto headerHash = POW::GenHeaderHash(
      rand1, rand2, m_mediator.m_selfPeer.m_ipAddress,
      m_mediator.m_selfKey.second, lookupId, m_proposedGasPrice);
  // Only in guard mode that shard guard can submit diffferent PoW
  if (GUARD_MODE && Guard::GetInstance().IsNodeInShardGuardList(
                        m_mediator.m_selfKey.second)) {
    winning_result = POW::GetInstance().PoWMine(
        block_num, shardGuardDiff, headerHash, FULL_DATASET_MINE, std::time(0));
  } else {
    winning_result = POW::GetInstance().PoWMine(
        block_num, difficulty, headerHash, FULL_DATASET_MINE, std::time(0));
  }

  if (winning_result.success) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning nonce   = 0x" << hex << winning_result.winning_nonce);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning result  = 0x" << hex << winning_result.result);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Winning mixhash = 0x" << hex << winning_result.mix_hash);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "rand1 = 0x" << DataConversion::charArrToHexStr(rand1));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "rand2 = 0x" << DataConversion::charArrToHexStr(rand2));

    m_stillMiningPrimary = false;

    // Possible scenarios
    // 1. Found solution that meets ds difficulty and difficulty
    // - Submit solution
    // 2. Found solution that meets only difficulty
    // - Submit solution and continue to do PoW till DS difficulty met or
    //   ds block received. (stopmining())
    auto checkerThread = [this]() mutable -> void {
      unique_lock<mutex> lk(m_mutexCVWaitDSBlock);
      const unsigned int fixedDSNodesPoWTime =
          NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
          POWPACKETSUBMISSION_WINDOW_IN_SECONDS;
      const unsigned int fixedDSBlockDistributionDelayTime =
          DELAY_FIRSTXNEPOCH_IN_MS / 1000;
      const unsigned int extraWaitTime = DSBLOCK_EXTRA_WAIT_TIME;
      if (cv_waitDSBlock.wait_for(
              lk, chrono::seconds(fixedDSNodesPoWTime +
                                  fixedDSBlockDistributionDelayTime +
                                  extraWaitTime)) == cv_status::timeout) {
        lock_guard<mutex> g(m_mutexDSBlock);
        if (m_mediator.m_currentEpochNum ==
            m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetEpochNum()) {
          LOG_GENERAL(WARNING, "DS was processed just now, ignore time out");
          return;
        }

        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Time out while waiting for DS Block");

        POW::GetInstance().StopMining();

        if (GetLatestDSBlock()) {
          LOG_GENERAL(INFO, "DS block created, means I lost PoW");
          if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
            // exciplitly declare in the same thread
            m_mediator.m_lookup->m_startedPoW = false;
          }
          RejoinAsNormal();
        } else {
          LOG_GENERAL(WARNING, "DS block not recvd, what to do ?");
        }
      }
    };

    // In guard mode, an additional scenario
    // 1. Shard guard submit pow with diff shardGuardDiff
    if (GUARD_MODE && Guard::GetInstance().IsNodeInShardGuardList(
                          m_mediator.m_selfKey.second)) {
      if (!SendPoWResultToDSComm(block_num, shardGuardDiff,
                                 winning_result.winning_nonce,
                                 winning_result.result, winning_result.mix_hash,
                                 lookupId, m_proposedGasPrice)) {
        return false;
      } else {
        DetachedFunction(1, checkerThread);
      }
    } else if (POW::GetInstance().CheckSolnAgainstsTargetedDifficulty(
                   winning_result.result, ds_difficulty)) {
      LOG_GENERAL(INFO,
                  "Found PoW solution that met requirement for both ds "
                  "commitee and shard.");

      if (!SendPoWResultToDSComm(block_num, ds_difficulty,
                                 winning_result.winning_nonce,
                                 winning_result.result, winning_result.mix_hash,
                                 lookupId, m_proposedGasPrice)) {
        return false;
      } else {
        DetachedFunction(1, checkerThread);
      }
    } else {
      // If solution does not meet targeted ds difficulty, send the initial
      // solution to ds commitee and continue to do PoW
      if (!SendPoWResultToDSComm(block_num, difficulty,
                                 winning_result.winning_nonce,
                                 winning_result.result, winning_result.mix_hash,
                                 lookupId, m_proposedGasPrice)) {
        return false;
      } else {
        DetachedFunction(1, checkerThread);
      }

      LOG_GENERAL(INFO,
                  "soln does not meet ds committee criteria. Will keep "
                  "doing more pow");

      ethash_mining_result ds_pow_winning_result = POW::GetInstance().PoWMine(
          block_num, ds_difficulty, headerHash, FULL_DATASET_MINE,
          winning_result.winning_nonce);

      if (ds_pow_winning_result.success) {
        LOG_GENERAL(INFO,
                    "Found PoW solution that meets ds commitee "
                    "requirement. 0x"
                        << hex << ds_pow_winning_result.result);

        // Submission of PoW for ds commitee
        if (!SendPoWResultToDSComm(
                block_num, ds_difficulty, ds_pow_winning_result.winning_nonce,
                ds_pow_winning_result.result, ds_pow_winning_result.mix_hash,
                lookupId, m_proposedGasPrice)) {
          return false;
        }
      } else {
        LOG_GENERAL(INFO,
                    "Unable to find PoW solution that meet ds commitee "
                    "requirement");
      }
    }
  }

  if (m_state != MICROBLOCK_CONSENSUS_PREP && m_state != MICROBLOCK_CONSENSUS) {
    SetState(WAITING_DSBLOCK);
  }

  return true;
}

bool Node::SendPoWResultToDSComm(const uint64_t& block_num,
                                 const uint8_t& difficultyLevel,
                                 const uint64_t winningNonce,
                                 const string& powResultHash,
                                 const string& powMixhash,
                                 const uint32_t& lookupId,
                                 const uint128_t& gasPrice) {
  LOG_MARKER();

  bytes powmessage = {MessageType::DIRECTORY, DSInstructionType::POWSUBMISSION};

  if (!Messenger::SetDSPoWSubmission(
          powmessage, MessageOffset::BODY, block_num, difficultyLevel,
          m_mediator.m_selfPeer, m_mediator.m_selfKey, winningNonce,
          powResultHash, powMixhash, lookupId, gasPrice)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::SetDSPoWSubmission failed.");
    return false;
  }

  vector<Peer> peerList;

  // Send to PoW PACKET_SENDERS which including DS leader
  Peer dsLeaderPeer;
  if (!m_mediator.m_DSCommittee->empty()) {
    if (Node::GetDSLeaderPeer(m_mediator.m_blocklinkchain.GetLatestBlockLink(),
                              m_mediator.m_dsBlockChain.GetLastBlock(),
                              *m_mediator.m_DSCommittee,
                              m_mediator.m_currentEpochNum, dsLeaderPeer)) {
      peerList.push_back(dsLeaderPeer);
    }
  }

  for (auto const& i : *m_mediator.m_DSCommittee) {
    if (peerList.size() < POW_PACKET_SENDERS && i.second != dsLeaderPeer) {
      peerList.push_back(i.second);
    }

    if (peerList.size() >= POW_PACKET_SENDERS) {
      break;
    }
  }

  P2PComm::GetInstance().SendMessage(peerList, powmessage);
  return true;
}

bool Node::ReadVariablesFromStartPoWMessage(
    const bytes& message, unsigned int cur_offset, uint64_t& block_num,
    uint8_t& ds_difficulty, uint8_t& difficulty,
    array<unsigned char, 32>& rand1, array<unsigned char, 32>& rand2) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ReadVariablesFromStartPoWMessage not expected to be "
                "called from LookUp node.");
    return true;
  }

  if (IsMessageSizeInappropriate(message.size(), cur_offset,
                                 sizeof(uint64_t) + sizeof(uint8_t) +
                                     sizeof(uint8_t) + UINT256_SIZE +
                                     UINT256_SIZE,
                                 PUB_KEY_SIZE + IP_SIZE + PORT_SIZE)) {
    return false;
  }

  // 8-byte block num
  block_num =
      Serializable::GetNumber<uint64_t>(message, cur_offset, sizeof(uint64_t));
  cur_offset += sizeof(uint64_t);

  // 1-byte ds difficulty
  ds_difficulty =
      Serializable::GetNumber<uint8_t>(message, cur_offset, sizeof(uint8_t));
  cur_offset += sizeof(uint8_t);

  // 1-byte difficulty
  difficulty =
      Serializable::GetNumber<uint8_t>(message, cur_offset, sizeof(uint8_t));
  cur_offset += sizeof(uint8_t);

  // 32-byte rand1
  copy(message.begin() + cur_offset,
       message.begin() + cur_offset + UINT256_SIZE, rand1.begin());
  cur_offset += UINT256_SIZE;

  // 32-byte rand2
  copy(message.begin() + cur_offset,
       message.begin() + cur_offset + UINT256_SIZE, rand2.begin());
  cur_offset += UINT256_SIZE;
  LOG_STATE("[START][EPOCH][" << std::setw(15) << std::left
                              << m_mediator.m_selfPeer.GetPrintableIPAddress()
                              << "][" << block_num << "]");

  // Log all values
  /**
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "My IP address     = "
                << m_mediator.m_selfPeer.GetPrintableIPAddress());
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "My Listening Port = " << m_mediator.m_selfPeer.m_listenPortHost);
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DS Difficulty        = " << to_string(ds_difficulty));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Difficulty        = " << to_string(difficulty));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Rand1             = " << DataConversion::charArrToHexStr(rand1));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Rand2             = " << DataConversion::charArrToHexStr(rand2));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Pubkey            = " << DataConversion::SerializableToHexStr(
                m_mediator.m_selfKey.second));
  **/

  // DS nodes ip addr and port
  const unsigned int numDS =
      (message.size() - cur_offset) / (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE);

  // Create and keep a view of the DS committee
  // We'll need this if we win PoW
  m_mediator.m_DSCommittee->clear();
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "DS nodes count    = " << numDS + 1);
  for (unsigned int i = 0; i < numDS; i++) {
    PubKey pubkey(message, cur_offset);
    cur_offset += PUB_KEY_SIZE;

    m_mediator.m_DSCommittee->emplace_back(
        make_pair(pubkey, Peer(message, cur_offset)));

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "DS Node IP: "
            << m_mediator.m_DSCommittee->back().second.GetPrintableIPAddress()
            << " Port: "
            << m_mediator.m_DSCommittee->back().second.m_listenPortHost);
    cur_offset += IP_SIZE + PORT_SIZE;
  }

  {
    lock_guard<mutex> g(m_mediator.m_mutexInitialDSCommittee);
    if (m_mediator.m_DSCommittee->size() !=
        m_mediator.m_initialDSCommittee->size()) {
      LOG_GENERAL(WARNING,
                  "The initial DS committee from file and "
                  "ReadVariablesFromStartPoWMessage size do not match "
                      << m_mediator.m_DSCommittee->size() << " "
                      << m_mediator.m_initialDSCommittee->size());
    }
    unsigned int i = 0;
    for (auto const& dsNode : *m_mediator.m_DSCommittee) {
      if (!(dsNode.first == m_mediator.m_initialDSCommittee->at(i))) {
        LOG_GENERAL(WARNING,
                    "PubKey from file and ReadVariablesFromStartPoWMessage do "
                    "not match  "
                        << dsNode.first << " "
                        << m_mediator.m_initialDSCommittee->at(i))
      }
      i++;
    }
  }

  return true;
}

bool Node::ProcessStartPoW(const bytes& message, unsigned int offset,
                           [[gnu::unused]] const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Node::ProcessStartPoW not expected to be called from "
                "LookUp node.");
    return true;
  }

  // Note: This function should only be invoked on a new node that was not part
  // of the sharding committees in previous epoch Message = [8-byte block num]
  // [1-byte ds difficulty]  [1-byte difficulty] [32-byte rand1] [32-byte rand2]
  // [33-byte pubkey] [16-byte ip] [4-byte port] ... (all the DS nodes)

  LOG_MARKER();
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "START OF EPOCH " << m_mediator.m_dsBlockChain.GetLastBlock()
                                         .GetHeader()
                                         .GetBlockNum() +
                                     1);

  if (m_mediator.m_currentEpochNum > 1) {
    // TODO:: Get the IP address of who send this message, and deduct its
    // reputation.
    LOG_GENERAL(WARNING,
                "Node::ProcessStartPoW is a bootstrap function, it "
                "shouldn't be called after blockchain started.");
    return false;
  }

  uint64_t block_num;
  uint8_t difficulty = POW_DIFFICULTY;
  uint8_t dsDifficulty = DS_POW_DIFFICULTY;

  array<unsigned char, 32> rand1;
  array<unsigned char, 32> rand2;

  if (!ReadVariablesFromStartPoWMessage(
          message, offset, block_num, dsDifficulty, difficulty, rand1, rand2)) {
    return false;
  }

  if (m_mediator.m_isRetrievedHistory) {
    block_num =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1;
    dsDifficulty =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
    difficulty =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
    rand1 = m_mediator.m_dsBlockRand;
    rand2 = m_mediator.m_txBlockRand;
  }

  // Start mining
  StartPoW(block_num, dsDifficulty, difficulty, rand1, rand2);

  return true;
}
