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
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/RootComputation.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;
using namespace boost::multiprecision;

void DirectoryService::ExtractDataFromMicroblocks(
    vector<MicroBlockInfo>& mbInfos, uint64_t& allGasLimit,
    uint64_t& allGasUsed, uint128_t& allRewards, uint32_t& numTxs) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ExtractDataFromMicroblocks not expected "
                "to be called from LookUp node");
    return;
  }

  LOG_MARKER();

  unsigned int i = 1;

  vector<BlockHash> microblockHashes;

  {
    lock_guard<mutex> g(m_mutexMicroBlocks);

    auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];

    for (auto& microBlock : microBlocks) {
      LOG_STATE("[STATS][" << std::setw(15) << std::left
                           << m_mediator.m_selfPeer.GetPrintableIPAddress()
                           << "][" << i << "    ]["
                           << microBlock.GetHeader().GetNumTxs()
                           << "] PROPOSED");

      i++;

      LOG_GENERAL(INFO, "Pushback microblock shard ID: "
                            << microBlock.GetHeader().GetShardId() << endl
                            << "hash: " << microBlock.GetHeader().GetHashes());

      uint64_t tmpGasLimit = allGasLimit, tmpGasUsed = allGasUsed;
      uint128_t tmpRewards = allRewards;

      bool flag = true;
      if (!SafeMath<uint64_t>::add(
              allGasLimit, microBlock.GetHeader().GetGasLimit(), allGasLimit)) {
        flag = false;
      }
      if (flag &&
          !SafeMath<uint64_t>::add(
              allGasUsed, microBlock.GetHeader().GetGasUsed(), allGasUsed)) {
        flag = false;
      }
      if (flag &&
          !SafeMath<uint128_t>::add(
              allRewards, microBlock.GetHeader().GetRewards(), allRewards)) {
        flag = false;
      }
      if (!flag) {
        allGasLimit = tmpGasLimit;
        allGasUsed = tmpGasUsed;
        allRewards = tmpRewards;
      }

      numTxs += microBlock.GetHeader().GetNumTxs();

      mbInfos.push_back({microBlock.GetBlockHash(),
                         microBlock.GetHeader().GetTxRootHash(),
                         microBlock.GetHeader().GetShardId()});
    }
  }
}

bool DirectoryService::ComposeFinalBlock() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComposeFinalBlock not expected to "
                "be called from LookUp node");
    return true;
  }

  std::vector<MicroBlockInfo> mbInfos;
  std::vector<uint32_t> shardIds;
  uint32_t version = TXBLOCK_VERSION;
  uint64_t allGasLimit = 0;
  uint64_t allGasUsed = 0;
  uint128_t allRewards = 0;
  uint32_t numTxs = 0;
  std::vector<bool> isMicroBlockEmpty;
  StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

  ExtractDataFromMicroblocks(mbInfos, allGasLimit, allGasUsed, allRewards,
                             numTxs);

  // Compute the MBInfoHash of the MicroBlock information
  MBInfoHash mbInfoHash;
  if (!Messenger::GetMbInfoHash(mbInfos, mbInfoHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetMbInfoHash failed");
    return false;
  }

  BlockHash prevHash;

  uint64_t blockNum = 0;
  if (m_mediator.m_txBlockChain.GetBlockCount() > 0) {
    TxBlock lastBlock = m_mediator.m_txBlockChain.GetLastBlock();
    prevHash = lastBlock.GetBlockHash();

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Prev block hash as per leader " << prevHash.hex());
    blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
  }

  if (m_mediator.m_dsBlockChain.GetBlockCount() <= 0) {
    LOG_GENERAL(WARNING, "assertion failed (" << __FILE__ << ":" << __LINE__
                                              << ": " << __FUNCTION__ << ")");
    return false;
  }

  StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

#ifdef DM_TEST_DM_BAD_ANNOUNCE
  if (m_viewChangeCounter == 0) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Leader compose wrong state root (DM_TEST_DM_BAD_ANNOUNCE)");
    stateRoot = StateHash();
  }
