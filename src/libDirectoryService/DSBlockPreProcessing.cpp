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

#include "DSComposition.h"
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
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/ShardSizeCalculator.h"
#include "libUtils/TimestampVerifier.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int DirectoryService::ComputeDSBlockParameters(
    const VectorOfPoWSoln& sortedDSPoWSolns, map<PubKey, Peer>& powDSWinners,
    MapOfPubKeyPoW& dsWinnerPoWs, uint8_t& dsDifficulty, uint8_t& difficulty,
    uint64_t& blockNum, BlockHash& prevHash) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComputeDSBlockParameters not expected to be "
                "called from LookUp node.");
    return 0;
  }

  // Assemble DS block header
  unsigned int numOfElectedDSMembers =
      min(sortedDSPoWSolns.size(), (size_t)NUM_DS_ELECTION);
  unsigned int counter = 0;
  for (const auto& submitter : sortedDSPoWSolns) {
    if (counter >= numOfElectedDSMembers) {
      break;
    }
    powDSWinners[submitter.second] = m_allPoWConns[submitter.second];
    dsWinnerPoWs[submitter.second] = m_allDSPoWs[submitter.second];
    counter++;
  }
  if (sortedDSPoWSolns.size() == 0) {
    LOG_GENERAL(WARNING, "No soln met the DS difficulty level");
    // TODO: To handle if no PoW soln can meet DS difficulty level.
  }

  blockNum = 0;
  dsDifficulty = DS_POW_DIFFICULTY;
  difficulty = POW_DIFFICULTY;
  auto lastBlockLink = m_mediator.m_blocklinkchain.GetLatestBlockLink();
  if (m_mediator.m_dsBlockChain.GetBlockCount() > 0) {
    DSBlock lastBlock = m_mediator.m_dsBlockChain.GetLastBlock();
    blockNum = lastBlock.GetHeader().GetBlockNum() + 1;
    prevHash = get<BlockLinkIndex::BLOCKHASH>(lastBlockLink);

    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Prev DS block hash as per leader " << prevHash.hex());
  }

  // Start to adjust difficulty from second DS block.
  if (blockNum > 1) {
    dsDifficulty = CalculateNewDSDifficulty(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty());
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Current DS difficulty "
                  << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetDSDifficulty())
                  << ", new DS difficulty " << std::to_string(dsDifficulty));

    difficulty = CalculateNewDifficulty(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty());
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Current difficulty "
                  << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetDifficulty())
                  << ", new difficulty " << std::to_string(difficulty));
  }

  return numOfElectedDSMembers;
}

void DirectoryService::ComputeSharding(const VectorOfPoWSoln& sortedPoWSolns) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComputeSharding not expected to be "
                "called from LookUp node.");
    return;
  }

  LOG_MARKER();

  m_shards.clear();
  m_publicKeyToshardIdMap.clear();

  // Cap the number of nodes based on MAX_SHARD_NODE_NUM
  const uint32_t numNodesForSharding =
      sortedPoWSolns.size() > MAX_SHARD_NODE_NUM ? MAX_SHARD_NODE_NUM
                                                 : sortedPoWSolns.size();

  LOG_GENERAL(INFO, "Number of PoWs received     = " << sortedPoWSolns.size());
  LOG_GENERAL(INFO, "Number of PoWs for sharding = " << numNodesForSharding);

  const uint32_t shardSize = m_mediator.GetShardSize(false);

  // Generate the number of shards and node counts per shard
  vector<uint32_t> shardCounts;
  ShardSizeCalculator::GenerateShardCounts(shardSize, SHARD_SIZE_TOLERANCE_LO,
                                           SHARD_SIZE_TOLERANCE_HI,
                                           numNodesForSharding, shardCounts);

  // Abort if zero shards generated
  if (shardCounts.empty()) {
    LOG_GENERAL(WARNING, "Zero shards generated");
    return;
  }

  // Resize the shard map to the generated number of shards
  for (unsigned int i = 0; i < shardCounts.size(); i++) {
    m_shards.emplace_back();
  }

  // Push all the sorted PoW submissions into an ordered map with key =
  // H(last_block_hash, pow_hash)
  map<array<unsigned char, BLOCK_HASH_SIZE>, PubKey> sortedPoWs;
  bytes lastBlockHash(BLOCK_HASH_SIZE);

  if (m_mediator.m_currentEpochNum > 1) {
    lastBlockHash =
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();
  }

  bytes hashVec(BLOCK_HASH_SIZE + POW_SIZE);
  copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());
  for (const auto& kv : sortedPoWSolns) {
    const PubKey& key = kv.second;
    const auto& powHash = kv.first;
    copy(powHash.begin(), powHash.end(), hashVec.begin() + BLOCK_HASH_SIZE);

    const bytes& sortHashVec = HashUtils::BytesToHash(hashVec);
    array<unsigned char, BLOCK_HASH_SIZE> sortHash{};
    copy(sortHashVec.begin(), sortHashVec.end(), sortHash.begin());
    sortedPoWs.emplace(sortHash, key);
  }

  // Distribute the map-ordered nodes among the generated shards
  // First fill up first shard, then second shard, ..., then final shard
  uint32_t shard_index = 0;
  for (const auto& kv : sortedPoWs) {
    // Move to next shard counter if current shard already filled up
    if (shardCounts.at(shard_index) == 0) {
      shard_index++;
      // Stop if all shards filled up
      if (shard_index == shardCounts.size()) {
        break;
      }
    }
    if (DEBUG_LEVEL >= 5) {
      string hashStr;
      if (!DataConversion::charArrToHexStr(kv.first, hashStr)) {
        LOG_GENERAL(WARNING, "[DSSORT] "
                                 << " unable to convert hash to string");
      } else {
        LOG_GENERAL(INFO, "[DSSORT] " << kv.second << " " << hashStr << endl);
      }
    }
    // Put the node into the shard
    const PubKey& key = kv.second;
    m_shards.at(shard_index)
        .emplace_back(key, m_allPoWConns.at(key), m_mapNodeReputation[key]);
    m_publicKeyToshardIdMap.emplace(key, shard_index);

    // Decrement remaining count for this shard
    shardCounts.at(shard_index)--;
  }
}

