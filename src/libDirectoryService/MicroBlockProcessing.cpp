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
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"

using namespace std;
using namespace boost::multiprecision;

bool DirectoryService::VerifyMicroBlockCoSignature(const MicroBlock& microBlock,
                                                   uint32_t shardId) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::VerifyMicroBlockCoSignature not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  const vector<bool>& B2 = microBlock.GetB2();
  vector<PubKey> keys;
  unsigned int index = 0;
  unsigned int count = 0;

  if (shardId == m_shards.size()) {
    if (m_mediator.m_DSCommittee->size() != B2.size()) {
      LOG_GENERAL(WARNING, "Mismatch: Shard(DS) size = "
                               << m_mediator.m_DSCommittee->size()
                               << ", co-sig bitmap size = " << B2.size());
      return false;
    }

    for (const auto& ds : *m_mediator.m_DSCommittee) {
      if (B2.at(index)) {
        keys.emplace_back(ds.first);
        count++;
      }
      index++;
    }
  } else {
    const auto& shard = m_shards.at(shardId);

    if (shard.size() != B2.size()) {
      LOG_GENERAL(WARNING, "Mismatch: Shard size = "
                               << shard.size()
                               << ", co-sig bitmap size = " << B2.size());
      return false;
    }

    // Generate the aggregated key
    for (const auto& kv : shard) {
      if (B2.at(index)) {
        keys.emplace_back(std::get<SHARD_NODE_PUBKEY>(kv));
        count++;
      }
      index++;
    }
  }

  if (count != ConsensusCommon::NumForConsensus(B2.size())) {
    LOG_GENERAL(WARNING, "Cosig was not generated by enough nodes");
    return false;
  }

  shared_ptr<PubKey> aggregatedKey = MultiSig::AggregatePubKeys(keys);
  if (aggregatedKey == nullptr) {
    LOG_GENERAL(WARNING, "Aggregated key generation failed");
    return false;
  }

  // Verify the collective signature
  vector<unsigned char> message;
  if (!microBlock.GetHeader().Serialize(message, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed");
    return false;
  }
  microBlock.GetCS1().Serialize(message, message.size());
  BitVector::SetBitVector(message, message.size(), microBlock.GetB1());
  if (!Schnorr::GetInstance().Verify(message, 0, message.size(),
                                     microBlock.GetCS2(), *aggregatedKey)) {
    LOG_GENERAL(WARNING, "Cosig verification failed");
    for (auto& kv : keys) {
      LOG_GENERAL(WARNING, kv);
    }
    return false;
  }

  return true;
}

bool DirectoryService::ProcessStateDelta(
    const vector<unsigned char>& stateDelta,
    const StateHash& microBlockStateDeltaHash,
    const BlockHash& microBlockHash) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessStateDelta not expected to be "
                "called from LookUp node.");
    return true;
  }

  LOG_GENERAL(INFO, "Received MicroBlock State Delta hash : "
                        << DataConversion::charArrToHexStr(
                               microBlockStateDeltaHash.asArray()));

  if (microBlockStateDeltaHash == StateHash()) {
    LOG_GENERAL(INFO,
                "State Delta hash received from microblock is null, "
                "skip processing state delta");
    return true;
  }

  if (stateDelta.empty()) {
    LOG_GENERAL(INFO, "State Delta is empty");
    return true;
  } else {
    LOG_GENERAL(INFO, "State Delta size: " << stateDelta.size());
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(stateDelta);
  StateHash stateDeltaHash(sha2.Finalize());

  LOG_GENERAL(INFO, "Calculated StateHash: " << stateDeltaHash);

  if (stateDeltaHash != microBlockStateDeltaHash) {
    LOG_GENERAL(WARNING,
                "State delta hash calculated does not match microblock");
    return false;
  }

  if (microBlockStateDeltaHash == StateHash()) {
    LOG_GENERAL(INFO, "State Delta from microblock is empty");
    return false;
  }

  if (!AccountStore::GetInstance().DeserializeDeltaTemp(stateDelta, 0)) {
    LOG_GENERAL(WARNING, "AccountStore::DeserializeDeltaTemp failed.");
    return false;
  }

  m_stateDeltaFromShards.clear();

  if (!AccountStore::GetInstance().SerializeDelta()) {
    LOG_GENERAL(WARNING, "AccountStore::SerializeDelta failed.");
    return false;
  }

  AccountStore::GetInstance().GetSerializedDelta(m_stateDeltaFromShards);

  m_microBlockStateDeltas[m_mediator.m_currentEpochNum].emplace(microBlockHash,
                                                                stateDelta);

  return true;
}