#endif  // DM_TEST_DM_BAD_ANNOUNCE

  // Compute the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed");
    return false;
  }

  m_finalBlock.reset(new TxBlock(
      TxBlockHeader(
          allGasLimit, allGasUsed, allRewards, blockNum,
          {stateRoot, stateDeltaHash, mbInfoHash}, numTxs,
          m_mediator.m_selfKey.second,
          m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
          version, committeeHash, prevHash),
      mbInfos, CoSignatures(m_mediator.m_DSCommittee->size())));

  LOG_STATE(
      "[STATS]["
      << std::setw(15) << std::left
      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "][" << m_finalBlock->GetHeader().GetNumTxs() << "] FINAL");

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Final block Composed: " << *m_finalBlock);

  return true;
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary "
                "not expected to be called from LookUp node");
    return true;
  }

  // Compose the final block from all the microblocks
  // I guess only the leader has to do this
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am the leader DS node. Creating final block");

  if (!m_mediator.GetIsVacuousEpoch() &&
      ((m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty() >=
            TXN_SHARD_TARGET_DIFFICULTY &&
        m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetDSDifficulty() >= TXN_DS_TARGET_DIFFICULTY) ||
       m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
           TXN_DS_TARGET_NUM)) {
    m_mediator.m_node->ProcessTransactionWhenShardLeader();
    if (!AccountStore::GetInstance().SerializeDelta()) {
      LOG_GENERAL(WARNING, "AccountStore::SerializeDelta failed");
      return false;
    }
  }
  AccountStore::GetInstance().CommitTempRevertible();

  if (!m_mediator.m_node->ComposeMicroBlock()) {
    LOG_GENERAL(WARNING, "DS ComposeMicroBlock Failed");
    m_mediator.m_node->m_microblock = nullptr;
  } else {
    m_microBlocks[m_mediator.m_currentEpochNum].emplace(
        *(m_mediator.m_node->m_microblock));
  }

  // stores it in m_finalBlock
  if (!ComposeFinalBlock()) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "DirectoryService::RunConsensusOnFinalBlockWhenDSPrimary failed");
    return false;
  }

#ifdef VC_TEST_FB_SUSPEND_1
  if (m_mode == PRIMARY_DS && m_viewChangeCounter < 1) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_FB_SUSPEND_1)");
    return false;
  }
#endif  // VC_TEST_FB_SUSPEND_1

#ifdef VC_TEST_FB_SUSPEND_3
  if (m_mode == PRIMARY_DS && m_viewChangeCounter < 3) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_FB_SUSPEND_3)");
    return false;
  }
#endif  // VC_TEST_FB_SUSPEND_3

  // Create new consensus object
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto commitErrorFunc = [this](const bytes& errorMsg,
                                const Peer& from) mutable -> bool {
    return OnNodeFinalConsensusError(errorMsg, from);
  };

  m_consensusObject.reset(new ConsensusLeader(
      m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
      m_consensusBlockHash, m_consensusMyID, m_mediator.m_selfKey.first,
      *m_mediator.m_DSCommittee, static_cast<uint8_t>(DIRECTORY),
      static_cast<uint8_t>(FINALBLOCKCONSENSUS), commitErrorFunc,
      ShardCommitFailureHandlerFunc(), true));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  if (m_mode == PRIMARY_DS) {
    LOG_STATE(
        "[FBCON]["
        << setw(15) << left << m_mediator.m_selfPeer.GetPrintableIPAddress()
        << "]["
        << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() +
               1
        << "] BGIN");
  }

  auto announcementGeneratorFunc =
      [this](bytes& dst, unsigned int offset, const uint32_t consensusID,
             const uint64_t blockNumber, const bytes& blockHash,
             const uint16_t leaderID, const PairOfKey& leaderKey,
             bytes& messageToCosign) mutable -> bool {
    return Messenger::SetDSFinalBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_finalBlock, m_mediator.m_node->m_microblock, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);

  return true;
}

// Check version (must be most current version)
bool DirectoryService::CheckFinalBlockVersion() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckFinalBlockVersion not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  if (m_finalBlock->GetHeader().GetVersion() != TXBLOCK_VERSION) {
    LOG_CHECK_FAIL("TxBlock version", m_finalBlock->GetHeader().GetVersion(),
                   TXBLOCK_VERSION);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_FINALBLOCK_VERSION);

    return false;
  }

  return true;
}

// Check block number (must be = 1 + block number of last Tx block header in the
// Tx blockchain)
bool DirectoryService::CheckFinalBlockNumber() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckFinalBlockNumber not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  // Check block number
  if (!m_mediator.CheckWhetherBlockIsLatest(
          m_finalBlock->GetHeader().GetDSBlockNum() + 1,
          m_finalBlock->GetHeader().GetBlockNum())) {
    LOG_GENERAL(WARNING, "CheckWhetherBlockIsLatest failed");
    return false;
  }

  return true;
}

// Check previous hash (must be = sha2-256 digest of last Tx block header in the
// Tx blockchain)
bool DirectoryService::CheckPreviousFinalBlockHash() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckPreviousFinalBlockHash not "
                "expected to be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  const BlockHash& finalblockPrevHash = m_finalBlock->GetHeader().GetPrevHash();
  BlockHash expectedPrevHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash();

  if (finalblockPrevHash != expectedPrevHash) {
    LOG_CHECK_FAIL("Prev block hash", finalblockPrevHash, expectedPrevHash);
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_PREV_FINALBLOCK_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "Prev block hash OK = " << finalblockPrevHash.hex());

  return true;
}

