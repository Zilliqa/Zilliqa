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

#include <algorithm>
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

bool DirectoryService::SendPoWPacketSubmissionToOtherDSComm() {
  LOG_MARKER();

  bytes powpacketmessage = {MessageType::DIRECTORY,
                            DSInstructionType::POWPACKETSUBMISSION};

  std::unique_lock<std::mutex> lk(m_mutexPowSolution);

  if (m_powSolutions.empty()) {
    LOG_GENERAL(INFO, "Didn't receive any pow submissions!!")
    return true;
  }

  if (!Messenger::SetDSPoWPacketSubmission(powpacketmessage,
                                           MessageOffset::BODY, m_powSolutions,
                                           m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetDSPoWPacketSubmission failed.");
    return false;
  }

  vector<Peer> peerList;

  if (BROADCAST_GOSSIP_MODE) {
    if (!P2PComm::GetInstance().SpreadRumor(powpacketmessage)) {
      LOG_GENERAL(INFO,
                  "Seems same packet was received by me from other DS member. "
                  "That's even better.")
      return true;
    }
  } else {
    // To-Do : not urgent , we used gossip mode for now.
    // check the powpacketmessage already received by me somehow.

    for (auto const& i : *m_mediator.m_DSCommittee) {
      peerList.push_back(i.second);
    }
    P2PComm::GetInstance().SendMessage(peerList, powpacketmessage);
  }

  return true;
}

bool DirectoryService::ProcessPoWPacketSubmission(
    const bytes& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "DirectoryService::ProcessPoWPacketSubmission not expected to be "
        "called from LookUp node.");
    return true;
  }

  std::vector<DSPowSolution> tmp;
  PubKey senderPubKey;
  if (!Messenger::GetDSPowPacketSubmission(message, offset, tmp,
                                           senderPubKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSPowPacketSubmission failed.");
    return false;
  }

  // check if sender pubkey is one from our expected list
  if (!CheckIfDSNode(senderPubKey)) {
    LOG_GENERAL(WARNING,
                "PubKey of packet sender "
                    << from
                    << " does not match any of the ds committee member");
    // In future, we may want to blacklist such node - TBD
    return false;
  }

  LOG_GENERAL(INFO, "PoW solutions received in this packet: " << tmp.size());
  for (auto& sol : tmp) {
    // No point processing the other solutions if DS Block consensus is starting
    if ((m_state == DSBLOCK_CONSENSUS_PREP) || (m_state == DSBLOCK_CONSENSUS)) {
      LOG_GENERAL(INFO, "Too late");
      break;
    }
    VerifyPoWSubmission(sol);
  }

  return true;
}

bool DirectoryService::ProcessPoWSubmission(const bytes& message,
                                            unsigned int offset,
                                            const Peer& from) {
  LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessPoWSubmission not expected to be "
                "called from LookUp node.");
    return true;
  }

  if ((m_consensusMyID >= POW_PACKET_SENDERS) && (m_mode != PRIMARY_DS)) {
    LOG_GENERAL(WARNING,
                "I am not supposed to receive individual pow submission. I "
                "accept only pow submission packets instead!!");
    return true;
  }

  uint64_t blockNumber;
  uint8_t difficultyLevel;
  Peer submitterPeer;
  PubKey submitterKey;
  uint64_t nonce;
  std::string resultingHash;
  std::string mixHash;
  uint32_t lookupId;
  uint128_t gasPrice;
  Signature signature;
  if (!Messenger::GetDSPoWSubmission(message, offset, blockNumber,
                                     difficultyLevel, submitterPeer,
                                     submitterKey, nonce, resultingHash,
                                     mixHash, signature, lookupId, gasPrice)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "DirectoryService::ProcessPowSubmission failed.");
    return false;
  }

  if (from.GetIpAddress() != submitterPeer.GetIpAddress()) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "The sender ip adress " << from.GetPrintableIPAddress()
                                      << " not match with address in message "
                                      << submitterPeer.GetPrintableIPAddress());
    return false;
  }

  if (resultingHash.size() != 64) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Wrong resultingHash size "
                  << resultingHash.size() << " submitted by "
                  << submitterPeer.GetPrintableIPAddress());
    return false;
  }

  if (mixHash.size() != 64) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Wrong mixHash size " << mixHash.size() << " submitted by "
                                    << submitterPeer.GetPrintableIPAddress());
    return false;
  }

  {
    std::unique_lock<std::mutex> lk(m_mutexPowSolution);
    auto submittedNumber =
        std::count_if(m_powSolutions.begin(), m_powSolutions.end(),
                      [&submitterKey](const DSPowSolution& soln) {
                        return submitterKey == soln.GetSubmitterKey();
                      });
    if (submittedNumber >= POW_SUBMISSION_LIMIT) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Node " << submitterKey
                        << " submitted pow count already reach limit");
      return false;
    }
  }

  DSPowSolution powSoln(blockNumber, difficultyLevel, submitterPeer,
                        submitterKey, nonce, resultingHash, mixHash, lookupId,
                        gasPrice, signature);

  if (VerifyPoWSubmission(powSoln)) {
    std::unique_lock<std::mutex> lk(m_mutexPowSolution);
    auto submittedNumber =
        std::count_if(m_powSolutions.begin(), m_powSolutions.end(),
                      [&submitterKey](const DSPowSolution& soln) {
                        return submitterKey == soln.GetSubmitterKey();
                      });
    if (submittedNumber >= POW_SUBMISSION_LIMIT) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                "Node " << submitterKey
                        << " submitted pow count already reach limit");
      return false;
    }
    m_powSolutions.emplace_back(powSoln);
  }

  return true;
}