void DirectoryService::InjectPoWForDSNode(
    VectorOfPoWSoln& sortedPoWSolns, unsigned int numOfProposedDSMembers,
    const std::vector<PubKey>& removeDSNodePubkeys) {
  LOG_MARKER();

  unsigned int numOfRemovedMembers = removeDSNodePubkeys.size();
  unsigned int numOfExpiring = numOfProposedDSMembers - numOfRemovedMembers;

  // Check the computed parameters for correctness.
  if (numOfProposedDSMembers > m_mediator.m_DSCommittee->size()) {
    LOG_GENERAL(WARNING,
                "FATAL: number of proposed ds member is larger than current ds "
                "committee. numOfProposedDSMembers: "
                    << numOfProposedDSMembers << " m_DSCommittee size: "
                    << m_mediator.m_DSCommittee->size());
    return;
  }

  // the number of removed members for non-performance has to be strictly less
  // than the total number of new incoming members because the field only
  // contains members that were removed for non-performance and not the expired
  // ones.
  if (numOfRemovedMembers > numOfProposedDSMembers) {
    LOG_GENERAL(WARNING,
                "FATAL: number of ds members to be removed is larger than the "
                "number of proposed ds members. numOfRemovedMembers: "
                    << numOfRemovedMembers
                    << " numOfProposedDSMembers: " << numOfProposedDSMembers);
    return;
  }

  // Add the oldest n DS committee member to m_allPoWs and m_allPoWConns so it
  // gets included in sharding structure
  SHA2<HashType::HASH_VARIANT_256> sha2;
  bytes serializedPubK;

  // Iterate through the current DS committee from the back, add a PoW
  // solution for the expiring nodes.
  unsigned int counter = 0;

  for (auto rit = m_mediator.m_DSCommittee->rbegin();
       rit != m_mediator.m_DSCommittee->rend(); ++rit) {
    // Only inject up to the number of benign expiring nodes.
    if (counter >= numOfExpiring) {
      break;
    }

    // Check if the current member is a node to be removed.
    if (std::find(removeDSNodePubkeys.begin(), removeDSNodePubkeys.end(),
                  rit->first) != removeDSNodePubkeys.end()) {
      // If it is, continue onto the next member.
      continue;
    }

    // TODO: Revise this as this is rather ad hoc. Currently, it is SHA2(PubK)
    // to act as the PoW soln
    PubKey nodePubKey = rit->first;
    nodePubKey.Serialize(serializedPubK, 0);
    sha2.Update(serializedPubK);
    bytes PubKeyHash;
    PubKeyHash = sha2.Finalize();
    array<unsigned char, 32> PubKeyHashArr{};

    // Injecting into sorted PoWs
    copy(PubKeyHash.begin(), PubKeyHash.end(), PubKeyHashArr.begin());

    // Check whether injected node submit soln (maliciously)
    // This could happen if the node rejoin as a normal shard node by submitting
    // PoW and DS committee injected it
    bool isDupPubKey = false;
    for (const auto& soln : sortedPoWSolns) {
      if (soln.second == nodePubKey) {
        LOG_GENERAL(WARNING,
                    "Injected node also submitted a soln. " << rit->second);
        isDupPubKey = true;
        break;
      }
    }

    // Skip the injection for this node if it is duplicated
    if (isDupPubKey) {
      continue;
    }

    sortedPoWSolns.emplace_back(PubKeyHashArr, nodePubKey);
    sha2.Reset();
    serializedPubK.clear();

    // Injecting into Pow Connections information
    if (rit->second == Peer()) {
      m_allPoWConns.emplace(m_mediator.m_selfKey.second, m_mediator.m_selfPeer);
      LOG_GENERAL(INFO,
                  "Injecting into PoW connections " << m_mediator.m_selfPeer);
    } else {
      m_allPoWConns.emplace(*rit);
      LOG_GENERAL(INFO, "Injecting into PoW connections " << rit->second);
    }

    ++counter;
  }

  LOG_GENERAL(INFO, "Num PoWs after injection = " << sortedPoWSolns.size());
}

bool DirectoryService::VerifyPoWWinner(
    const MapOfPubKeyPoW& dsWinnerPoWsFromLeader) {
  LOG_MARKER();

  // Separate the PoW Winners from the nodes to be removed.
  const auto& NewDSMembers = m_pendingDSBlock->GetHeader().GetDSPoWWinners();

  // For each of the proposed DS winners,
  for (const auto& DSPowWinner : NewDSMembers) {
    // Check that the proposed winner's connection information exists in our
    // view of all PoW Conns. Add it if it does not.
    if (m_allPoWConns.find(DSPowWinner.first) != m_allPoWConns.end()) {
      const auto& peer = m_allPoWConns.at(DSPowWinner.first);
      if (peer != DSPowWinner.second) {
        LOG_CHECK_FAIL("PoW Winner IP", DSPowWinner.second, peer);
        return false;
      }
    } else {
      // I don't know the winner -> store the IP given by the leader
      m_allPoWConns.emplace(DSPowWinner.first, DSPowWinner.second);
    }

    // Check that the PoW submission exists for the winner.
    if (m_allDSPoWs.find(DSPowWinner.first) == m_allDSPoWs.end()) {
      LOG_GENERAL(INFO,
                  "Cannot find DS PoW for node: "
                      << DSPowWinner.first
                      << ". Will continue look for it in PoW from leader.");
      if (dsWinnerPoWsFromLeader.find(DSPowWinner.first) !=
          dsWinnerPoWsFromLeader.end()) {
        uint8_t expectedDSDiff = m_mediator.m_dsBlockChain.GetLastBlock()
                                     .GetHeader()
                                     .GetDSDifficulty();
        const auto& peer = m_allPoWConns.at(DSPowWinner.first);
        const auto& dsPowSoln = dsWinnerPoWsFromLeader.at(DSPowWinner.first);

        auto headerHash = POW::GenHeaderHash(
            m_mediator.m_dsBlockRand, m_mediator.m_txBlockRand, peer,
            DSPowWinner.first, dsPowSoln.lookupId, dsPowSoln.gasPrice);

        string resultStr, mixHashStr;
        if (!DataConversion::charArrToHexStr(dsPowSoln.result, resultStr)) {
          return false;
        }
        if (!DataConversion::charArrToHexStr(dsPowSoln.mixhash, mixHashStr)) {
          return false;
        }

        // Validate the PoW submission
        bool result = POW::GetInstance().PoWVerify(
            m_pendingDSBlock->GetHeader().GetBlockNum(), expectedDSDiff,
            headerHash, dsPowSoln.nonce, resultStr, mixHashStr);
        if (!result) {
          LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                    "WARNING: Failed to verify DS PoW from node "
                        << DSPowWinner.first);
          return false;
        }

        // Insert the DS pow to my DS pow list so later can calculate DS
        // difficulty
        {
          std::lock_guard<mutex> lock(m_mutexAllPOW);
          m_allPoWs[DSPowWinner.first] = dsPowSoln;
        }
        AddDSPoWs(DSPowWinner.first, dsPowSoln);

      } else {
        LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                  "WARNING: Cannot find the DS winner PoW in DS PoW list from "
                  "leader.");
        return false;
      }
    }
  }

  return true;
}

bool DirectoryService::VerifyDifficulty() {
  auto remoteDSDifficulty = m_pendingDSBlock->GetHeader().GetDSDifficulty();
  auto localDSDifficulty = CalculateNewDSDifficulty(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty());
  uint32_t dsDifficultyDiff = std::max(remoteDSDifficulty, localDSDifficulty) -
                              std::min(remoteDSDifficulty, localDSDifficulty);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "DS difficulty diff " << dsDifficultyDiff);
  if (dsDifficultyDiff > DIFFICULTY_DIFF_TOL) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "WARNING: The ds difficulty "
                  << std::to_string(remoteDSDifficulty)
                  << " from leader not match with local calculated "
                     "result "
                  << std::to_string(localDSDifficulty));
    return false;
  }

  auto remoteDifficulty = m_pendingDSBlock->GetHeader().GetDifficulty();
  auto localDifficulty = CalculateNewDifficulty(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty());
  uint32_t difficultyDiff = std::max(remoteDifficulty, localDifficulty) -
                            std::min(remoteDifficulty, localDifficulty);
  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Difficulty diff " << difficultyDiff);
  if (difficultyDiff > DIFFICULTY_DIFF_TOL) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "WARNING: The difficulty "
                  << std::to_string(remoteDifficulty)
                  << " from leader not match with local calculated "
                     "result "
                  << std::to_string(localDifficulty));
    return false;
  }
  return true;
}