bool DirectoryService::ProcessMicroblockSubmissionFromShardCore(
    const MicroBlock& microBlock, const vector<unsigned char>& stateDelta) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessMicroblockSubmissionCore not "
                "expected to be called from LookUp node.");
    return true;
  }

  if (!m_mediator.CheckWhetherBlockIsLatest(
          microBlock.GetHeader().GetDSBlockNum() + 1,
          microBlock.GetHeader().GetEpochNum())) {
    LOG_GENERAL(WARNING,
                "ProcessMicroblockSubmissionFromShardCore "
                "CheckWhetherBlockIsLatest failed");
    return false;
  }

  uint32_t shardId = microBlock.GetHeader().GetShardId();
  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "shard_id " << shardId);

  const PubKey& pubKey = microBlock.GetHeader().GetMinerPubKey();

  // Check public key - shard ID mapping
  const auto& minerEntry = m_publicKeyToshardIdMap.find(pubKey);
  if (minerEntry == m_publicKeyToshardIdMap.end()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Cannot find the miner key: "
                  << DataConversion::SerializableToHexStr(pubKey));
    return false;
  }
  if (minerEntry->second != shardId) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Microblock shard ID mismatch");
    return false;
  }

  CommitteeHash committeeHash;
  if (!Messenger::GetShardHash(m_shards.at(shardId), committeeHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetShardHash failed.");
    return false;
  }
  if (committeeHash != microBlock.GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING, "Microblock committee hash mismatched"
                             << endl
                             << "expected: " << committeeHash << endl
                             << "received: "
                             << microBlock.GetHeader().GetCommitteeHash());
    return false;
  }

  // Verify the co-signature
  if (!VerifyMicroBlockCoSignature(microBlock, shardId)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Microblock co-sig verification failed");
    return false;
  }

  LOG_GENERAL(INFO, "MicroBlock StateDeltaHash: "
                        << microBlock.GetHeader().GetHashes());

  lock_guard<mutex> g(m_mutexMicroBlocks);

  if (m_stopRecvNewMBSubmission) {
    LOG_GENERAL(WARNING,
                "DS microblock consensus already started, ignore this "
                "microblock submission");
    return false;
  }

  if (!SaveCoinbase(microBlock.GetB1(), microBlock.GetB2(),
                    microBlock.GetHeader().GetShardId(),
                    m_mediator.m_currentEpochNum)) {
    return false;
  }

  vector<unsigned char> body;
  microBlock.Serialize(body, 0);
  if (!BlockStorage::GetBlockStorage().PutMicroBlock(microBlock.GetBlockHash(),
                                                     body)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in persistence");
  }

  if (!m_mediator.GetIsVacuousEpoch()) {
    if (!ProcessStateDelta(stateDelta,
                           microBlock.GetHeader().GetStateDeltaHash(),
                           microBlock.GetBlockHash())) {
      LOG_GENERAL(WARNING, "State delta attached to the microblock is invalid");
      return false;
    }
  }

  auto& microBlocksAtEpoch = m_microBlocks[m_mediator.m_currentEpochNum];
  microBlocksAtEpoch.emplace(microBlock);

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            microBlocksAtEpoch.size()
                << " of " << m_shards.size() << " microblocks received");

  if (microBlocksAtEpoch.size() == m_shards.size()) {
    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum
                         << "] LAST RECVD");
    LOG_STATE("[MIBLKSWAIT[" << setw(15) << left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << m_mediator.m_currentEpochNum
                             << "] DONE");

    for (auto& mb : microBlocksAtEpoch) {
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Timestamp: " << mb.GetHeader().GetTimestamp()
                              << mb.GetHeader().GetStateDeltaHash());
    }

    m_stopRecvNewMBSubmission = true;
    cv_scheduleDSMicroBlockConsensus.notify_all();

    auto func = [this]() mutable -> void { RunConsensusOnFinalBlock(); };

    DetachedFunction(1, func);
  } else {
    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum
                         << "] FRST RECVD");
  }

  // TODO: Re-request from shard leader if microblock is not received after a
  // certain time.
  return true;
}

void DirectoryService::CommitMBSubmissionMsgBuffer() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexMBSubmissionBuffer);

  for (auto it = m_MBSubmissionBuffer.begin();
       it != m_MBSubmissionBuffer.end();) {
    if (it->first < m_mediator.m_currentEpochNum) {
      it = m_MBSubmissionBuffer.erase(it);
    } else if (it->first == m_mediator.m_currentEpochNum) {
      for (const auto& entry : it->second) {
        ProcessMicroblockSubmissionFromShardCore(entry.m_microBlock,
                                                 entry.m_stateDelta);
      }
      m_MBSubmissionBuffer.erase(it);
      break;
    } else {
      it++;
    }
  }
}