bool DirectoryService::VerifyPoWSubmission(const DSPowSolution& sol) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::VerifyPoWSubmission not expected to be "
                "called from LookUp node.");
    return true;
  }

  if (m_state == FINALBLOCK_CONSENSUS) {
    std::unique_lock<std::mutex> cv_lk(m_MutexCVPOWSubmission);

    if (cv_POWSubmission.wait_for(
            cv_lk, std::chrono::seconds(POW_SUBMISSION_TIMEOUT)) ==
        std::cv_status::timeout) {
      LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum, "State wait timed out");
    }

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "State transition completed");
  }

  if (!CheckState(PROCESS_POWSUBMISSION)) {
    return false;
  }

  uint8_t difficultyLevel = sol.GetDifficultyLevel();
  uint64_t blockNumber = sol.GetBlockNumber();
  Peer submitterPeer = sol.GetSubmitterPeer();
  PubKey submitterPubKey = sol.GetSubmitterKey();
  uint64_t nonce = sol.GetNonce();
  string resultingHash = sol.GetResultingHash();
  string mixHash = sol.GetMixHash();
  Signature signature = sol.GetSignature();
  uint32_t lookupId = sol.GetLookupId();
  uint128_t gasPrice = sol.GetGasPrice();

  // Check block number
  if (!CheckWhetherDSBlockIsFresh(blockNumber)) {
    return false;
  }

  // Reject PoW submissions from existing members of DS committee
  if (!CheckSolnFromNonDSCommittee(submitterPubKey, submitterPeer)) {
    return false;
  }

  if (!CheckState(VERIFYPOW)) {
    return true;
  }

  if (!Guard::GetInstance().IsValidIP(submitterPeer.m_ipAddress)) {
    LOG_GENERAL(WARNING,
                "IP belong to private ip subnet or is a broadcast address");
    return false;
  }

  // Log all values
  LOG_GENERAL(INFO, "Key   = " << submitterPubKey);
  LOG_GENERAL(INFO, "Peer  = " << submitterPeer);
  LOG_GENERAL(INFO, "Diff  = " << to_string(difficultyLevel));

  if (CheckPoWSubmissionExceedsLimitsForNode(submitterPubKey)) {
    LOG_GENERAL(WARNING, "Max PoW sent");
    return false;
  }

  // Define the PoW parameters
  array<unsigned char, 32> rand1 = m_mediator.m_dsBlockRand;
  array<unsigned char, 32> rand2 = m_mediator.m_txBlockRand;

  LOG_GENERAL(INFO, "Block = " << blockNumber);

  uint8_t expectedDSDiff = DS_POW_DIFFICULTY;
  uint8_t expectedDiff = POW_DIFFICULTY;
  uint8_t expectedShardGuardDiff = POW_DIFFICULTY / POW_DIFFICULTY;

  // Non-genesis block
  if (blockNumber > 1) {
    expectedDSDiff =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
    expectedDiff =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
  }

  if (!GUARD_MODE) {
    if (difficultyLevel != expectedDSDiff && difficultyLevel != expectedDiff) {
      LOG_CHECK_FAIL("Difficulty level", to_string(difficultyLevel),
                     to_string(expectedDSDiff)
                         << " or " << to_string(expectedDiff));
      // TODO: penalise sender in reputation manager
      return false;
    }
  } else {
    bool difficultyCorrect = true;
    if (Guard::GetInstance().IsNodeInShardGuardList(submitterPubKey)) {
      if (difficultyLevel != expectedShardGuardDiff) {
        difficultyCorrect = false;
      }
    } else if (difficultyLevel != expectedDSDiff &&
               difficultyLevel != expectedDiff) {
      difficultyCorrect = false;
    }

    if (!difficultyCorrect) {
      LOG_CHECK_FAIL("Difficulty level", to_string(difficultyLevel),
                     to_string(expectedDSDiff)
                         << " or " << to_string(expectedDiff) << " or "
                         << to_string(expectedShardGuardDiff));
      // TODO: penalise sender in reputation manager
      return false;
    }
  }

  // m_timespec = r_timer_start();

  auto headerHash = POW::GenHeaderHash(rand1, rand2, submitterPeer.m_ipAddress,
                                       submitterPubKey, lookupId, gasPrice);
  bool result = POW::GetInstance().PoWVerify(
      blockNumber, difficultyLevel, headerHash, nonce, resultingHash, mixHash);

  // LOG_GENERAL(INFO, "[POWSTAT] " << r_timer_end(m_timespec));

  if (result) {
    // Do another check on the state before accessing m_allPoWs
    // Accept slightly late entries as we need to multicast the DSBLOCK to
    // everyone if ((m_state != POW_SUBMISSION) && (m_state !=
    // DSBLOCK_CONSENSUS_PREP))
    if (CheckState(VERIFYPOW)) {
      // LOG_GENERAL(INFO, "Verified OK");
      lock(m_mutexAllPOW, m_mutexAllPoWConns);
      lock_guard<mutex> g(m_mutexAllPOW, adopt_lock);
      lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

      array<uint8_t, 32> resultingHashArr, mixHashArr;
      DataConversion::HexStrToStdArray(resultingHash, resultingHashArr);
      DataConversion::HexStrToStdArray(mixHash, mixHashArr);
      PoWSolution soln(nonce, resultingHashArr, mixHashArr, lookupId, gasPrice);

      m_allPoWConns.emplace(submitterPubKey, submitterPeer);
      if (m_allPoWs.find(submitterPubKey) == m_allPoWs.end()) {
        m_allPoWs[submitterPubKey] = soln;
      } else if (m_allPoWs[submitterPubKey].result > soln.result) {
        // string harderSolnStr, oldSolnStr;
        // DataConversion::charArrToHexStr(soln.result, harderSolnStr);
        // DataConversion::charArrToHexStr(m_allPoWs[submitterPubKey].result,
        // oldSolnStr);
        LOG_GENERAL(INFO, "Replaced");
        m_allPoWs[submitterPubKey] = soln;
      } else if (m_allPoWs[submitterPubKey].result == soln.result) {
        LOG_GENERAL(INFO, "Duplicated");
        return true;
      }

      uint8_t expectedDSDiff = DS_POW_DIFFICULTY;
      if (blockNumber > 1) {
        expectedDSDiff = m_mediator.m_dsBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetDSDifficulty();
      }

      // Push the same solution into the DS PoW list if it qualifies
      if (difficultyLevel >= expectedDSDiff) {
        AddDSPoWs(submitterPubKey, soln);
      }

      UpdatePoWSubmissionCounterforNode(submitterPubKey);
    }
  } else {
    string rand1Str, rand2Str;
    DataConversion::charArrToHexStr(rand1, rand1Str);
    DataConversion::charArrToHexStr(rand2, rand2Str);
    LOG_GENERAL(INFO, "[Invalid PoW] Block: "
                          << blockNumber
                          << " Diff: " << to_string(difficultyLevel)
                          << " Nonce: " << nonce << " IP: " << submitterPeer
                          << " Rand1: " << rand1Str << " Rand2: " << rand2Str);
  }

  return result;
}