bool DirectoryService::VerifyRemovedByzantineNodes() {
  LOG_MARKER();

  // Get the list of proposed DS members
  const auto& powWinners = m_pendingDSBlock->GetHeader().GetDSPoWWinners();
  unsigned int numOfProposedMembers = powWinners.size();

  // Get the list of DS members to remove
  const auto& removeDSNodePubkeys =
      m_pendingDSBlock->GetHeader().GetDSRemovePubKeys();
  unsigned int numOfRemovedMembers = removeDSNodePubkeys.size();

  // Create an empty vector to populate with our view of the DS members to
  // remove.
  std::vector<PubKey> comparedToBeRemoved;
  unsigned int comparedNumOfRemoved =
      DetermineByzantineNodes(numOfProposedMembers, comparedToBeRemoved);

  // Check that the number of nodes to remove matches the proposed
  // DS block.
  if (numOfRemovedMembers != comparedNumOfRemoved) {
    LOG_GENERAL(WARNING,
                "The number of nodes to be removed due to bad performance does "
                "not match our view. numOfRemovedMembers: "
                    << numOfRemovedMembers
                    << " comparedNumOfRemoved: " << comparedNumOfRemoved);
    return false;
  }

  // Check that all of the nodes we computed to remove are present in the
  // proposed DS block.
  for (const auto& pubkey : comparedToBeRemoved) {
    if (std::find(removeDSNodePubkeys.begin(), removeDSNodePubkeys.end(),
                  pubkey) == removeDSNodePubkeys.end()) {
      LOG_GENERAL(WARNING, "Expected "
                               << pubkey
                               << " to be proposed for removal but could not "
                                  "find it in the proposed DS block");
      return false;
    }
  }

  return true;
}

bool DirectoryService::VerifyPoWOrdering(
    const DequeOfShard& shards, const MapOfPubKeyPoW& allPoWsFromLeader,
    const MapOfPubKeyPoW& priorityNodePoWs) {
  LOG_MARKER();

  // Requires mutex for m_shards
  bytes lastBlockHash(BLOCK_HASH_SIZE, 0);
  set<PubKey> keyset;

  if (m_mediator.m_currentEpochNum > 1) {
    lastBlockHash =
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();
  }

  const float MISORDER_TOLERANCE =
      (float)MISORDER_TOLERANCE_IN_PERCENT / ONE_HUNDRED_PERCENT;
  const uint32_t MAX_MISORDER_NODE =
      std::ceil(priorityNodePoWs.size() * MISORDER_TOLERANCE);

  LOG_GENERAL(INFO, "Tolerance = " << std::fixed << std::setprecision(2)
                                   << MISORDER_TOLERANCE << " = "
                                   << MAX_MISORDER_NODE << " nodes.");

  // Get the proposed DS members so we can get the size.
  std::map<PubKey, Peer> dsPoWWinners =
      m_pendingDSBlock->GetHeader().GetDSPoWWinners();

  // Get the list of removed nodes.
  std::vector<PubKey> removeDSNodePubkeys =
      m_pendingDSBlock->GetHeader().GetDSRemovePubKeys();

  // Sort and trim the PoW solutions
  auto sortedPoWSolns =
      SortPoWSoln(priorityNodePoWs, true, removeDSNodePubkeys.size());

  // Remove the DS solutions from the PoW solutions.
  for (const auto& winner : dsPoWWinners) {
    const PubKey& toFind = winner.first;
    auto it = std::remove_if(
        sortedPoWSolns.begin(), sortedPoWSolns.end(),
        [&toFind](
            const std::pair<std::array<unsigned char, 32>, PubKey>& item) {
          return item.second == toFind;
        });
    sortedPoWSolns.erase(it, sortedPoWSolns.end());
  }

  // Inject expired DS members into the shard POW.
  InjectPoWForDSNode(sortedPoWSolns, dsPoWWinners.size(), removeDSNodePubkeys);

  if (DEBUG_LEVEL >= 5) {
    for (const auto& pairPoWKey : sortedPoWSolns) {
      string PoWkeyStr;
      if (!DataConversion::charArrToHexStr(pairPoWKey.first, PoWkeyStr)) {
        LOG_GENERAL(WARNING,
                    "[POWS]"
                        << " cannot convert pairPoWKey.first to hex string");
      } else {
        LOG_GENERAL(INFO, "[POWS]" << PoWkeyStr << " " << pairPoWKey.second);
      }
    }
  }

  bytes hashVec(BLOCK_HASH_SIZE + BLOCK_HASH_SIZE);
  std::copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());
  bool ret = true;
  bytes vec(BLOCK_HASH_SIZE), preVec(BLOCK_HASH_SIZE);
  uint32_t misorderNodes = 0;
  for (const auto& shard : shards) {
    for (const auto& shardNode : shard) {
      const PubKey& toFind = std::get<SHARD_NODE_PUBKEY>(shardNode);
      auto it = std::find_if(
          sortedPoWSolns.cbegin(), sortedPoWSolns.cend(),
          [&toFind](
              const std::pair<std::array<unsigned char, 32>, PubKey>& item) {
            return item.second == toFind;
          });

      std::array<unsigned char, 32> result{};
      if (it == sortedPoWSolns.cend()) {
        LOG_GENERAL(WARNING, "Failed to find key in the PoW ordering "
                                 << toFind << " " << sortedPoWSolns.size());

        if (m_allPoWs.find(toFind) != m_allPoWs.end()) {
          result = m_allPoWs.at(toFind).result;
          LOG_GENERAL(INFO, "Found the PoW from local PoW list");
        } else {
          LOG_GENERAL(INFO,
                      "Checking for the key and PoW in the announcement...");
          auto pubKeyToPoW = allPoWsFromLeader.find(toFind);
          if (pubKeyToPoW != allPoWsFromLeader.end()) {
            const auto& peer = std::get<SHARD_NODE_PEER>(shardNode);
            const auto& powSoln = pubKeyToPoW->second;
            if (VerifyPoWFromLeader(peer, pubKeyToPoW->first, powSoln)) {
              result = powSoln.result;
            } else {
              ret = false;
              break;
            }
          } else {
            LOG_GENERAL(INFO, "Key also not in the PoWs in the announcement.");
            ret = false;
            break;
          }
        }
      } else {
        result = it->first;
      }

      auto r = keyset.insert(std::get<SHARD_NODE_PUBKEY>(shardNode));
      if (!r.second) {
        LOG_GENERAL(WARNING, "The key is not unique in the sharding structure "
                                 << std::get<SHARD_NODE_PUBKEY>(shardNode));
        ret = false;
        break;
      }

      copy(result.begin(), result.end(), hashVec.begin() + BLOCK_HASH_SIZE);
      const bytes& sortHashVec = HashUtils::BytesToHash(hashVec);

      if (DEBUG_LEVEL >= 5) {
        string sortHashVecStr;
        if (!DataConversion::Uint8VecToHexStr(sortHashVec, sortHashVecStr)) {
          LOG_GENERAL(INFO,
                      "[DSSORT]"
                          << " Unable to convert sortHashVec to hex string");
        } else {
          LOG_GENERAL(INFO, "[DSSORT]"
                                << sortHashVecStr << " "
                                << std::get<SHARD_NODE_PUBKEY>(shardNode));
        }
      }
      if (sortHashVec < vec) {
        string vecStr, sortHashVecStr;
        if (!DataConversion::Uint8VecToHexStr(vec, vecStr) ||
            !DataConversion::Uint8VecToHexStr(sortHashVec, sortHashVecStr)) {
          LOG_GENERAL(WARNING,
                      "Unable to convert vec or sortHashVec to hex string");
        } else {
          LOG_GENERAL(WARNING, "Bad PoW ordering found: " << vecStr << " "
                                                          << sortHashVecStr);
        }

        ++misorderNodes;
        // If there is one PoW ordering fail, then vec is assigned to a big
        // mismatch hash already, need to revert it to previous result and
        // continue the comparison.
        vec = preVec;
        continue;
      }
      preVec = vec;
      vec = sortHashVec;
    }
    if (!ret) {
      break;
    }
  }

  if (misorderNodes > MAX_MISORDER_NODE) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Failed to Verify due to bad PoW ordering count "
                  << misorderNodes << " "
                  << "exceed limit " << MAX_MISORDER_NODE);
    return false;
  }
  return ret;
}

