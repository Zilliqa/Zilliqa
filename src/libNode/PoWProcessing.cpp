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

#include <Schnorr.h>
#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountStore/AccountStore.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2P.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using namespace boost::multiprecision;

constexpr const unsigned int IP_SIZE = 16;
constexpr const unsigned int PORT_SIZE = 4;

// TODO: only used in libNode. Move somewhere more appropriate.
extern bool IsMessageSizeInappropriate(unsigned int messageSize,
                                       unsigned int offset,
                                       unsigned int minLengthNeeded,
                                       unsigned int factor = 0,
                                       const std::string& errMsg = "");

bool Node::GetLatestDSBlock() {
  LOG_MARKER();
  unsigned int counter = 1;
  while (!m_mediator.m_lookup->m_fetchedLatestDSBlock &&
         counter <= FETCH_LOOKUP_MSG_MAX_RETRY) {
    m_synchronizer.FetchLatestDSBlocksSeed(
        m_mediator.m_lookup,
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1);
    {
      unique_lock<mutex> lock(
          m_mediator.m_lookup->m_mutexLatestDSBlockUpdation);
      // TODO: cv fix
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
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Not in POW_SUBMISSION state");
    return false;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Current dsblock is " << block_num);

  lock_guard<mutex> g(m_mutexGasPrice);

  auto headerHash = POW::GenHeaderHash(rand1, rand2, m_mediator.m_selfPeer,
                                       m_mediator.m_selfKey.second, lookupId,
                                       m_proposedGasPrice);

  EthashMiningResult ds_pow_winning_result = POW::GetInstance().PoWMine(
      block_num, ds_difficulty, m_mediator.m_selfKey, headerHash,
      FULL_DATASET_MINE, std::time(0), POW_WINDOW_IN_SECONDS);

  if (ds_pow_winning_result.success) {
    LOG_GENERAL(INFO, "DS diff soln = " << ds_pow_winning_result.result);

    LOG_GENERAL(INFO,
                "nonce   = " << hex << ds_pow_winning_result.winning_nonce);
    LOG_GENERAL(INFO, "result  = " << hex << ds_pow_winning_result.result);
    LOG_GENERAL(INFO, "mixhash = " << hex << ds_pow_winning_result.mix_hash);
    auto checkerThread = [this]() mutable -> void {
      unique_lock<mutex> lk(m_mutexCVWaitDSBlock);
      const unsigned int fixedDSNodesPoWTime =
          NEW_NODE_SYNC_INTERVAL + POW_WINDOW_IN_SECONDS +
          POWPACKETSUBMISSION_WINDOW_IN_SECONDS;
      const unsigned int fixedDSBlockDistributionDelayTime =
          DELAY_FIRSTXNEPOCH_IN_MS / 1000;
      const unsigned int extraWaitTime = DSBLOCK_EXTRA_WAIT_TIME;
      // TODO: cv fix
      if (cv_waitDSBlock.wait_for(
              lk, chrono::seconds(fixedDSNodesPoWTime +
                                  fixedDSBlockDistributionDelayTime +
                                  extraWaitTime)) == cv_status::timeout) {
        lock_guard<mutex> g(m_mutexDSBlock);

        POW::GetInstance().StopMining();

        if (m_mediator.m_currentEpochNum ==
            m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetEpochNum()) {
          LOG_GENERAL(WARNING, "DS was processed just now, ignore time out");
          return;
        }

        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "Time out while waiting for DS Block");
        // notify wait in InitMining
        m_mediator.m_lookup->cv_waitJoined.notify_all();

        if (GetLatestDSBlock()) {
          LOG_GENERAL(INFO, "DS block created, means I lost PoW");
          if (m_mediator.m_lookup->GetSyncType() == SyncType::NO_SYNC) {
            // exciplitly declare in the same thread
            m_mediator.m_lookup->m_startedPoW = false;
          }
          m_mediator.m_lookup->SetSyncType(SyncType::NORMAL_SYNC);
          StartSynchronization();
        } else {
          LOG_GENERAL(WARNING, "DS block not recvd, will initiate rejoin");
          RejoinAsNormal();
        }
      }
    };
    // Submission of PoW for ds commitee
    if (!SendPoWResultToDSComm(
            block_num, ds_difficulty, ds_pow_winning_result.winning_nonce,
            ds_pow_winning_result.result, ds_pow_winning_result.mix_hash,
            lookupId, m_proposedGasPrice)) {
      return false;
    } else {
      DetachedFunction(1, checkerThread);
    }
  } else {
    // If failed to do PoW, try to rejoin in next DS block
    LOG_GENERAL(
        INFO,
        "Failed to do PoW, setting to sync mode, try do pow in new DS epoch ");
    m_mediator.m_lookup->m_startedPoW = false;
    m_mediator.m_lookup->SetSyncType(SyncType::NORMAL_SYNC);
    StartSynchronization();
    return false;
  }

  SetState(WAITING_DSBLOCK);

  return true;
}

bool Node::CheckIfGovProposalActive() {
  LOG_MARKER();

#ifdef GOVVC_TEST_DS_SUSPEND_3
  srand(time(0));
  m_govProposalInfo.startDSEpoch = 1;
  m_govProposalInfo.endDSEpoch = 5;
  m_govProposalInfo.remainingVoteCount = 3;
  m_govProposalInfo.proposal.first = rand() % 100;
  m_govProposalInfo.proposal.second = rand() % 10;
  LOG_GENERAL(
      INFO,
      "[GOVVCTEST] m_govProposalInfo startDSEpoch="
          << m_govProposalInfo.startDSEpoch
          << " endDSEpoch=" << m_govProposalInfo.endDSEpoch
          << " isGovProposalActive=" << m_govProposalInfo.isGovProposalActive
          << " remainingVoteCount=" << m_govProposalInfo.remainingVoteCount
          << " proposalId=" << m_govProposalInfo.proposal.first
          << " voteValue=" << m_govProposalInfo.proposal.second);
#endif

  uint64_t curDSEpochNo =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  curDSEpochNo++;
  if (curDSEpochNo >= m_govProposalInfo.startDSEpoch &&
      curDSEpochNo <= m_govProposalInfo.endDSEpoch) {
    m_govProposalInfo.isGovProposalActive = true;
    return true;
  } else {
    m_govProposalInfo.isGovProposalActive = false;
  }
  return false;
}

bool Node::SendPoWResultToDSComm(const uint64_t& block_num,
                                 const uint8_t& difficultyLevel,
                                 const uint64_t winningNonce,
                                 const string& powResultHash,
                                 const string& powMixhash,
                                 const uint32_t& lookupId,
                                 const uint128_t& gasPrice) {
  LOG_MARKER();

  // If governance proposal is active, send vote in DS epoch range POW
  GovProposalIdVotePair govProposal{0, 0};
  {
    lock_guard<mutex> g(m_mutexGovProposal);
    if (CheckIfGovProposalActive()) {
      govProposal = m_govProposalInfo.proposal;
      LOG_GENERAL(INFO, "[Gov] proposalId=" << govProposal.first
                                            << " vote=" << govProposal.second);
    }
  }

  zbytes powmessage = {MessageType::DIRECTORY,
                       DSInstructionType::POWSUBMISSION};

  if (!Messenger::SetDSPoWSubmission(
          powmessage, MessageOffset::BODY, block_num, difficultyLevel,
          m_mediator.m_selfPeer, m_mediator.m_selfKey, winningNonce,
          powResultHash, powMixhash, lookupId, gasPrice, govProposal,
          POW_SUBMISSION_VERSION_TAG == "" ? VERSION_TAG
                                           : POW_SUBMISSION_VERSION_TAG)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetDSPoWSubmission failed.");
    return false;
  }

  vector<Peer> peerList;

  // Send to PoW PACKET_SENDERS which including DS leader
  PairOfNode dsLeader;
  if (!m_mediator.m_DSCommittee->empty()) {
    if (Node::GetDSLeader(m_mediator.m_blocklinkchain.GetLatestBlockLink(),
                          m_mediator.m_dsBlockChain.GetLastBlock(),
                          *m_mediator.m_DSCommittee, dsLeader)) {
      peerList.push_back(dsLeader.second);
    }
  }

  for (auto const& i : *m_mediator.m_DSCommittee) {
    if (peerList.size() < POW_PACKET_SENDERS && i.second != dsLeader.second) {
      peerList.push_back(i.second);
    }

    if (peerList.size() >= POW_PACKET_SENDERS) {
      break;
    }
  }

  zil::p2p::GetInstance().SendMessage(peerList, powmessage);
  return true;
}

bool Node::ReadVariablesFromStartPoWMessage(
    const zbytes& message, unsigned int cur_offset, uint64_t& block_num,
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
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "My IP address     = "
                << m_mediator.m_selfPeer.GetPrintableIPAddress());
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "My Listening Port = " << m_mediator.m_selfPeer.m_listenPortHost);
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "DS Difficulty        = " << to_string(ds_difficulty));
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "Difficulty        = " << to_string(difficulty));
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "Rand1             = " << DataConversion::charArrToHexStr(rand1));
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "Rand2             = " << DataConversion::charArrToHexStr(rand2));
LOG_EPOCH(INFO,m_mediator.m_currentEpochNum,
            "Pubkey            = " << DataConversion::SerializableToHexStr(
                m_mediator.m_selfKey.second));
  **/

  // DS nodes ip addr and port
  const unsigned int numDS =
      (message.size() - cur_offset) / (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE);

  // Create and keep a view of the DS committee
  // We'll need this if we win PoW
  m_mediator.m_DSCommittee->clear();
  LOG_GENERAL(INFO, "DS count = " << numDS);

  PubKey emptyPubKey;

  for (unsigned int i = 0; i < numDS; i++) {
    PubKey pubkey(message, cur_offset);
    cur_offset += PUB_KEY_SIZE;

    m_mediator.m_DSCommittee->emplace_back(
        make_pair(pubkey, Peer(message, cur_offset)));

    LOG_GENERAL(INFO, "[" << PAD(i, 3, ' ') << "] "
                          << m_mediator.m_DSCommittee->back().second);
    cur_offset += IP_SIZE + PORT_SIZE;
  }

  {
    lock_guard<mutex> g(m_mediator.m_mutexInitialDSCommittee);
    if (m_mediator.m_DSCommittee->size() !=
        m_mediator.m_initialDSCommittee->size()) {
      LOG_CHECK_FAIL("DS committee size", m_mediator.m_DSCommittee->size(),
                     m_mediator.m_initialDSCommittee->size());
    }
    unsigned int i = 0;
    for (auto& dsNode : *m_mediator.m_DSCommittee) {
      const auto& initialPubKey = m_mediator.m_initialDSCommittee->at(i);
      if (!(dsNode.first == initialPubKey)) {
        LOG_CHECK_FAIL("DS PubKey", dsNode.first, initialPubKey);
        if (dsNode.first == emptyPubKey) {
          dsNode.first = initialPubKey;
        }
      }
      i++;
    }
  }

  return true;
}

bool Node::ProcessStartPoW(const zbytes& message, unsigned int offset,
                           [[gnu::unused]] const Peer& from,
                           [[gnu::unused]] const unsigned char& startByte) {
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
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
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

  array<unsigned char, 32> rand1{};
  array<unsigned char, 32> rand2{};

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

  // Add ds guard to exclude list for new node
  Guard::GetInstance().AddDSGuardToBlacklistExcludeList(
      *m_mediator.m_DSCommittee);
  // Start mining
  StartPoW(block_num, dsDifficulty, difficulty, rand1, rand2);

  return true;
}