bool DirectoryService::CheckSolnFromNonDSCommittee(
    const PubKey& submitterPubKey, const Peer& submitterPeer) {
  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  for (const auto& dsMember : *m_mediator.m_DSCommittee) {
    // Reject soln if any of the following condition is true
    if (dsMember.first == submitterPubKey) {
      LOG_GENERAL(WARNING,
                  submitterPubKey
                      << " is part of the current DS committee. Soln sent from "
                      << submitterPeer);
      return false;
    }

    if (dsMember.second == submitterPeer) {
      LOG_GENERAL(WARNING,
                  submitterPeer << " is part of the current DS committee");
      return false;
    }
  }
  return true;
}

bool DirectoryService::CheckPoWSubmissionExceedsLimitsForNode(
    const PubKey& key) {
  lock_guard<mutex> g(m_mutexAllPoWCounter);
  if (m_AllPoWCounter.find(key) == m_AllPoWCounter.end()) {
    return false;
  } else if (m_AllPoWCounter[key] < POW_SUBMISSION_LIMIT) {
    return false;
  }
  return true;
}

void DirectoryService::UpdatePoWSubmissionCounterforNode(const PubKey& key) {
  lock_guard<mutex> g(m_mutexAllPoWCounter);

  if (m_AllPoWCounter.find(key) == m_AllPoWCounter.end()) {
    m_AllPoWCounter.emplace(key, 1);
  } else {
    m_AllPoWCounter[key] = m_AllPoWCounter[key] + 1;
  }
}