bool DirectoryService::VerifyPoWFromLeader(const Peer& peer,
                                           const PubKey& pubKey,
                                           const PoWSolution& powSoln) {
  auto headerHash =
      POW::GenHeaderHash(m_mediator.m_dsBlockRand, m_mediator.m_txBlockRand,
                         peer, pubKey, powSoln.lookupId, powSoln.gasPrice);

  auto difficulty =
      (GUARD_MODE && Guard::GetInstance().IsNodeInShardGuardList(pubKey))
          ? (POW_DIFFICULTY / POW_DIFFICULTY)
          : m_mediator.m_dsBlockChain.GetLastBlock()
                .GetHeader()
                .GetDifficulty();

  string resultStr, mixHashStr;
  if (!DataConversion::charArrToHexStr(powSoln.result, resultStr)) {
    return false;
  }

  if (!DataConversion::charArrToHexStr(powSoln.mixhash, mixHashStr)) {
    return false;
  }

  if (!POW::GetInstance().PoWVerify(m_pendingDSBlock->GetHeader().GetBlockNum(),
                                    difficulty, headerHash, powSoln.nonce,
                                    resultStr, mixHashStr)) {
    LOG_GENERAL(WARNING, "Failed to verify PoW solution from leader for node: "
                             << pubKey);
    return false;
  }

  m_allPoWs[pubKey] = powSoln;

  m_allPoWConns.emplace(pubKey, peer);

  auto dsDifficulty =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();

  if (POW::GetInstance().PoWVerify(m_pendingDSBlock->GetHeader().GetBlockNum(),
                                   dsDifficulty, headerHash, powSoln.nonce,
                                   resultStr, mixHashStr)) {
    AddDSPoWs(pubKey, powSoln);
  }
  return true;
}

bool DirectoryService::VerifyNodePriority(const DequeOfShard& shards,
                                          MapOfPubKeyPoW& priorityNodePoWs) {
  // If the PoW submissions less than the max number of nodes, then all nodes
  // can join, no need to verify.
  if (m_allPoWs.size() <= MAX_SHARD_NODE_NUM) {
    priorityNodePoWs = m_allPoWs;
    return true;
  }

  uint32_t numOutOfMyPriorityList = 0;
  uint8_t lowestPriority = 0;
  auto setTopPriorityNodes = FindTopPriorityNodes(lowestPriority);

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Lowest priority to join is " << to_string(lowestPriority));

  // Inject the DS committee members into priority nodes list, because the
  // kicked out ds nodes will join the shard node, so the verify priority for
  // these nodes will pass.
  for (const auto& kv : *m_mediator.m_DSCommittee) {
    setTopPriorityNodes.insert(kv.first);
  }

  for (const auto& shard : shards) {
    for (const auto& shardNode : shard) {
      const PubKey& toFind = std::get<SHARD_NODE_PUBKEY>(shardNode);
      if (setTopPriorityNodes.find(toFind) == setTopPriorityNodes.end()) {
        auto reputation = m_mapNodeReputation[toFind];
        auto priority = CalculateNodePriority(reputation);
        if (priority < lowestPriority) {
          ++numOutOfMyPriorityList;
          LOG_GENERAL(WARNING,
                      "Node " << toFind << " is not in my top priority list");
        }
      }
    }
  }

  const uint32_t MAX_NODE_OUT_OF_LIST = std::ceil(
      MAX_SHARD_NODE_NUM * PRIORITY_TOLERANCE_IN_PERCENT / ONE_HUNDRED_PERCENT);
  if (numOutOfMyPriorityList > MAX_NODE_OUT_OF_LIST) {
    LOG_GENERAL(WARNING, "Number of node not in my priority "
                             << numOutOfMyPriorityList << " exceed tolerance "
                             << MAX_NODE_OUT_OF_LIST);
    return false;
  }

  for (const auto& pubKeyPoW : m_allPoWs) {
    if (setTopPriorityNodes.find(pubKeyPoW.first) !=
        setTopPriorityNodes.end()) {
      priorityNodePoWs.insert(pubKeyPoW);
    }
  }

  return true;
}