// Check timestamp (must be greater than timestamp of last Tx block header in
// the Tx blockchain)
bool DirectoryService::CheckFinalBlockTimestamp() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckFinalBlockTimestamp not expected "
                "to be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  return VerifyTimestamp(m_finalBlock->GetTimestamp(),
                         CONSENSUS_OBJECT_TIMEOUT);
}

// Check microblock hashes
bool DirectoryService::CheckMicroBlocks(bytes& errorMsg, bool fromShards,
                                        bool generateErrorMsg) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckMicroBlocks not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexMicroBlocks);

    m_missingMicroBlocks[m_mediator.m_currentEpochNum].clear();
    // O(n^2) might be fine since number of shards is low
    // If its slow on benchmarking, may be first populate an unordered_set and
    // then std::find
    for (const auto& info : m_finalBlock->GetMicroBlockInfos()) {
      if (info.m_shardId == m_shards.size()) {
        continue;
      }

      BlockHash hash = info.m_microBlockHash;
      LOG_GENERAL(INFO, "MicroBlock hash = " << hash);
      bool found = false;
      auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
      for (auto& microBlock : microBlocks) {
        if (microBlock.GetBlockHash() == hash) {
          found = true;
          break;
        }
      }

      if (!found) {
        LOG_GENERAL(WARNING, "cannot find microblock with hash: " << hash);
        m_missingMicroBlocks[m_mediator.m_currentEpochNum].emplace_back(hash);
      }
    }
  }

  if (!m_missingMicroBlocks[m_mediator.m_currentEpochNum].empty()) {
    if (fromShards) {
      LOG_GENERAL(INFO, "Only check for microblocks from shards, failed");
      return false;
    }

    if (generateErrorMsg) {
      if (!Messenger::SetDSMissingMicroBlocksErrorMsg(
              errorMsg, 0, m_missingMicroBlocks[m_mediator.m_currentEpochNum],
              m_mediator.m_currentEpochNum,
              m_mediator.m_selfPeer.m_listenPortHost)) {
        LOG_GENERAL(WARNING,
                    "Messenger::SetDSMissingMicroBlocksErrorMsg failed");
        return false;
      }

      LOG_PAYLOAD(INFO, "ErrorMsg generated:", errorMsg, 200);
    }

    // AccountStore::GetInstance().InitTemp();
    // LOG_GENERAL(WARNING, "Got missing microblocks, revert state delta");
    // AccountStore::GetInstance().DeserializeDeltaTemp(
    //     m_mediator.m_ds->m_stateDeltaFromShards, 0);

    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::FINALBLOCK_MISSING_MICROBLOCKS);

    return false;
  }

  return true;
}