void DirectoryService::ResetPoWSubmissionCounter() {
  lock_guard<mutex> g(m_mutexAllPoWCounter);
  m_AllPoWCounter.clear();
}

void DirectoryService::AddDSPoWs(PubKey Pubk, const PoWSolution& DSPOWSoln) {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  m_allDSPoWs[Pubk] = DSPOWSoln;
}

MapOfPubKeyPoW DirectoryService::GetAllDSPoWs() {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  return m_allDSPoWs;
}

void DirectoryService::ClearDSPoWSolns() {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  m_allDSPoWs.clear();
}

std::array<unsigned char, 32> DirectoryService::GetDSPoWSoln(PubKey Pubk) {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  if (m_allDSPoWs.find(Pubk) != m_allDSPoWs.end()) {
    return m_allDSPoWs[Pubk].result;
  } else {
    LOG_GENERAL(WARNING, "No such element in m_allDSPoWs");
    return array<unsigned char, 32>();
  }
}

bool DirectoryService::IsNodeSubmittedDSPoWSoln(PubKey Pubk) {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  return m_allDSPoWs.find(Pubk) != m_allDSPoWs.end();
}

uint32_t DirectoryService::GetNumberOfDSPoWSolns() {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  return m_allDSPoWs.size();
}

/// Calculate node priority to determine which node has the priority to join the
/// network.
uint8_t DirectoryService::CalculateNodePriority(uint16_t reputation) {
  if (0 == reputation) {
    return 0;
  }
  return log2(reputation);
}

void DirectoryService::ClearReputationOfNodeWithoutPoW() {
  for (auto& kv : m_mapNodeReputation) {
    if (m_allPoWs.find(kv.first) == m_allPoWs.end()) {
      kv.second = 0;
    }
  }
}

void DirectoryService::ClearReputationOfNodeFailToJoin(
    const DequeOfShard& shards, std::map<PubKey, uint16_t>& mapNodeReputation) {
  std::set<PubKey> allShardNodePubKey;
  for (const auto& shard : shards) {
    for (const auto& shardNode : shard) {
      allShardNodePubKey.insert(std::get<SHARD_NODE_PUBKEY>(shardNode));
    }
  }

  for (auto& kv : mapNodeReputation) {
    if (allShardNodePubKey.find(kv.first) == allShardNodePubKey.end()) {
      kv.second = 0;
    }
  }
}

std::set<PubKey> DirectoryService::FindTopPriorityNodes(
    uint8_t& lowestPriority) {
  std::vector<std::pair<PubKey, uint8_t>> vecNodePriority;
  vecNodePriority.reserve(m_allPoWs.size());
  for (const auto& kv : m_allPoWs) {
    const auto& pubKey = kv.first;
    auto reputation = m_mapNodeReputation[pubKey];
    auto priority = CalculateNodePriority(reputation);
    vecNodePriority.emplace_back(pubKey, priority);
    LOG_GENERAL(INFO, "Node " << pubKey << " reputation " << reputation
                              << " priority " << std::to_string(priority));
  }

  std::sort(vecNodePriority.begin(), vecNodePriority.end(),
            [](const std::pair<PubKey, uint8_t>& kv1,
               const std::pair<PubKey, uint8_t>& kv2) {
              return kv1.second > kv2.second;
            });

  std::set<PubKey> setTopPriorityNodes;
  for (size_t i = 0; i < MAX_SHARD_NODE_NUM && i < vecNodePriority.size();
       ++i) {
    setTopPriorityNodes.insert(vecNodePriority[i].first);
    lowestPriority = vecNodePriority[i].second;
  }

  // Because the oldest DS commitee member still need to keep in the network as
  // shard node even it didn't do PoW, so also put it into the priority node
  // list.
  setTopPriorityNodes.insert(m_mediator.m_DSCommittee->back().first);
  return setTopPriorityNodes;
}