VectorOfPoWSoln DirectoryService::SortPoWSoln(
    const MapOfPubKeyPoW& mapOfPoWs, bool trimBeyondCommSize,
    const unsigned int byzantineRemoved) {
  std::map<array<unsigned char, 32>, PubKey> PoWOrderSorter;
  for (const auto& powsoln : mapOfPoWs) {
    PoWOrderSorter[powsoln.second.result] = powsoln.first;
  }

  // Put it back to vector for easy manipulation and adjustment of the ordering
  VectorOfPoWSoln sortedPoWSolns;
  if (trimBeyondCommSize) {
    const uint32_t numNodesTotal = PoWOrderSorter.size();

    // Number of Nodes to Trim. Account for the removed Byzantine nodes that do
    // not get injected as a shard solution.
    const uint32_t numNodesAfterTrim =
        std::min(ShardSizeCalculator::GetTrimmedShardCount(
                     m_mediator.GetShardSize(false), SHARD_SIZE_TOLERANCE_LO,
                     SHARD_SIZE_TOLERANCE_HI, numNodesTotal) +
                     byzantineRemoved,
                 numNodesTotal);

    LOG_GENERAL(INFO, "Trimming the solutions sorted list from "
                          << numNodesTotal << " to " << numNodesAfterTrim);

    uint32_t count = 0;
    if (!GUARD_MODE) {
      for (auto kv = PoWOrderSorter.begin();
           (kv != PoWOrderSorter.end()) && (count < numNodesAfterTrim);
           kv++, count++) {
        sortedPoWSolns.emplace_back(*kv);
      }
    } else {
      // If total num of shard nodes to be trim, ensure shard guards do not get
      // trimmed. To do it, a new map  will be created to included all shard
      // guards and a subset of normal shard nods
      // Steps:
      // 1. Maintain a map that called "FilteredPoWOrderSorter". It will
      // eventually contains Shard guards + subset of normal nodes
      // 2. Maintain a shadow copy of "PoWOrderSorter" called
      // "ShadowPoWOrderSorter". It is to track non guards node.
      // 3. Add shard guards to "FilteredPoWOrderSorter" ands remove it from
      // "ShadowPoWOrderSorter"
      // 4. If there are still slots left, obtained remaining normal shard node
      // from "ShadowPoWOrderSorter". Use it to populate
      // "FilteredPoWOrderSorter"
      // 5. Finally, sort "FilteredPoWOrderSorter" and stored result in
      // "PoWOrderSorter"
      uint32_t trimmedGuardCount = ceil(numNodesAfterTrim * SHARD_GUARD_TOL);
      uint32_t trimmedNonGuardCount = numNodesAfterTrim - trimmedGuardCount;

      if (trimmedGuardCount + trimmedNonGuardCount < numNodesAfterTrim) {
        LOG_GENERAL(WARNING,
                    "trimmedGuardCount: "
                        << trimmedGuardCount
                        << " trimmedNonGuardCount: " << trimmedNonGuardCount
                        << " numNodesAfterTrim: " << numNodesAfterTrim);
        trimmedGuardCount +=
            (numNodesAfterTrim - trimmedGuardCount - trimmedNonGuardCount);
        LOG_GENERAL(WARNING,
                    "Added  "
                        << (numNodesAfterTrim - trimmedGuardCount -
                            trimmedNonGuardCount)
                        << " to trimmedGuardCount to form a complete shard.");
      }

      // Assign all shard guards first
      std::map<array<unsigned char, 32>, PubKey> FilteredPoWOrderSorter;
      std::map<array<unsigned char, 32>, PubKey> ShadowPoWOrderSorter =
          PoWOrderSorter;

      // Add shard guards to "FilteredPoWOrderSorter"
      // Remove it from "ShadowPoWOrderSorter"
      for (auto kv = PoWOrderSorter.begin();
           (kv != PoWOrderSorter.end()) && (count < numNodesAfterTrim); kv++) {
        if (Guard::GetInstance().IsNodeInShardGuardList(kv->second)) {
          if (count == trimmedGuardCount) {
            LOG_GENERAL(INFO,
                        "Could not form max number of shard. Only allowed "
                            << trimmedGuardCount);
            break;
          }
          FilteredPoWOrderSorter.emplace(*kv);
          ShadowPoWOrderSorter.erase(kv->first);
          count++;
        }
      }

      // Assign non shard guards if there is any slots
      for (auto kv = ShadowPoWOrderSorter.begin();
           (kv != ShadowPoWOrderSorter.end()) && (count < numNodesAfterTrim);
           kv++) {
        FilteredPoWOrderSorter.emplace(*kv);
        count++;
      }

      // Sort "FilteredPoWOrderSorter" and stored it in "sortedPoWSolns"
      for (auto kv : FilteredPoWOrderSorter) {
        sortedPoWSolns.emplace_back(kv);
      }
      LOG_GENERAL(INFO, "Trimmed counts = " << trimmedGuardCount << " "
                                            << trimmedNonGuardCount);
      LOG_GENERAL(INFO, "Accepted solns = " << sortedPoWSolns.size());
    }

    LOG_GENERAL(INFO, "Num solns after trimming = " << sortedPoWSolns.size());

  } else {
    for (const auto& kv : PoWOrderSorter) {
      sortedPoWSolns.emplace_back(kv);
    }
  }

  return sortedPoWSolns;
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSPrimary() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnDSBlockWhenDSPrimary not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am the leader DS node. Creating DS block.");

  lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
  lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

  MapOfPubKeyPoW allPoWs;
  MapOfPubKeyPoW allDSPoWs;

  {
    std::lock_guard<std::mutex> g(m_mutexAllPOW);
    allPoWs = m_allPoWs;
  }

  {
    std::lock_guard<std::mutex> g(m_mutexAllDSPOWs);
    allDSPoWs = m_allDSPoWs;
  }

  if (allPoWs.size() > MAX_SHARD_NODE_NUM) {
    LOG_GENERAL(INFO, "PoWs recvd " << allPoWs.size()
                                    << " more than max node number "
                                    << MAX_SHARD_NODE_NUM);
    uint8_t lowestPriority = 0;
    auto setTopPriorityNodes = FindTopPriorityNodes(lowestPriority);
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Lowest priority to join is " << to_string(lowestPriority));

    MapOfPubKeyPoW tmpAllPoWs;
    for (const auto& pubKeyPoW : allPoWs) {
      if (setTopPriorityNodes.find(pubKeyPoW.first) !=
          setTopPriorityNodes.end()) {
        tmpAllPoWs.insert(pubKeyPoW);
      } else {
        LOG_GENERAL(INFO,
                    "Node " << pubKeyPoW.first
                            << " failed to join because priority not enough.");
        if (allDSPoWs.find(pubKeyPoW.first) != allDSPoWs.end()) {
          allDSPoWs.erase(pubKeyPoW.first);
        }
      }
    }

    allPoWs.swap(tmpAllPoWs);
  }

  auto sortedDSPoWSolns = SortPoWSoln(allDSPoWs);

  std::map<PubKey, Peer> powDSWinners;
  std::vector<PubKey> removeDSNodePubkeys;
  MapOfPubKeyPoW dsWinnerPoWs;
  uint32_t version = DSBLOCK_VERSION;
  uint8_t dsDifficulty = 0;
  uint8_t difficulty = 0;
  uint64_t blockNum = 0;
  BlockHash prevHash;

  // Determine the DS PoW winners.
  unsigned int numOfProposedDSMembers =
      ComputeDSBlockParameters(sortedDSPoWSolns, powDSWinners, dsWinnerPoWs,
                               dsDifficulty, difficulty, blockNum, prevHash);

  // Determine the losers from the performance.
  unsigned int numByzantine = 0;
  if (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
      UPGRADE_TARGET_DS_NUM) {
    numByzantine =
        DetermineByzantineNodes(numOfProposedDSMembers, removeDSNodePubkeys);
  }

  // Sort and trim the PoW solutions.
  auto sortedPoWSolns = SortPoWSoln(allPoWs, true, numByzantine);

  // Remove the DS solutions from the PoW solutions.
  unsigned int counter = 0;
  for (const auto& submitter : sortedDSPoWSolns) {
    if (counter >= numOfProposedDSMembers) {
      break;
    }
    sortedPoWSolns.erase(
        remove(sortedPoWSolns.begin(), sortedPoWSolns.end(), submitter),
        sortedPoWSolns.end());
    counter++;
  }

  // Inject expired DS members into the shard POW.
  InjectPoWForDSNode(sortedPoWSolns, numOfProposedDSMembers,
                     removeDSNodePubkeys);

  if (DEBUG_LEVEL >= 5) {
    for (const auto& pairPoWKey : sortedPoWSolns) {
      string powHashStr;
      if (!DataConversion::charArrToHexStr(pairPoWKey.first, powHashStr)) {
        LOG_GENERAL(WARNING,
                    "[POWS]"
                        << " Unable to convert pairPoWKey.first to hex str");
      } else {
        LOG_GENERAL(INFO, "[POWS]" << powHashStr << " " << pairPoWKey.second);
      }
    }
  }

  ClearReputationOfNodeWithoutPoW();
  ComputeSharding(sortedPoWSolns);

  vector<Peer> proposedDSMembersInfo;
  proposedDSMembersInfo.reserve(sortedDSPoWSolns.size());
  for (const auto& proposedMember : sortedDSPoWSolns) {
    proposedDSMembersInfo.emplace_back(m_allPoWConns[proposedMember.second]);
  }

  // Compute the DSBlockHashSet member of the DSBlockHeader
  DSBlockHashSet dsBlockHashSet;
  if (!Messenger::GetShardingStructureHash(SHARDINGSTRUCTURE_VERSION, m_shards,
                                           dsBlockHashSet.m_shardingHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetShardingStructureHash failed.");
    return false;
  }

  m_mediator.m_node->m_myshardId = m_shards.size();
  if (!BlockStorage::GetBlockStorage().PutShardStructure(
          m_shards, m_mediator.m_node->m_myshardId)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutShardStructure failed");
    return false;
  }

  // Compute the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }

  // Assemble DS block
  // To-do: Handle exceptions.
  // TODO: Revise DS block structure
  {
    m_pendingDSBlock.reset(new DSBlock(
        DSBlockHeader(dsDifficulty, difficulty, m_mediator.m_selfKey.second,
                      blockNum, m_mediator.m_currentEpochNum, GetNewGasPrice(),
                      m_mediator.m_curSWInfo, powDSWinners, removeDSNodePubkeys,
                      dsBlockHashSet, version, committeeHash, prevHash),
        CoSignatures(m_mediator.m_DSCommittee->size())));
  }

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "New DSBlock created with ds difficulty "
                << std::to_string(dsDifficulty) << " and difficulty "
                << std::to_string(difficulty));

  // Create new consensus object
  uint32_t consensusID = 0;
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

#ifdef VC_TEST_DS_SUSPEND_1
  if (m_mode == PRIMARY_DS && m_viewChangeCounter < 1) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_DS_SUSPEND_1)");
    return false;
  }