bool DirectoryService::ProcessMicroblockSubmissionFromShard(
    const uint64_t epochNumber, const vector<MicroBlock>& microBlocks,
    const vector<vector<unsigned char>>& stateDeltas) {
  LOG_MARKER();

#ifdef DM_TEST_DM_LESSMB_ONE
  if (m_mediator.m_ds->m_consensusMyID ==
      ((m_mediator.m_ds->m_consensusLeaderID + 1) %
       m_mediator.m_DSCommittee->size())) {
    LOG_GENERAL(WARNING,
                "Letting one of the backups refuse some Microblock submission "
                "(DM_TEST_DM_LESSMB_ONE)");
    return false;
  }
#endif  // DM_TEST_DM_LESSMB_ONE

#ifdef DM_TEST_DM_LESSMB_ALL
  if (m_mediator.m_ds->m_mode == BACKUP_DS) {
    LOG_GENERAL(WARNING,
                "Letting all of the backups refuse some Microblock submission "
                "(DM_TEST_DM_LESSMB_ALL)");
    return false;
  }
#endif  // DM_TEST_DM_LESSMB_ALL

  LOG_GENERAL(
      INFO, "Received microblock submission for epoch number " << epochNumber);

  if (microBlocks.empty()) {
    LOG_GENERAL(WARNING, "MicroBlocks received is empty");
    return false;
  }

  if (stateDeltas.empty()) {
    LOG_GENERAL(WARNING, "StateDeltas received is empty");
    return false;
  }

  const auto& microBlock = microBlocks.at(0);
  const auto& stateDelta = stateDeltas.at(0);

  if (m_mediator.m_currentEpochNum < epochNumber) {
    lock_guard<mutex> g(m_mutexMBSubmissionBuffer);
    m_MBSubmissionBuffer[epochNumber].emplace_back(microBlock, stateDelta);

    return true;
  } else if (m_mediator.m_currentEpochNum == epochNumber) {
    if (CheckState(PROCESS_MICROBLOCKSUBMISSION)) {
      return ProcessMicroblockSubmissionFromShardCore(microBlock, stateDelta);
    } else {
      lock_guard<mutex> g(m_mutexMBSubmissionBuffer);
      m_MBSubmissionBuffer[epochNumber].emplace_back(microBlock, stateDelta);

      return true;
    }
  }

  LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
            "This microblock submission is too late");

  return false;
}

bool DirectoryService::ProcessMicroblockSubmission(
    [[gnu::unused]] const vector<unsigned char>& message,
    [[gnu::unused]] unsigned int offset, [[gnu::unused]] const Peer& from) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessMicroblockSubmission not "
                "expected to be called from LookUp node.");
    return true;
  }

  unsigned char submitMBType = 0;
  uint64_t epochNumber = 0;
  vector<MicroBlock> microBlocks;
  vector<vector<unsigned char>> stateDeltas;

  if (!Messenger::GetDSMicroBlockSubmission(message, offset, submitMBType,
                                            epochNumber, microBlocks,
                                            stateDeltas)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSMicroBlockSubmission failed.");
    return false;
  }

  if (submitMBType == SUBMITMICROBLOCKTYPE::SHARDMICROBLOCK) {
    return ProcessMicroblockSubmissionFromShard(epochNumber, microBlocks,
                                                stateDeltas);
  } else if (submitMBType == SUBMITMICROBLOCKTYPE::MISSINGMICROBLOCK) {
    return ProcessMissingMicroblockSubmission(epochNumber, microBlocks,
                                              stateDeltas);
  } else {
    LOG_GENERAL(WARNING, "Malformed message");
  }

  return false;
}