bool DirectoryService::CheckLegitimacyOfMicroBlocks() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckLegitimacyOfMicroBlocks not expected "
                "to be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  uint64_t allGasLimit = 0;
  uint64_t allGasUsed = 0;
  uint128_t allRewards = 0;
  uint32_t allNumTxns = 0;
  uint32_t allNumMicroBlockHashes = 0;

  {
    lock_guard<mutex> g(m_mutexMicroBlocks);

    auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
    for (auto& microBlock : microBlocks) {
      uint64_t tmpGasLimit = allGasLimit, tmpGasUsed = allGasUsed;
      uint128_t tmpRewards = allRewards;

      bool flag = true;
      if (!SafeMath<uint64_t>::add(
              allGasLimit, microBlock.GetHeader().GetGasLimit(), allGasLimit)) {
        flag = false;
      }
      if (flag &&
          !SafeMath<uint64_t>::add(
              allGasUsed, microBlock.GetHeader().GetGasUsed(), allGasUsed)) {
        flag = false;
      }
      if (flag &&
          !SafeMath<uint128_t>::add(
              allRewards, microBlock.GetHeader().GetRewards(), allRewards)) {
        flag = false;
      }
      if (!flag) {
        allGasLimit = tmpGasLimit;
        allGasUsed = tmpGasUsed;
        allRewards = tmpRewards;
      }

      allNumTxns += microBlock.GetHeader().GetNumTxs();
      ++allNumMicroBlockHashes;
    }
  }

  bool ret = true;

  if (allGasLimit != m_finalBlock->GetHeader().GetGasLimit()) {
    LOG_CHECK_FAIL("Gas limit", m_finalBlock->GetHeader().GetGasLimit(),
                   allGasLimit);
    // m_consensusObject->SetConsensusErrorCode(
    //     ConsensusCommon::FINALBLOCK_GASLIMIT_MISMATCH);
    // return false;
    ret = false;
  }

  if (ret && allGasUsed != m_finalBlock->GetHeader().GetGasUsed()) {
    LOG_CHECK_FAIL("Gas used", m_finalBlock->GetHeader().GetGasUsed(),
                   allGasUsed);
    // m_consensusObject->SetConsensusErrorCode(
    //     ConsensusCommon::FINALBLOCK_GASUSED_MISMATCH);
    // return false;
    ret = false;
  }

  if (ret && allRewards != m_finalBlock->GetHeader().GetRewards()) {
    LOG_CHECK_FAIL("Rewards", m_finalBlock->GetHeader().GetRewards(),
                   allRewards);
    // m_consensusObject->SetConsensusErrorCode(
    //     ConsensusCommon::FINALBLOCK_REWARDS_MISMATCH);
    // return false;
    ret = false;
  }

  if (ret && allNumTxns != m_finalBlock->GetHeader().GetNumTxs()) {
    LOG_CHECK_FAIL("Txn num", m_finalBlock->GetHeader().GetNumTxs(),
                   allNumTxns);
    // m_consensusObject->SetConsensusErrorCode(
    //     ConsensusCommon::FINALBLOCK_NUMTXNS_MISMATCH);
    // return false;
    ret = false;
  }

  if (ret &&
      allNumMicroBlockHashes != m_finalBlock->GetMicroBlockInfos().size()) {
    LOG_CHECK_FAIL("Num of MB hashes",
                   m_finalBlock->GetMicroBlockInfos().size(),
                   allNumMicroBlockHashes);
    // m_consensusObject->SetConsensusErrorCode(
    //     ConsensusCommon::FINALBLOCK_MBNUM_MISMATCH);
    // return false;
    ret = false;
  }

  if (!ret) {
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::FINALBLOCK_MBS_LEGITIMACY_ERROR);
  }

  return ret;
}

bool DirectoryService::OnNodeFinalConsensusError(const bytes& errorMsg,
                                                 const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::OnNodeFailFinalConsensus not expected "
                "to be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  if (errorMsg.size() < sizeof(uint8_t)) {
    LOG_GENERAL(WARNING, "Malformed Message");
    LOG_PAYLOAD(INFO, "errorMsg from " << from, errorMsg, 200);
    return false;
  }

  const unsigned char type = errorMsg[0];
  const unsigned int offset = sizeof(uint8_t);

  switch (type) {
    case FINALCONSENSUSERRORTYPE::CHECKMICROBLOCK: {
      LOG_GENERAL(INFO, "ErrorType: " << CHECKMICROBLOCK);
      return true;
    }
    case FINALCONSENSUSERRORTYPE::DSMBMISSINGTXN: {
      LOG_GENERAL(INFO, "ErrorType: " << CHECKMICROBLOCK);
      return m_mediator.m_node->OnNodeMissingTxns(errorMsg, offset, from);
    }
    case FINALCONSENSUSERRORTYPE::CHECKFINALBLOCK: {
      LOG_GENERAL(INFO, "ErrorType: " << CHECKMICROBLOCK);
      return true;
    }
    case FINALCONSENSUSERRORTYPE::DSFBMISSINGMB: {
      LOG_GENERAL(INFO, "ErrorType: " << CHECKMICROBLOCK);
      return OnNodeMissingMicroBlocks(errorMsg, offset, from);
    }
    default:
      LOG_GENERAL(WARNING, "Wrong Consensus Error Type: " << type);
      return false;
  }
}