#endif  // VC_TEST_DS_SUSPEND_1

#ifdef VC_TEST_DS_SUSPEND_3
  if (m_mode == PRIMARY_DS && m_viewChangeCounter < 3) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_DS_SUSPEND_3)");
    return false;
  }
#endif  // VC_TEST_DS_SUSPEND_3

  m_consensusObject.reset(new ConsensusLeader(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
      static_cast<uint8_t>(DIRECTORY), static_cast<uint8_t>(DSBLOCKCONSENSUS),
      NodeCommitFailureHandlerFunc(), ShardCommitFailureHandlerFunc(), true));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "WARNING: Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  LOG_STATE(
      "[DSCON]["
      << std::setw(15) << std::left
      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] BGIN, POWS = " << m_allPoWs.size());

  // Refer to Effective mordern C++. Item 32: Use init capture to move objects
  // into closures.
  auto announcementGeneratorFunc =
      [this, dsWinnerPoWs = move(dsWinnerPoWs)](
          bytes& dst, unsigned int offset, const uint32_t consensusID,
          const uint64_t blockNumber, const bytes& blockHash,
          const uint16_t leaderID, const PairOfKey& leaderKey,
          bytes& messageToCosign) mutable -> bool {
    return Messenger::SetDSDSBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_pendingDSBlock, m_shards, m_allPoWs, dsWinnerPoWs, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);
  return true;
}

bool DirectoryService::DSBlockValidator(
    const bytes& message, unsigned int offset, [[gnu::unused]] bytes& errorMsg,
    const uint32_t consensusID, const uint64_t blockNumber,
    const bytes& blockHash, const uint16_t leaderID, const PubKey& leaderKey,
    bytes& messageToCosign) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::DSBlockValidator not "
                "expected to be called from LookUp node.");
    return true;
  }

  m_tempShards.clear();

  lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
  lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

  m_pendingDSBlock.reset(new DSBlock);

  MapOfPubKeyPoW allPoWsFromLeader;
  MapOfPubKeyPoW dsWinnerPoWsFromLeader;

  if (!Messenger::GetDSDSBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_pendingDSBlock, m_tempShards, allPoWsFromLeader,
          dsWinnerPoWsFromLeader, messageToCosign)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSDSBlockAnnouncement failed.");
    return false;
  }

  if (m_pendingDSBlock->GetHeader().GetVersion() != DSBLOCK_VERSION) {
    LOG_CHECK_FAIL("DSBlock version",
                   m_pendingDSBlock->GetHeader().GetVersion(), DSBLOCK_VERSION);
    return false;
  }

  if (!m_mediator.CheckWhetherBlockIsLatest(
          m_pendingDSBlock->GetHeader().GetBlockNum(),
          m_pendingDSBlock->GetHeader().GetEpochNum())) {
    LOG_GENERAL(WARNING, "DSBlockValidator CheckWhetherBlockIsLatest failed");
    return false;
  }

  BlockHash temp_blockHash = m_pendingDSBlock->GetHeader().GetMyHash();
  if (temp_blockHash != m_pendingDSBlock->GetBlockHash()) {
    LOG_GENERAL(WARNING,
                "Block Hash in Newly received DS Block doesn't match. "
                "Calculated: "
                    << temp_blockHash
                    << " Received: " << m_pendingDSBlock->GetBlockHash().hex());
    return false;
  }

  // Check timestamp
  if (!VerifyTimestamp(m_pendingDSBlock->GetTimestamp(),
                       CONSENSUS_OBJECT_TIMEOUT)) {
    return false;
  }

  // Verify the DSBlockHashSet member of the DSBlockHeader
  ShardingHash shardingHash;
  if (!Messenger::GetShardingStructureHash(SHARDINGSTRUCTURE_VERSION,
                                           m_tempShards, shardingHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetShardingStructureHash failed.");
    return false;
  }
  if (shardingHash != m_pendingDSBlock->GetHeader().GetShardingHash()) {
    LOG_GENERAL(WARNING,
                "Sharding structure hash in newly received DS Block doesn't "
                "match. Calculated: "
                    << shardingHash << " Received: "
                    << m_pendingDSBlock->GetHeader().GetShardingHash());
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  if (committeeHash != m_pendingDSBlock->GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING,
                "DS committee hash in newly received DS Block doesn't match. "
                "Calculated: "
                    << committeeHash << " Received: "
                    << m_pendingDSBlock->GetHeader().GetCommitteeHash());
    for (const auto& i : *m_mediator.m_DSCommittee) {
      LOG_GENERAL(WARNING, i.second);
    }
    return false;
  }

  BlockHash prevHash = get<BlockLinkIndex::BLOCKHASH>(
      m_mediator.m_blocklinkchain.GetLatestBlockLink());
  if (prevHash != m_pendingDSBlock->GetHeader().GetPrevHash()) {
    LOG_GENERAL(
        WARNING,
        "Prev Block hash in newly received DS Block doesn't match. Calculated "
            << prevHash << " Received"
            << m_pendingDSBlock->GetHeader().GetPrevHash());
    return false;
  }

  if (!VerifyPoWWinner(dsWinnerPoWsFromLeader)) {
    LOG_GENERAL(WARNING, "Failed to verify PoW winner");
    return false;
  }

  if (!ProcessShardingStructure(m_tempShards, m_tempPublicKeyToshardIdMap,
                                m_tempMapNodeReputation)) {
    return false;
  }

  // Verify the node priority before do the PoW trimming inside
  // VerifyPoWOrdering.
  ClearReputationOfNodeWithoutPoW();
  MapOfPubKeyPoW priorityNodePoWs;
  if (!VerifyNodePriority(m_tempShards, priorityNodePoWs)) {
    LOG_GENERAL(WARNING, "Failed to verify node priority");
    return false;
  }

  if (!VerifyPoWOrdering(m_tempShards, allPoWsFromLeader, priorityNodePoWs)) {
    LOG_GENERAL(WARNING, "Failed to verify ordering");
    return false;
  }

  // Check if the current block version to be validated requires removed nodes
  // validation.
  const uint32_t REMOVED_FIELD_DSBLOCK_VERSION = 2;
  if (m_pendingDSBlock->GetHeader().GetVersion() >=
          REMOVED_FIELD_DSBLOCK_VERSION &&
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
          UPGRADE_TARGET_DS_NUM) {
    // Verify the injected Byzantine nodes to be removed in the winners list.
    if (!VerifyRemovedByzantineNodes()) {
      LOG_GENERAL(WARNING,
                  "Failed to verify the Byzantine nodes to be removed");
      return false;
    }
  }

  // Start to verify difficulty from DS block number 2.
  if (m_pendingDSBlock->GetHeader().GetBlockNum() > 1) {
    if (!VerifyDifficulty()) {
      return false;
    }
  }

  if (!VerifyGasPrice(m_pendingDSBlock->GetHeader().GetGasPrice())) {
    LOG_GENERAL(WARNING, "Failed to verify gas price");
    return false;
  }

  return true;
}