bool DirectoryService::ProcessMissingMicroblockSubmission(
    const uint64_t epochNumber, const vector<MicroBlock>& microBlocks,
    const vector<vector<unsigned char>>& stateDeltas) {
  if (epochNumber != m_mediator.m_currentEpochNum) {
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "untimely delivery of "
                  << "missing microblocks. received: " << epochNumber
                  << " , local: " << m_mediator.m_currentEpochNum);
  }

  {
    lock_guard<mutex> g(m_mutexMicroBlocks);
    auto& microBlocksAtEpoch = m_microBlocks[epochNumber];

    if (microBlocks.size() != stateDeltas.size()) {
      LOG_GENERAL(WARNING, "size of microBlocks fetched "
                               << microBlocks.size()
                               << " is different from size of "
                                  "stateDeltas fetched "
                               << stateDeltas.size());
      return false;
    }

    for (unsigned int i = 0; i < microBlocks.size(); ++i) {
      if (!m_mediator.CheckWhetherBlockIsLatest(
              microBlocks.at(i).GetHeader().GetDSBlockNum() + 1,
              microBlocks.at(i).GetHeader().GetEpochNum())) {
        LOG_GENERAL(WARNING,
                    "ProcessMissingMicroblockSubmission "
                    "CheckWhetherBlockIsLatest failed");
        return false;
      }

      uint32_t shardId = microBlocks.at(i).GetHeader().GetShardId();
      LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                "shard_id: " << shardId << ", pubkey: "
                             << microBlocks.at(i).GetHeader().GetMinerPubKey());

      const PubKey& pubKey = microBlocks.at(i).GetHeader().GetMinerPubKey();

      // Check public key - shard ID mapping
      if (shardId == m_shards.size()) {
        // DS shard
        bool found = false;
        for (const auto& ds : *m_mediator.m_DSCommittee) {
          if (ds.first == pubKey) {
            found = true;
            break;
          }
        }
        if (!found) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Cannot find the miner key in DS committee: "
                        << DataConversion::SerializableToHexStr(pubKey));
          continue;
        }
      } else {
        // normal shard
        const auto& minerEntry = m_publicKeyToshardIdMap.find(pubKey);
        if (minerEntry == m_publicKeyToshardIdMap.end()) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Cannot find the miner key in normal shard: "
                        << DataConversion::SerializableToHexStr(pubKey));
          continue;
        }
        if (minerEntry->second != shardId) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Microblock shard ID mismatch");
          continue;
        }
      }

      // Verify the co-signature
      if (shardId != m_mediator.m_node->m_myshardId) {
        if (!VerifyMicroBlockCoSignature(microBlocks[i], shardId)) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Microblock co-sig verification failed");
          continue;
        }
      }

      {
        // Check whether the fetched microblock is in missing microblocks list
        bool found = false;
        const auto& missingMBHashes = m_missingMicroBlocks[epochNumber];
        for (const auto& missingMBHash : missingMBHashes) {
          if (missingMBHash == microBlocks[i].GetBlockHash()) {
            found = true;
            break;
          }
        }
        if (!found) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Microblock fetched is not in missing list");
          continue;
        }
      }

      {
        // Check whether already have the microblock
        bool found = false;
        const auto& myMicroBlocks = m_microBlocks[epochNumber];
        for (const auto& myMicroBlock : myMicroBlocks) {
          if (myMicroBlock.GetBlockHash() == microBlocks[i].GetBlockHash()) {
            found = true;
            break;
          }
        }
        if (found) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "Microblock already exists in local");
          continue;
        }
      }

      LOG_GENERAL(INFO, "MicroBlock Hash: "
                            << microBlocks.at(i).GetHeader().GetHashes());

      if (microBlocks.at(i).GetHeader().GetShardId() != m_shards.size()) {
        if (!SaveCoinbase(microBlocks[i].GetB1(), microBlocks[i].GetB2(),
                          microBlocks[i].GetHeader().GetShardId(),
                          m_mediator.m_currentEpochNum)) {
          continue;
        }
      }

      if (!m_mediator.GetIsVacuousEpoch(epochNumber)) {
        if (!ProcessStateDelta(
                stateDeltas.at(i),
                microBlocks.at(i).GetHeader().GetStateDeltaHash(),
                microBlocks.at(i).GetBlockHash())) {
          LOG_GENERAL(WARNING,
                      "State delta attached to the microblock is invalid");
          continue;
        }
      }

      vector<unsigned char> body;
      microBlocks[i].Serialize(body, 0);
      if (!BlockStorage::GetBlockStorage().PutMicroBlock(
              microBlocks[i].GetBlockHash(), body)) {
        LOG_GENERAL(WARNING, "Failed to put microblock in persistence");
      }

      microBlocksAtEpoch.emplace(microBlocks.at(i));
      // m_fetchedMicroBlocks.emplace(microBlock);

      LOG_GENERAL(INFO, microBlocksAtEpoch.size()
                            << " of " << m_shards.size()
                            << " microblocks received for Epoch "
                            << epochNumber);
    }
  }

  // TODO: Check if every microblock is obtained
  std::vector<unsigned char> errorMsg;
  if (!CheckMicroBlocks(errorMsg)) {
    LOG_GENERAL(WARNING,
                "Still have missing microblocks after fetching, what to do???");
    return false;
  }

  cv_MissingMicroBlock.notify_all();
  return true;
}