bool DirectoryService::OnNodeMissingMicroBlocks(const bytes& errorMsg,
                                                const unsigned int offset,
                                                const Peer& from) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::OnNodeMissingMicroBlocks not expected "
                "to be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  vector<BlockHash> missingMicroBlocks;
  uint64_t epochNum = 0;
  uint32_t portNo = 0;

  if (!Messenger::GetDSMissingMicroBlocksErrorMsg(
          errorMsg, offset, missingMicroBlocks, epochNum, portNo)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSMissingMicroBlocksErrorMsg failed");
    return false;
  }

  Peer peer(from.m_ipAddress, portNo);

  lock_guard<mutex> g(m_mutexMicroBlocks);

  auto& microBlocks = m_microBlocks[epochNum];

  vector<MicroBlock> microBlocksSent;
  vector<bytes> stateDeltasSent;

  for (const auto& hash : missingMicroBlocks) {
    bool found = false;
    // O(n^2) might be fine since number of shards is low
    // If its slow on benchmarking, may be first populate an unordered_set and
    // then std::find
    auto microBlockIter = microBlocks.begin();
    for (; microBlockIter != microBlocks.end(); microBlockIter++) {
      if (microBlockIter->GetBlockHash() == hash) {
        found = true;
        break;
      }
    }

    if (microBlockIter->GetHeader().GetShardId() == m_shards.size()) {
      LOG_GENERAL(WARNING, "Ignore the fetching of DS microblock");
      continue;
    }

    if (!found) {
      LOG_GENERAL(WARNING,
                  "cannot find missing microblock: (hash)" << hash.hex());
      continue;
    }

    auto found_delta =
        m_microBlockStateDeltas[epochNum].find(microBlockIter->GetBlockHash());
    if (found_delta != m_microBlockStateDeltas[epochNum].end()) {
      stateDeltasSent.emplace_back(found_delta->second);
    } else {
      stateDeltasSent.push_back({});
    }

    microBlocksSent.emplace_back(*microBlockIter);
  }

  // // Final state delta
  // bytes stateDelta;
  // if (m_finalBlock->GetHeader().GetStateDeltaHash() != StateHash()) {
  //   AccountStore::GetInstance().GetSerializedDelta(stateDelta);
  // } else {
  //   LOG_GENERAL(INFO,
  //               "State Delta Hash is empty, skip sharing final state delta");
  // }

  bytes mb_message = {MessageType::DIRECTORY,
                      DSInstructionType::MICROBLOCKSUBMISSION};

  if (!Messenger::SetDSMicroBlockSubmission(
          mb_message, MessageOffset::BODY,
          DirectoryService::SUBMITMICROBLOCKTYPE::MISSINGMICROBLOCK, epochNum,
          microBlocksSent, stateDeltasSent, m_mediator.m_selfKey)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::SetDSMicroBlockSubmission failed");
    return false;
  }

  P2PComm::GetInstance().SendMessage(peer, mb_message);

  return true;
}

bool DirectoryService::CheckMicroBlockInfo() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckIsMicroBlockEmpty not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  const auto& microBlockInfos = m_finalBlock->GetMicroBlockInfos();

  LOG_GENERAL(INFO,
              "Total num of microblocks to check: " << microBlockInfos.size())

  for (unsigned int i = 0; i < microBlockInfos.size(); i++) {
    // LOG_GENERAL(INFO,
    //             "Microblock" << i
    //                 << "; shardId: " << shardIdsInMicroBlocks[i]
    //                 << "; hashes: " << hashesInMicroBlocks[i]
    //                 << "; IsMicroBlockEmpty: "
    //                 << m_finalBlock->GetIsMicroBlockEmpty()[i]);
    auto& microBlocks = m_microBlocks[m_mediator.m_currentEpochNum];
    for (auto& microBlock : microBlocks) {
      if (microBlock.GetBlockHash() == microBlockInfos.at(i).m_microBlockHash) {
        if (m_finalBlock->GetMicroBlockInfos().at(i).m_txnRootHash !=
            microBlock.GetHeader().GetTxRootHash()) {
          LOG_GENERAL(
              WARNING,
              "MicroBlockInfo::m_txnRootHash in proposed final block is "
              "incorrect"
                  << endl
                  << "MB Hash: " << microBlockInfos.at(i).m_microBlockHash
                  << endl
                  << "Expected: " << microBlock.GetHeader().GetTxRootHash()
                  << " Received: "
                  << m_finalBlock->GetMicroBlockInfos().at(i).m_txnRootHash);

          m_consensusObject->SetConsensusErrorCode(
              ConsensusCommon::FINALBLOCK_MICROBLOCK_TXNROOT_ERROR);

          return false;
        } else if (m_finalBlock->GetMicroBlockInfos().at(i).m_shardId !=
                   microBlock.GetHeader().GetShardId()) {
          LOG_GENERAL(
              WARNING,
              "ShardIds in proposed final block is incorrect"
                  << endl
                  << "MB Hash: " << microBlockInfos.at(i).m_microBlockHash
                  << endl
                  << "Expected: " << (microBlock.GetHeader().GetShardId() == 0)
                  << " Received: "
                  << m_finalBlock->GetMicroBlockInfos().at(i).m_shardId);
          return false;
        }
        break;
      }
    }
  }

  // Compute the MBInfoHash of the MicroBlock information
  MBInfoHash mbInfoHash;
  if (!Messenger::GetMbInfoHash(microBlockInfos, mbInfoHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetMbInfoHash failed");
    return false;
  }

  return mbInfoHash == m_finalBlock->GetHeader().GetMbInfoHash();
}

// Check state root
bool DirectoryService::CheckStateRoot() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckStateRoot not expected to be "
                "called from LookUp node");
    return true;
  }

  LOG_MARKER();

  StateHash stateRoot = AccountStore::GetInstance().GetStateRootHash();

  if (stateRoot != m_finalBlock->GetHeader().GetStateRootHash()) {
    LOG_CHECK_FAIL("State root hash",
                   m_finalBlock->GetHeader().GetStateRootHash(), stateRoot);
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_FINALBLOCK_STATE_ROOT);

    return false;
  }

  LOG_GENERAL(INFO, "State root hash  = " << stateRoot);

  return true;
}