bool DirectoryService::RunConsensusOnDSBlockWhenDSBackup() {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnDSBlockWhenDSBackup not "
                "expected to be called from LookUp node.");
    return true;
  }

#ifdef VC_TEST_VC_PRECHECK_1
  uint64_t dsCurBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  uint64_t txCurBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  // FIXME: Prechecking not working due at epoch 1 due to the way we have low
  // blocknum
  if (m_consensusMyID == 3 && dsCurBlockNum != 0 && txCurBlockNum > 10) {
    LOG_EPOCH(
        WARNING, m_mediator.m_currentEpochNum,
        "I am suspending myself to test viewchange (VC_TEST_VC_PRECHECK_1)");
    this_thread::sleep_for(chrono::seconds(45));
    return false;
  }
#endif  // VC_TEST_VC_PRECHECK_1

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "I am a backup DS node. Waiting for DS block announcement. "
            "Leader is at index  "
                << GetConsensusLeaderID() << " "
                << m_mediator.m_DSCommittee->at(GetConsensusLeaderID()).second);

  // Dummy values for now
  uint32_t consensusID = 0x0;
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto func = [this](const bytes& input, unsigned int offset, bytes& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const bytes& blockHash, const uint16_t leaderID,
                     const PubKey& leaderKey,
                     bytes& messageToCosign) mutable -> bool {
    return DSBlockValidator(input, offset, errorMsg, consensusID, blockNumber,
                            blockHash, leaderID, leaderKey, messageToCosign);
  };

  m_consensusObject.reset(new ConsensusBackup(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, GetConsensusLeaderID(), m_mediator.m_selfKey.first,
      *m_mediator.m_DSCommittee, static_cast<uint8_t>(DIRECTORY),
      static_cast<uint8_t>(DSBLOCKCONSENSUS), func));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
              "Unable to create consensus object");
    return false;
  }

  return true;
}

bool DirectoryService::ProcessShardingStructure(
    const DequeOfShard& shards,
    std::map<PubKey, uint32_t>& publicKeyToshardIdMap,
    std::map<PubKey, uint16_t>& mapNodeReputation) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessShardingStructure not "
                "expected to be called from LookUp node.");
    return true;
  }

  publicKeyToshardIdMap.clear();
  mapNodeReputation.clear();

  size_t totalShardNodes = 0;
  for (const auto& shard : shards) {
    totalShardNodes += shard.size();
  }

  const size_t MAX_DIFF_IP_NODES = std::ceil(
      totalShardNodes * DIFF_IP_TOLERANCE_IN_PERCENT / ONE_HUNDRED_PERCENT);
  size_t diffIpNodes = 0;

  for (unsigned int i = 0; i < shards.size(); i++) {
    for (const auto& shardNode : shards.at(i)) {
      const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(shardNode);

      mapNodeReputation[pubKey] = std::get<SHARD_NODE_REP>(shardNode);

      auto storedMember = m_allPoWConns.find(pubKey);

      // I know the member but the member IP given by the leader is different!
      if (storedMember != m_allPoWConns.end()) {
        if (storedMember->second != std::get<SHARD_NODE_PEER>(shardNode)) {
          LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                    "IP of the member different "
                    "from what was in m_allPoWConns???");
          LOG_GENERAL(WARNING, "Stored  "
                                   << storedMember->second << " Received"
                                   << std::get<SHARD_NODE_PEER>(shardNode));
          diffIpNodes++;

          if (diffIpNodes > MAX_DIFF_IP_NODES) {
            LOG_EPOCH(WARNING, m_mediator.m_currentEpochNum,
                      "Number of nodes using different IP address "
                          << diffIpNodes << " exceeds tolerance "
                          << MAX_DIFF_IP_NODES);
            return false;
          }

          // If the node ip i get is different from leader, erase my one, and
          // accept the ip from leader if within tolerance
          m_allPoWConns.erase(storedMember);
          m_allPoWConns.emplace(std::get<SHARD_NODE_PUBKEY>(shardNode),
                                std::get<SHARD_NODE_PEER>(shardNode));
        }
      }
      // I don't know the member -> store the IP given by the leader
      else {
        m_allPoWConns.emplace(std::get<SHARD_NODE_PUBKEY>(shardNode),
                              std::get<SHARD_NODE_PEER>(shardNode));
      }

      publicKeyToshardIdMap.emplace(std::get<SHARD_NODE_PUBKEY>(shardNode), i);
    }
  }

  return true;
}

void DirectoryService::SaveDSPerformanceCore(
    std::map<uint64_t, std::map<int32_t, std::vector<PubKey>>>&
        coinbaseRewardees,
    std::map<PubKey, uint32_t>& dsMemberPerformance, DequeOfNode& dsComm,
    uint64_t currentEpochNum, unsigned int numOfFinalBlock,
    int finalblockRewardID) {
  LOG_MARKER();

  // Clear the previous performances.
  dsMemberPerformance.clear();

  // Initialise the map with the DS Committee public keys mapped to 0.
  for (const auto& member : dsComm) {
    dsMemberPerformance[member.first] = 0;
  }

  uint64_t firstTxEpoch = 0;
  if (currentEpochNum >= numOfFinalBlock) {
    firstTxEpoch = (currentEpochNum / numOfFinalBlock - 1) * numOfFinalBlock;
  }
  // Go through the coinbase rewardees and tally the number of co-sigs.
  // For each TX epoch,
  for (auto const& rewardees : coinbaseRewardees) {
    // if rewardees not from current ds epoch, ignore them
    if (rewardees.first < firstTxEpoch) {
      continue;
    }
    // Find the DS Shard.
    for (auto const& shard : rewardees.second) {
      if (shard.first == finalblockRewardID) {
        // Find the rewards that belong to the DS Shard.
        for (auto const& pubkey : shard.second) {
          // Check if the public key exists in the initialized map.
          if (dsMemberPerformance.find(pubkey) == dsMemberPerformance.end()) {
            LOG_GENERAL(WARNING,
                        "Unknown (Not in DS Committee) public key "
                            << pubkey
                            << " found to have "
                               "contributed co-sigs as a DS Committee member.");
          } else {
            // Increment the performance score if the public key exists.
            ++dsMemberPerformance[pubkey];
          }
        }
      }
    }
  }

  // Display the performance scores of all the DS Committee members.
  LOG_EPOCH(INFO, currentEpochNum, "DS Committee Co-Signature Performance");
  unsigned int index = 0;
  uint32_t maxCoSigs = (numOfFinalBlock - 1) * 2;
  for (const auto& member : dsMemberPerformance) {
    LOG_GENERAL(INFO, "[" << PAD(index++, 3, ' ') << "] " << member.first << " "
                          << PAD(member.second, 4, ' ') << "/" << maxCoSigs);
  }
}

