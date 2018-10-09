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

#include <algorithm>
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Whitelist.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

bool DirectoryService::ProcessPoWSubmission(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessPoWSubmission not expected to be "
                "called from LookUp node.");
    return true;
  }

  if (m_state == FINALBLOCK_CONSENSUS) {
    std::unique_lock<std::mutex> cv_lk(m_MutexCVPOWSubmission);

    if (cv_POWSubmission.wait_for(
            cv_lk, std::chrono::seconds(POW_SUBMISSION_TIMEOUT)) ==
        std::cv_status::timeout) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Time out while waiting for state transition ");
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "State transition is completed. (check for timeout)");
  }

  if (!CheckState(PROCESS_POWSUBMISSION)) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Not at POW_SUBMISSION. Current state is " << m_state);
    return false;
  }

  uint64_t blockNumber = 0;
  uint8_t difficultyLevel = 0;
  Peer submitterPeer;
  PubKey submitterPubKey;
  uint64_t nonce = 0;
  string resultingHash;
  string mixHash;
  Signature signature;

  if (!Messenger::GetDSPoWSubmission(
          message, offset, blockNumber, difficultyLevel, submitterPeer,
          submitterPubKey, nonce, resultingHash, mixHash, signature)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSPoWSubmission failed.");
    return false;
  }

  // Check block number
  if (!CheckWhetherDSBlockIsFresh(blockNumber)) {
    return false;
  }

  if (TEST_NET_MODE && not Whitelist::GetInstance().IsNodeInDSWhiteList(
                           submitterPeer, submitterPubKey)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Submitted PoW but node is not in DS whitelist. Hence, "
              "not accepted!");
  }

  // Todo: Reject PoW submissions from existing members of DS committee

  if (!CheckState(VERIFYPOW)) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Too late - current state is "
                  << m_state
                  << ". Don't verify cause I have other work to do. "
                     "Assume true as it has no impact.");
    return true;
  }

  if (!Whitelist::GetInstance().IsValidIP(submitterPeer.m_ipAddress)) {
    LOG_GENERAL(WARNING,
                "IP belong to private ip subnet or is a broadcast address");
    return false;
  }

  if (CheckPoWSubmissionExceedsLimitsForNode(submitterPubKey)) {
    LOG_GENERAL(WARNING, submitterPeer << " has exceeded max pow submission");
    return false;
  }

  // Log all values
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Winner Public_key             = 0x"
                << DataConversion::SerializableToHexStr(submitterPubKey));
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Winner Peer ip addr           = " << submitterPeer);

  // Define the PoW parameters
  array<unsigned char, 32> rand1 = m_mediator.m_dsBlockRand;
  array<unsigned char, 32> rand2 = m_mediator.m_txBlockRand;

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "dsblock_num            = " << blockNumber);

  uint8_t expectedDSDiff = DS_POW_DIFFICULTY;
  uint8_t expectedDiff = POW_DIFFICULTY;

  // Non-genesis block
  if (blockNumber > 1) {
    expectedDSDiff =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
    expectedDiff =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
  }

  if (difficultyLevel != expectedDSDiff && difficultyLevel != expectedDiff) {
    LOG_GENERAL(WARNING, "Difficulty level is invalid. difficultyLevel: "
                             << to_string(difficultyLevel)
                             << " Expected: " << to_string(expectedDSDiff)
                             << " or " << to_string(expectedDiff));

    // TODO: penalise sender in reputation manager
    return false;
  }

  m_timespec = r_timer_start();

  bool result = POW::GetInstance().PoWVerify(
      blockNumber, difficultyLevel, rand1, rand2, submitterPeer.m_ipAddress,
      submitterPubKey, false, nonce, resultingHash, mixHash);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "[POWSTAT] pow verify (microsec): " << r_timer_end(m_timespec));

  if (result) {
    // Do another check on the state before accessing m_allPoWs
    // Accept slightly late entries as we need to multicast the DSBLOCK to
    // everyone if ((m_state != POW_SUBMISSION) && (m_state !=
    // DSBLOCK_CONSENSUS_PREP))
    if (!CheckState(VERIFYPOW)) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Too late - current state is " << m_state);
    } else {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "POW verification passed");
      lock(m_mutexAllPOW, m_mutexAllPoWConns);
      lock_guard<mutex> g(m_mutexAllPOW, adopt_lock);
      lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

      std::array<unsigned char, 32> winningHashArr =
          DataConversion::HexStrToStdArray(resultingHash);

      m_allPoWConns.emplace(submitterPubKey, submitterPeer);
      m_allPoWs[submitterPubKey] = winningHashArr;

      uint8_t expectedDSDiff = DS_POW_DIFFICULTY;
      if (blockNumber > 1) {
        expectedDSDiff = m_mediator.m_dsBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetDSDifficulty();
      }

      if (difficultyLevel == expectedDSDiff) {
        AddDSPoWs(submitterPubKey, winningHashArr);
      }

      UpdatePoWSubmissionCounterforNode(submitterPubKey);
    }
  } else {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Invalid PoW submission"
                  << "\n"
                  << "blockNum: " << blockNumber
                  << " Difficulty: " << to_string(difficultyLevel)
                  << " nonce: " << nonce << " ip: " << submitterPeer
                  << " rand1: " << DataConversion::charArrToHexStr(rand1)
                  << " rand2: " << DataConversion::charArrToHexStr(rand2));
  }

  return result;
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

void DirectoryService::AddDSPoWs(PubKey Pubk,
                                 std::array<unsigned char, 32> DSPOWSoln) {
  lock_guard<mutex> g(m_mutexAllDSPOWs);
  m_allDSPoWs[Pubk] = DSPOWSoln;
}

std::map<PubKey, std::array<unsigned char, 32>>
DirectoryService::GetAllDSPoWs() {
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
    return m_allDSPoWs[Pubk];
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

std::set<PubKey> DirectoryService::FindTopPriorityNodes() {
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
  }

  // Because the oldest DS commitee member still need to keep in the network as
  // shard node even it didn't do PoW, so also put it into the priority node
  // list.
  setTopPriorityNodes.insert(m_mediator.m_DSCommittee->back().first);
  return setTopPriorityNodes;
}