bool DirectoryService::CheckStateDeltaHash() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckStateDeltaHash not expected to be "
                "called from LookUp node");
    return true;
  }

  LOG_MARKER();

  StateHash stateRootHash = AccountStore::GetInstance().GetStateDeltaHash();

  if (stateRootHash != m_finalBlock->GetHeader().GetStateDeltaHash()) {
    LOG_CHECK_FAIL("State delta hash",
                   m_finalBlock->GetHeader().GetStateDeltaHash(),
                   stateRootHash);
    m_consensusObject->SetConsensusErrorCode(
        ConsensusCommon::INVALID_FINALBLOCK_STATE_DELTA_HASH);

    return false;
  }

  LOG_GENERAL(INFO, "State delta hash = " << stateRootHash);

  return true;
}

bool DirectoryService::CheckBlockHash() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckBlockHash not expected to be "
                "called from LookUp node");
    return true;
  }

  LOG_MARKER();

  BlockHash temp_blockHash = m_finalBlock->GetHeader().GetMyHash();
  if (temp_blockHash != m_finalBlock->GetBlockHash()) {
    LOG_CHECK_FAIL("Block hash", m_finalBlock->GetBlockHash().hex(),
                   temp_blockHash);
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed");
    return false;
  }
  if (committeeHash != m_finalBlock->GetHeader().GetCommitteeHash()) {
    LOG_CHECK_FAIL("DS committee hash",
                   m_finalBlock->GetHeader().GetCommitteeHash(), committeeHash);
    return false;
  }

  return true;
}

bool DirectoryService::CheckFinalBlockValidity(bytes& errorMsg) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckFinalBlockValidity not expected to "
                "be called from LookUp node");
    return true;
  }

  return CheckBlockHash() && CheckFinalBlockVersion() &&
         CheckFinalBlockNumber() && CheckPreviousFinalBlockHash() &&
         CheckFinalBlockTimestamp() &&
         CheckMicroBlocks(errorMsg, false, true) &&
         CheckLegitimacyOfMicroBlocks() && CheckMicroBlockInfo() &&
         CheckStateRoot() && CheckStateDeltaHash();
}

bool DirectoryService::CheckMicroBlockValidity(bytes& errorMsg) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::CheckMicroBlockValidity not expected to "
                "be called from LookUp node");
    return true;
  }

  LOG_MARKER();

  bool ret = true;

  MicroBlockInfo mbInfo{
      m_mediator.m_node->m_microblock->GetBlockHash(),
      m_mediator.m_node->m_microblock->GetHeader().GetTxRootHash(),
      m_mediator.m_node->m_microblock->GetHeader().GetShardId()};

  // Check whether microblock is in TxBlock
  if (m_finalBlock->GetMicroBlockInfos().end() ==
      std::find(m_finalBlock->GetMicroBlockInfos().begin(),
                m_finalBlock->GetMicroBlockInfos().end(), mbInfo)) {
    LOG_GENERAL(WARNING, "Microblock attached is not found in finalblock");
    ret = false;
  }

  if (ret && !m_mediator.m_node->CheckMicroBlockValidity(errorMsg)) {
    LOG_GENERAL(WARNING, "Microblock validation failed");
    ret = false;
  }

  if (!ret) {
    m_mediator.m_node->m_microblock = nullptr;
  } else {
    m_microBlocks[m_mediator.m_currentEpochNum].emplace(
        *(m_mediator.m_node->m_microblock));
  }

  return ret;
}