void DirectoryService::SaveDSPerformance() {
  LOG_MARKER();
  std::lock(m_mutexCoinbaseRewardees, m_mutexDsMemberPerformance);
  std::lock_guard<mutex> h(m_mutexCoinbaseRewardees, std::adopt_lock);
  std::lock_guard<mutex> g(m_mutexDsMemberPerformance, std::adopt_lock);

  SaveDSPerformanceCore(m_coinbaseRewardees, m_dsMemberPerformance,
                        *m_mediator.m_DSCommittee, m_mediator.m_currentEpochNum,
                        NUM_FINAL_BLOCK_PER_POW,
                        CoinbaseReward::FINALBLOCK_REWARD);
}

unsigned int DirectoryService::DetermineByzantineNodesCore(
    unsigned int numOfProposedDSMembers,
    std::vector<PubKey>& removeDSNodePubkeys, uint64_t currentEpochNum,
    unsigned int numOfFinalBlock, double performanceThreshold,
    unsigned int maxByzantineRemoved, DequeOfNode& dsComm,
    const std::map<PubKey, uint32_t>& dsMemberPerformance) {
  LOG_MARKER();

  // Do not determine Byzantine nodes on the first epoch when performance cannot
  // be measured.
  if (currentEpochNum <= 1) {
    LOG_GENERAL(INFO,
                "Skipping determining Byzantine nodes for removal since "
                "performance cannot be measured on the first epoch.");
    return 0;
  }

  // Parameters
  uint32_t maxCoSigs = (numOfFinalBlock - 1) * 2;
  uint32_t threshold = std::ceil(performanceThreshold * maxCoSigs);
  unsigned int numToRemove =
      std::min(maxByzantineRemoved, numOfProposedDSMembers);

  // Build a list of Byzantine Nodes
  LOG_EPOCH(INFO, currentEpochNum,
            "Evaluating performance of the current DS Committee.");
  LOG_GENERAL(INFO, "maxCoSigs = " << maxCoSigs);
  LOG_GENERAL(
      INFO, "threshold = " << threshold << " (" << performanceThreshold << ")");
  unsigned int numByzantine = 0;
  unsigned int index = 0;
  for (auto it = dsComm.begin(); it != dsComm.end(); ++it) {
    // Do not evaluate guard nodes.
    if (GUARD_MODE && Guard::GetInstance().IsNodeInDSGuardList(it->first)) {
      continue;
    }

    // Check if the score is below the calculated threshold.
    uint32_t score = dsMemberPerformance.at(it->first);
    if (score < threshold) {
      // Only add the node to be removed if there is still capacity.
      if (numByzantine < numToRemove) {
        removeDSNodePubkeys.emplace_back(it->first);
      }

      // Log the index and public key of a found Byzantine node regardless of if
      // they will be removed.
      LOG_GENERAL(INFO, "[" << PAD(index++, 3, ' ') << "] " << it->first << " "
                            << PAD(score, 4, ' ') << "/" << maxCoSigs);
      ++numByzantine;
    }
  }

  // Log the general statistics of the computation.
  unsigned int numRemoved = std::min(numToRemove, numByzantine);
  LOG_GENERAL(INFO, "Number of DS members not meeting the co-sig threshold: "
                        << numByzantine);
  LOG_GENERAL(INFO,
              "Number of Byzantine DS members to be removed: " << numRemoved);

  return numRemoved;
}

unsigned int DirectoryService::DetermineByzantineNodes(
    unsigned int numOfProposedDSMembers,
    std::vector<PubKey>& removeDSNodePubkeys) {
  LOG_MARKER();
  std::lock(m_mutexDsMemberPerformance, m_mediator.m_mutexDSCommittee);
  std::lock_guard<mutex> g(m_mutexDsMemberPerformance, std::adopt_lock);
  std::lock_guard<mutex> g2(m_mediator.m_mutexDSCommittee, std::adopt_lock);

  return DetermineByzantineNodesCore(
      numOfProposedDSMembers, removeDSNodePubkeys, m_mediator.m_currentEpochNum,
      NUM_FINAL_BLOCK_PER_POW, DS_PERFORMANCE_THRESHOLD_PERCENT,
      NUM_DS_BYZANTINE_REMOVED, *m_mediator.m_DSCommittee,
      m_dsMemberPerformance);
}

void DirectoryService::RunConsensusOnDSBlock() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnDSBlock not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "Number of PoW recvd: " << m_allPoWs.size() << ", DS PoW recvd: "
                                    << m_allDSPoWs.size());

  LOG_STATE("[POW][" << m_mediator.m_currentEpochNum << "] DS PoW = "
                     << m_allDSPoWs.size() << " PoW = " << m_allPoWs.size());

  if (m_doRejoinAtDSConsensus) {
    RejoinAsDS();
  }

  if (m_state != DSBLOCK_CONSENSUS_PREP) {
    SetState(DSBLOCK_CONSENSUS_PREP);
  }

  // Record the performance of the coinbase rewardees to get the co-sigs
  // before the variable is cleared.
  SaveDSPerformance();

  {
    lock_guard<mutex> h(m_mutexCoinbaseRewardees);
    m_coinbaseRewardees.clear();
  }

  // Upon consensus object creation failure, one should not return from the
  // function, but rather wait for view change.
  bool ConsensusObjCreation = true;
  if (m_mode == PRIMARY_DS) {
    ConsensusObjCreation = RunConsensusOnDSBlockWhenDSPrimary();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING, "Error after RunConsensusOnDSBlockWhenDSPrimary");
    }
  } else {
    ConsensusObjCreation = RunConsensusOnDSBlockWhenDSBackup();
    if (!ConsensusObjCreation) {
      LOG_GENERAL(WARNING, "Error after RunConsensusOnDSBlockWhenDSBackup");
    }
  }

  if (ConsensusObjCreation) {
    SetState(DSBLOCK_CONSENSUS);
    cv_DSBlockConsensusObject.notify_all();
  }

  // View change will wait for timeout. If conditional variable is notified
  // before timeout, the thread will return without triggering view change.
  std::unique_lock<std::mutex> cv_lk(m_MutexCVViewChangeDSBlock);
  if (cv_viewChangeDSBlock.wait_for(cv_lk,
                                    std::chrono::seconds(VIEWCHANGE_TIME)) ==
      std::cv_status::timeout) {
    LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
              "Initiated DS block view change. ");

    if (m_mode == PRIMARY_DS) {
      ConsensusLeader* cl =
          dynamic_cast<ConsensusLeader*>(m_consensusObject.get());
      if (cl != nullptr) {
        cl->Audit();
      }
    }

    auto func = [this]() -> void { RunConsensusOnViewChange(); };
    DetachedFunction(1, func);
  }
}