bool DirectoryService::FinalBlockValidator(
    const bytes& message, unsigned int offset, bytes& errorMsg,
    const uint32_t consensusID, const uint64_t blockNumber,
    const bytes& blockHash, const uint16_t leaderID, const PubKey& leaderKey,
    bytes& messageToCosign) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::FinalBlockValidator not expected to be "
                "called from LookUp node");
    return true;
  }

  LOG_MARKER();

  m_finalBlock.reset(new TxBlock);

  m_mediator.m_node->m_microblock.reset(new MicroBlock());

  if (!Messenger::GetDSFinalBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_finalBlock, m_mediator.m_node->m_microblock,
          messageToCosign)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSFinalBlockAnnouncement failed");
    m_mediator.m_node->m_microblock = nullptr;
    return false;
  }

  bytes t_errorMsg;
  if (CheckMicroBlocks(t_errorMsg, true,
                       false)) {  // Firstly check whether the leader
                                  // has any mb that I don't have
    if (m_mediator.m_node->m_microblock != nullptr) {
      if (!CheckMicroBlockValidity(errorMsg)) {
        LOG_GENERAL(WARNING, "DS CheckMicroBlockValidity Failed");
        if (m_consensusObject->GetConsensusErrorCode() ==
            ConsensusCommon::MISSING_TXN) {
          errorMsg.insert(errorMsg.begin(), DSMBMISSINGTXN);
        } else {
          m_consensusObject->SetConsensusErrorCode(
              ConsensusCommon::INVALID_DS_MICROBLOCK);
          errorMsg.insert(errorMsg.begin(), CHECKMICROBLOCK);
        }
        return false;
      }
      AccountStore::GetInstance().SerializeDelta();
      AccountStore::GetInstance().CommitTempRevertible();
    }
  } else {
    m_mediator.m_node->m_microblock = nullptr;
    AccountStore::GetInstance().InitTemp();
    AccountStore::GetInstance().DeserializeDeltaTemp(m_stateDeltaFromShards, 0);
    AccountStore::GetInstance().SerializeDelta();
  }

  if (!CheckFinalBlockValidity(errorMsg)) {
    LOG_GENERAL(WARNING,
                "To-do: What to do if proposed finalblock is not valid?");
    if (m_consensusObject->GetConsensusErrorCode() ==
        ConsensusCommon::FINALBLOCK_MISSING_MICROBLOCKS) {
      errorMsg.insert(errorMsg.begin(), DSFBMISSINGMB);
    } else {
      errorMsg.insert(errorMsg.begin(), CHECKFINALBLOCK);
    }

    return false;
  }

  string finalblockPrevHashStr;
  if (!DataConversion::charArrToHexStr(
          m_finalBlock->GetHeader().GetPrevHash().asArray(),
          finalblockPrevHashStr)) {
    return false;
  }
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Final block " << m_finalBlock->GetHeader().GetBlockNum()
                           << " received with prevhash 0x"
                           << finalblockPrevHashStr);

  return true;
}

bool DirectoryService::RunConsensusOnFinalBlockWhenDSBackup() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnFinalBlockWhenDSBackup "
                "not expected to be called from LookUp node");
    return true;
  }

#ifdef VC_TEST_VC_PRECHECK_2
  uint64_t dsCurBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t txCurBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  // FIXME: Prechecking not working due at epoch 1 due to the way we have low
  // blocknum
  if (m_consensusMyID == 3 && dsCurBlockNum != 0 && txCurBlockNum > 10) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_VC_PRECHECK_2)");
    this_thread::sleep_for(chrono::seconds(45));
    return false;
  }
#endif  // VC_TEST_VC_PRECHECK_2
  if (!m_mediator.GetIsVacuousEpoch() &&
      ((m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty() >=
            TXN_SHARD_TARGET_DIFFICULTY &&
        m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetDSDifficulty() >= TXN_DS_TARGET_DIFFICULTY) ||
       m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
           TXN_DS_TARGET_NUM)) {
    m_mediator.m_node->ProcessTransactionWhenShardBackup();
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am a backup DS node. Waiting for final block announcement. "
            "Leader is at index  "
                << GetConsensusLeaderID() << " "
                << m_mediator.m_DSCommittee->at(GetConsensusLeaderID()).second
                << " my consensus id is " << m_consensusMyID);

  // Create new consensus object
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto func = [this](const bytes& input, unsigned int offset, bytes& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const bytes& blockHash, const uint16_t leaderID,
                     const PubKey& leaderKey,
                     bytes& messageToCosign) mutable -> bool {
    return FinalBlockValidator(input, offset, errorMsg, consensusID,
                               blockNumber, blockHash, leaderID, leaderKey,
                               messageToCosign);
  };

  m_consensusObject.reset(new ConsensusBackup(
      m_mediator.m_consensusID, m_mediator.m_currentEpochNum,
      m_consensusBlockHash, m_consensusMyID, GetConsensusLeaderID(),
      m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
      static_cast<uint8_t>(DIRECTORY),
      static_cast<uint8_t>(FINALBLOCKCONSENSUS), func));

  m_mediator.m_node->m_consensusObject = m_consensusObject;

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unable to create consensus object");
    return false;
  }

  return true;
}

void DirectoryService::PrepareRunConsensusOnFinalBlockNormal() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(
        WARNING,
        "DirectoryService::PrepareRunConsensusOnFinalBlockNormal not expected "
        "to be called from LookUp node");
    return;
  }

  LOG_MARKER();

  if (m_mediator.GetIsVacuousEpoch()) {
    LOG_EPOCH(
        INFO, m_mediator.m_currentEpochNum,
        "Vacuous epoch: Skipping submit transactions, and start InitCoinBase");
    m_mediator.m_node->CleanCreatedTransaction();
    // Coinbase
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum, "[CNBSE]");

    InitCoinbase();
    AccountStore::GetInstance().SerializeDelta();
  }
}

void DirectoryService::RunConsensusOnFinalBlock() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnFinalBlock not expected "
                "to be called from LookUp node");
    return;
  }

  {
    lock_guard<mutex> g(m_mutexRunConsensusOnFinalBlock);

    if (!(m_state == VIEWCHANGE_CONSENSUS || m_state == MICROBLOCK_SUBMISSION ||
          m_state == FINALBLOCK_CONSENSUS_PREP)) {
      LOG_GENERAL(WARNING,
                  "DirectoryService::RunConsensusOnFinalBlock "
                  "is not allowed in current state "
                      << m_state);
      return;
    }

#ifdef FALLBACK_TEST
    if (m_mediator.m_currentEpochNum == FALLBACK_TEST_EPOCH &&
        m_mediator.m_consensusID > 1) {
      LOG_GENERAL(INFO, "Stop DS for testing fallback");
      return;
    }
#endif  // FALLBACK_TEST

    if (m_doRejoinAtFinalConsensus) {
      RejoinAsDS();
    }

    if (m_state != FINALBLOCK_CONSENSUS_PREP) {
      SetState(FINALBLOCK_CONSENSUS_PREP);
    }

    m_mediator.m_node->PrepareGoodStateForFinalBlock();

    LOG_GENERAL(INFO, "RunConsensusOnFinalBlock ");
    PrepareRunConsensusOnFinalBlockNormal();

    // Upon consensus object creation failure, one should not return from the
    // function, but rather wait for view change.
    bool ConsensusObjCreation = true;
    if (m_mode == PRIMARY_DS) {
      this_thread::sleep_for(chrono::milliseconds(ANNOUNCEMENT_DELAY_IN_MS));
      ConsensusObjCreation = RunConsensusOnFinalBlockWhenDSPrimary();
      if (!ConsensusObjCreation) {
        LOG_GENERAL(WARNING,
                    "Consensus failed at "
                    "RunConsensusOnFinalBlockWhenDSPrimary");
      }
    } else {
      ConsensusObjCreation = RunConsensusOnFinalBlockWhenDSBackup();
      if (!ConsensusObjCreation) {
        LOG_GENERAL(WARNING,
                    "Consensus failed at "
                    "RunConsensusOnFinalBlockWhenDSBackup");
      }
    }

    if (ConsensusObjCreation) {
      SetState(FINALBLOCK_CONSENSUS);
    }

    m_startedRunFinalblockConsensus = true;

    auto func1 = [this]() -> void { CommitFinalBlockConsensusBuffer(); };

    DetachedFunction(1, func1);
  }

  auto func1 = [this]() -> void {
    // View change will wait for timeout. If conditional variable is notified
    // before timeout, the thread will return without triggering view change.
    std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeFinalBlock);
    if (cv_viewChangeFinalBlock.wait_for(
            cv_lk, std::chrono::seconds(VIEWCHANGE_TIME)) ==
        std::cv_status::timeout) {
      LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                "Initiated final block view change");

      if (m_mode == PRIMARY_DS) {
        ConsensusLeader* cl =
            dynamic_cast<ConsensusLeader*>(m_consensusObject.get());
        if (cl != nullptr) {
          cl->Audit();
        }
      }

      auto func2 = [this]() -> void {
        RemoveDSMicroBlock();  // Remove DS microblock from my list of
                               // microblocks
        RunConsensusOnViewChange();
      };
      DetachedFunction(1, func2);
    }
  };

  DetachedFunction(1, func1);
}

void DirectoryService::RemoveDSMicroBlock() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMicroBlocks);

  auto& microBlocksAtEpoch = m_microBlocks[m_mediator.m_currentEpochNum];
  auto dsmb = find_if(microBlocksAtEpoch.begin(), microBlocksAtEpoch.end(),
                      [this](const MicroBlock& mb) -> bool {
                        return mb.GetHeader().GetShardId() == m_shards.size();
                      });
  if (dsmb != microBlocksAtEpoch.end()) {
    LOG_GENERAL(INFO, "Removed DS microblock from list of microblocks");
    microBlocksAtEpoch.erase(dsmb);
  }

  m_mediator.m_node->m_microblock = nullptr;

  AccountStore::GetInstance().RevertCommitTemp();

  AccountStore::GetInstance().InitTemp();
  AccountStore::GetInstance().DeserializeDeltaTemp(m_stateDeltaFromShards, 0);
  AccountStore::GetInstance().SerializeDelta();
}
