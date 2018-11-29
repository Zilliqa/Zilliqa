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
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libPOW/pow.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/UpgradeManager.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int DirectoryService::ComputeDSBlockParameters(
    const VectorOfPoWSoln& sortedDSPoWSolns, VectorOfPoWSoln& sortedPoWSolns,
    map<PubKey, Peer>& powDSWinners, MapOfPubKeyPoW& dsWinnerPoWs,
    uint8_t& dsDifficulty, uint8_t& difficulty, uint64_t& blockNum,
    BlockHash& prevHash) {
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
    sortedPoWSolns.erase(
        remove(sortedPoWSolns.begin(), sortedPoWSolns.end(), submitter),
        sortedPoWSolns.end());
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

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Prev DS block hash as per leader " << prevHash.hex());
  }

  // Start to adjust difficulty from second DS block.
  if (blockNum > 1) {
    dsDifficulty = CalculateNewDSDifficulty(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Current DS difficulty "
                  << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetDSDifficulty())
                  << ", new DS difficulty " << std::to_string(dsDifficulty));

    difficulty = CalculateNewDifficulty(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty());
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Current difficulty "
                  << std::to_string(m_mediator.m_dsBlockChain.GetLastBlock()
                                        .GetHeader()
                                        .GetDifficulty())
                  << ", new difficulty " << std::to_string(difficulty));
  }

  if (UpgradeManager::GetInstance().HasNewSW()) {
    if (UpgradeManager::GetInstance().DownloadSW()) {
      lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
      m_mediator.m_curSWInfo = *UpgradeManager::GetInstance().GetLatestSWInfo();
    }
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

  if (sortedPoWSolns.size() < m_mediator.GetShardSize(false)) {
    LOG_GENERAL(WARNING, "PoWs recvd less than one shard size. sortedPoWSolns: "
                             << sortedPoWSolns.size());
  }

  std::set<PubKey> setTopPriorityNodes;
  if (sortedPoWSolns.size() > MAX_SHARD_NODE_NUM) {
    LOG_GENERAL(INFO, "PoWs recvd " << sortedPoWSolns.size()
                                    << " more than max node number "
                                    << MAX_SHARD_NODE_NUM);
    setTopPriorityNodes = FindTopPriorityNodes();
  }

  auto numShardNodes = sortedPoWSolns.size() > MAX_SHARD_NODE_NUM
                           ? MAX_SHARD_NODE_NUM
                           : sortedPoWSolns.size();

  uint32_t numOfComms = numShardNodes / m_mediator.GetShardSize(false);
  uint32_t max_shard = numOfComms - 1;

  if (numOfComms == 0) {
    LOG_GENERAL(WARNING, "Cannot form even one committee "
                             << " number of Pows " << sortedPoWSolns.size()
                             << " Setting numOfcomms to be 1");
    numOfComms = 1;
    max_shard = 0;
  }

  uint32_t numNodesPerShard = numShardNodes / numOfComms;

  for (unsigned int i = 0; i < numOfComms; i++) {
    m_shards.emplace_back();
  }
  map<array<unsigned char, BLOCK_HASH_SIZE>, PubKey> sortedPoWs;
  vector<unsigned char> lastBlockHash(BLOCK_HASH_SIZE);

  if (m_mediator.m_currentEpochNum > 1) {
    lastBlockHash =
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();
  }

  vector<unsigned char> hashVec(BLOCK_HASH_SIZE + POW_SIZE);
  copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());

  for (const auto& kv : sortedPoWSolns) {
    const PubKey& key = kv.second;
    if (!setTopPriorityNodes.empty() &&
        setTopPriorityNodes.find(key) == setTopPriorityNodes.end()) {
      LOG_GENERAL(INFO, "Node "
                            << key
                            << " failed to join because priority not enough.");
      continue;
    }

    const auto& powHash = kv.first;
    // sort all PoW submissions according to H(last_block_hash, pow_hash)
    copy(powHash.begin(), powHash.end(), hashVec.begin() + BLOCK_HASH_SIZE);

    const vector<unsigned char>& sortHashVec = HashUtils::BytesToHash(hashVec);
    array<unsigned char, BLOCK_HASH_SIZE> sortHash;
    copy(sortHashVec.begin(), sortHashVec.end(), sortHash.begin());
    sortedPoWs.emplace(sortHash, key);
  }

  uint32_t i = 0;
  uint32_t j = 0;
  for (const auto& kv : sortedPoWs) {
    if (DEBUG_LEVEL >= 5) {
      LOG_GENERAL(INFO, "[DSSORT] " << kv.second << " "
                                    << DataConversion::charArrToHexStr(kv.first)
                                    << endl);
    }

    unsigned int shard_index = i / numNodesPerShard;
    if (shard_index > max_shard) {
      shard_index = j % (max_shard + 1);
      j++;
    }

    const PubKey& key = kv.second;
    auto& shard = m_shards.at(shard_index);
    shard.emplace_back(key, m_allPoWConns.at(key), m_mapNodeReputation[key]);
    m_publicKeyToshardIdMap.emplace(key, shard_index);
    i++;
  }
}

void DirectoryService::InjectPoWForDSNode(VectorOfPoWSoln& sortedPoWSolns,
                                          unsigned int numOfProposedDSMembers) {
  // Add the oldest n DS committee member to m_allPoWs and m_allPoWConns so it
  // gets included in sharding structure
  if (numOfProposedDSMembers > m_mediator.m_DSCommittee->size()) {
    LOG_GENERAL(FATAL,
                "number of proposed ds member is larger than current ds "
                "committee. numOfProposedDSMembers: "
                    << numOfProposedDSMembers << " m_DSCommittee size: "
                    << m_mediator.m_DSCommittee->size());
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  vector<unsigned char> serializedPubK;
  for (unsigned int i = 0; i < numOfProposedDSMembers; i++) {
    // TODO: Revise this as this is rather ad hoc. Currently, it is SHA2(PubK)
    // to act as the PoW soln
    // TODO: To determine how to include kicked out ds member (who did not do
    // PoW) back into the shardding strcture
    PubKey nodePubKey =
        m_mediator.m_DSCommittee->at(m_mediator.m_DSCommittee->size() - 1 - i)
            .first;
    nodePubKey.Serialize(serializedPubK, 0);
    sha2.Update(serializedPubK);
    vector<unsigned char> PubKeyHash;
    PubKeyHash = sha2.Finalize();
    array<unsigned char, 32> PubKeyHashArr;

    // Injecting into sorted PoWs
    copy(PubKeyHash.begin(), PubKeyHash.end(), PubKeyHashArr.begin());

    // Check whether injected node submit soln (maliciously)
    // This could happen if the node rejoin as a normal shard node by submitting
    // PoW and DS committee injected it
    bool isDupPubKey = false;
    for (const auto& soln : sortedPoWSolns) {
      if (soln.second == nodePubKey) {
        LOG_GENERAL(WARNING,
                    "Injected node also submitted a soln. "
                        << m_mediator.m_DSCommittee
                               ->at(m_mediator.m_DSCommittee->size() - 1 - i)
                               .second);
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
    LOG_GENERAL(INFO, "Injecting into Pow Connections");
    if (m_mediator.m_DSCommittee->at(m_mediator.m_DSCommittee->size() - 1 - i)
            .second == Peer()) {
      m_allPoWConns.emplace(m_mediator.m_selfKey.second, m_mediator.m_selfPeer);
      LOG_GENERAL(INFO, m_mediator.m_selfPeer);
    } else {
      m_allPoWConns.emplace(m_mediator.m_DSCommittee->at(
          m_mediator.m_DSCommittee->size() - 1 - i));
      LOG_GENERAL(INFO, m_mediator.m_DSCommittee
                            ->at(m_mediator.m_DSCommittee->size() - 1 - i)
                            .second);
    }
  }
}

bool DirectoryService::VerifyPoWWinner(
    const MapOfPubKeyPoW& dsWinnerPoWsFromLeader) {
  const auto& NewDSMembers = m_pendingDSBlock->GetHeader().GetDSPoWWinners();
  for (const auto& DSPowWinner : NewDSMembers) {
    if (m_allPoWConns.find(DSPowWinner.first) != m_allPoWConns.end()) {
      if (m_allPoWConns.at(DSPowWinner.first) != DSPowWinner.second) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "WARNING: Why is the IP of the winner different from "
                  "what I have in m_allPoWConns???");
        return false;
      }
    } else {
      // I don't know the winner -> store the IP given by the leader
      m_allPoWConns.emplace(DSPowWinner.first, DSPowWinner.second);
    }

    if (m_allDSPoWs.find(DSPowWinner.first) == m_allDSPoWs.end()) {
      LOG_GENERAL(INFO,
                  "Cannot find DS PoW for node: "
                      << DSPowWinner.first
                      << ". Will continue look for it in PoW from leader.");
      if (dsWinnerPoWsFromLeader.find(DSPowWinner.first) !=
          dsWinnerPoWsFromLeader.end()) {
        uint8_t expectedDSDiff = DS_POW_DIFFICULTY;
        const auto& peer = m_allPoWConns.at(DSPowWinner.first);
        const auto& dsPowSoln = dsWinnerPoWsFromLeader.at(DSPowWinner.first);
        // Non-genesis block
        if (m_mediator.m_currentEpochNum > 1) {
          expectedDSDiff = m_mediator.m_dsBlockChain.GetLastBlock()
                               .GetHeader()
                               .GetDSDifficulty();
        }

        bool result = POW::GetInstance().PoWVerify(
            m_pendingDSBlock->GetHeader().GetBlockNum(), expectedDSDiff,
            m_mediator.m_dsBlockRand, m_mediator.m_txBlockRand,
            peer.m_ipAddress, DSPowWinner.first, dsPowSoln.lookupId,
            dsPowSoln.gasPrice, dsPowSoln.nonce,
            DataConversion::charArrToHexStr(dsPowSoln.result),
            DataConversion::charArrToHexStr(dsPowSoln.mixhash));
        if (!result) {
          LOG_EPOCH(WARNING,
                    std::to_string(m_mediator.m_currentEpochNum).c_str(),
                    "WARNING: Failed to verify DS PoW from node "
                        << DSPowWinner.first);
          return false;
        }
      } else {
        LOG_EPOCH(WARNING, std::to_string(m_mediator.m_currentEpochNum).c_str(),
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
  constexpr uint8_t DIFFICULTY_TOL = 1;
  if (std::max(remoteDSDifficulty, localDSDifficulty) -
          std::min(remoteDSDifficulty, localDSDifficulty) >
      DIFFICULTY_TOL) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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
  if (std::max(remoteDifficulty, localDifficulty) -
          std::min(remoteDifficulty, localDifficulty) >
      DIFFICULTY_TOL) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "WARNING: The difficulty "
                  << std::to_string(remoteDifficulty)
                  << " from leader not match with local calculated "
                     "result "
                  << std::to_string(localDifficulty));
    return false;
  }
  return true;
}

bool DirectoryService::VerifyPoWOrdering(
    const DequeOfShard& shards, const MapOfPubKeyPoW& allPoWsFromTheLeader) {
  // Requires mutex for m_shards
  vector<unsigned char> lastBlockHash(BLOCK_HASH_SIZE, 0);
  set<PubKey> keyset;

  if (m_mediator.m_currentEpochNum > 1) {
    lastBlockHash =
        m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();
  }

  const float MISORDER_TOLERANCE =
      (float)MISORDER_TOLERANCE_IN_PERCENT / 100.00f;
  const uint32_t MAX_MISORDER_NODE =
      std::ceil(m_allPoWs.size() * MISORDER_TOLERANCE);

  LOG_GENERAL(INFO, "Tolerance = " << std::fixed << std::setprecision(2)
                                   << MISORDER_TOLERANCE << " = "
                                   << MAX_MISORDER_NODE << " nodes.");

  auto sortedPoWSolns = SortPoWSoln(m_allPoWs, true);
  InjectPoWForDSNode(sortedPoWSolns,
                     m_pendingDSBlock->GetHeader().GetDSPoWWinners().size());
  if (DEBUG_LEVEL >= 5) {
    for (const auto& pairPoWKey : sortedPoWSolns) {
      LOG_GENERAL(INFO, "[POWS]"
                            << DataConversion::charArrToHexStr(pairPoWKey.first)
                            << " " << pairPoWKey.second);
    }
  }

  vector<unsigned char> hashVec(BLOCK_HASH_SIZE + BLOCK_HASH_SIZE);
  std::copy(lastBlockHash.begin(), lastBlockHash.end(), hashVec.begin());
  bool ret = true;
  vector<unsigned char> vec(BLOCK_HASH_SIZE), preVec(BLOCK_HASH_SIZE);
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

      std::array<unsigned char, 32> result;
      if (it == sortedPoWSolns.cend()) {
        LOG_GENERAL(WARNING, "Failed to find key in the PoW ordering "
                                 << toFind << " " << sortedPoWSolns.size());

        LOG_GENERAL(INFO,
                    "Checking for the key and PoW in the announcement...");

        auto itLeaderMap = allPoWsFromTheLeader.find(toFind);
        if (itLeaderMap != allPoWsFromTheLeader.end()) {
          LOG_GENERAL(INFO,
                      "TODO: Verify the PoW submission for this unknown node.");
          result = itLeaderMap->second.result;
        } else {
          LOG_GENERAL(INFO, "Key also not in the PoWs in the announcement.");
          ret = false;
        }

        if (!ret) {
          break;
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
      const vector<unsigned char>& sortHashVec =
          HashUtils::BytesToHash(hashVec);
      if (DEBUG_LEVEL >= 5) {
        LOG_GENERAL(INFO, "[DSSORT]"
                              << DataConversion::Uint8VecToHexStr(sortHashVec)
                              << " " << std::get<SHARD_NODE_PUBKEY>(shardNode));
      }
      if (sortHashVec < vec) {
        LOG_GENERAL(WARNING,
                    "Bad PoW ordering found: "
                        << DataConversion::Uint8VecToHexStr(vec) << " "
                        << DataConversion::Uint8VecToHexStr(sortHashVec));
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
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Failed to Verify due to bad PoW ordering count "
                  << misorderNodes << " "
                  << "exceed limit " << MAX_MISORDER_NODE);
    return false;
  }
  return ret;
}

bool DirectoryService::VerifyNodePriority(const DequeOfShard& shards) {
  // If the PoW submissions less than the max number of nodes, then all nodes
  // can join, no need to verify.
  if (m_allPoWs.size() <= MAX_SHARD_NODE_NUM) {
    return true;
  }

  uint32_t numOutOfMyPriorityList = 0;
  auto setTopPriorityNodes = FindTopPriorityNodes();
  for (const auto& shard : shards) {
    for (const auto& shardNode : shard) {
      const PubKey& toFind = std::get<SHARD_NODE_PUBKEY>(shardNode);
      if (setTopPriorityNodes.find(toFind) == setTopPriorityNodes.end()) {
        ++numOutOfMyPriorityList;
        LOG_GENERAL(WARNING,
                    "Node " << toFind << " is not in my top priority list");
      }
    }
  }

  constexpr float tolerance = 0.02f;
  const uint32_t MAX_NODE_OUT_OF_LIST =
      std::ceil(MAX_SHARD_NODE_NUM * tolerance);
  if (numOutOfMyPriorityList > MAX_NODE_OUT_OF_LIST) {
    LOG_GENERAL(WARNING, "Number of node not in my priority "
                             << numOutOfMyPriorityList << " exceed tolerance "
                             << MAX_NODE_OUT_OF_LIST);
    return false;
  }
  return true;
}

void DirectoryService::ComputeTxnSharingAssignments(
    const vector<Peer>& proposedDSMembers) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ComputeTxnSharingAssignments not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();

  // PART 1
  // First version: We just take the first X nodes in DS committee
  // Take note that this is the OLD DS committee -> we must consider that
  // winnerpeer is the new DS member (and the last node in the committee will no
  // longer be a DS node)

  m_DSReceivers.clear();

  uint32_t num_ds_nodes =
      (m_mediator.m_DSCommittee->size() < TX_SHARING_CLUSTER_SIZE)
          ? m_mediator.m_DSCommittee->size()
          : TX_SHARING_CLUSTER_SIZE;

  // Add the new DS leader first
  for (const auto& proposedMember : proposedDSMembers) {
    m_DSReceivers.emplace_back(proposedMember);
  }

  m_mediator.m_node->m_txnSharingIAmSender = true;
  num_ds_nodes--;

  // Add the rest from the current DS committee
  for (unsigned int i = 0; i < num_ds_nodes; i++) {
    if (i != m_consensusMyID) {
      m_DSReceivers.emplace_back(m_mediator.m_DSCommittee->at(i).second);
    } else {
      // when i == m_consensusMyID use m_mediator.m_selfPeer since IP/ port in
      // m_mediator.m_DSCommittee->at(m_consensusMyID).second is zeroed out
      m_DSReceivers.emplace_back(m_mediator.m_selfPeer);
    }
  }

  // PART 2 and 3
  // First version: We just take the first X nodes for receiving and next X
  // nodes for sending

  m_shardReceivers.clear();
  m_shardSenders.clear();

  for (const auto& shard : m_shards) {
    // PART 2

    m_shardReceivers.emplace_back();

    uint32_t nodes_recv_lo = 0;
    uint32_t nodes_recv_hi = nodes_recv_lo + TX_SHARING_CLUSTER_SIZE - 1;

    if (nodes_recv_hi >= shard.size()) {
      nodes_recv_hi = shard.size() - 1;
    }

    unsigned int num_nodes = nodes_recv_hi - nodes_recv_lo + 1;

    auto node_peer = shard.begin();
    for (unsigned int j = 0; j < num_nodes; j++) {
      m_shardReceivers.back().emplace_back(
          std::get<SHARD_NODE_PEER>(*node_peer));
      node_peer++;
    }

    // PART 3

    m_shardSenders.emplace_back();

    uint32_t nodes_send_lo = 0;
    uint32_t nodes_send_hi = 0;

    if (shard.size() <= TX_SHARING_CLUSTER_SIZE) {
      nodes_send_lo = nodes_recv_lo;
      nodes_send_hi = nodes_recv_hi;
    } else if (shard.size() < (2 * TX_SHARING_CLUSTER_SIZE)) {
      nodes_send_lo = shard.size() - TX_SHARING_CLUSTER_SIZE;
      nodes_send_hi = nodes_send_lo + TX_SHARING_CLUSTER_SIZE - 1;
    } else {
      nodes_send_lo = TX_SHARING_CLUSTER_SIZE;
      nodes_send_hi = nodes_send_lo + TX_SHARING_CLUSTER_SIZE - 1;
    }

    num_nodes = nodes_send_hi - nodes_send_lo + 1;

    node_peer = shard.begin();
    advance(node_peer, nodes_send_lo);

    for (unsigned int j = 0; j < num_nodes; j++) {
      m_shardSenders.back().emplace_back(std::get<SHARD_NODE_PEER>(*node_peer));
      node_peer++;
    }
  }
}

VectorOfPoWSoln DirectoryService::SortPoWSoln(const MapOfPubKeyPoW& mapOfPoWs,
                                              bool trimBeyondCommSize) {
  std::map<array<unsigned char, 32>, PubKey> PoWOrderSorter;
  for (const auto& powsoln : mapOfPoWs) {
    PoWOrderSorter[powsoln.second.result] = powsoln.first;
  }

  // Put it back to vector for easy manipulation and adjustment of the ordering
  VectorOfPoWSoln sortedPoWSolns;
  if (trimBeyondCommSize && (COMM_SIZE > 0)) {
    const unsigned int numNodesTotal = PoWOrderSorter.size();
    const unsigned int numNodesTrimmed =
        (numNodesTotal < COMM_SIZE)
            ? numNodesTotal
            : numNodesTotal - (numNodesTotal % COMM_SIZE);

    LOG_GENERAL(INFO, "Trimming the solutions sorted list from "
                          << numNodesTotal << " to " << numNodesTrimmed
                          << " to avoid going over COMM_SIZE " << COMM_SIZE);

    unsigned int count = 0;
    if (!GUARD_MODE) {
      for (auto kv = PoWOrderSorter.begin();
           (kv != PoWOrderSorter.end()) && (count < numNodesTrimmed);
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
      unsigned int trimmedGuardCount =
          ceil(numNodesTrimmed * ConsensusCommon::TOLERANCE_FRACTION);
      unsigned int trimmedNonGuardCount = numNodesTrimmed - trimmedGuardCount;

      if (trimmedGuardCount + trimmedNonGuardCount < numNodesTrimmed) {
        LOG_GENERAL(WARNING,
                    "Network has less than 1/3 non shard guard node. Filling "
                    "it with guard nodes");
        trimmedGuardCount +=
            (numNodesTrimmed - trimmedGuardCount - trimmedNonGuardCount);
      }

      // Assign all shard guards first
      std::map<array<unsigned char, 32>, PubKey> FilteredPoWOrderSorter;
      std::map<array<unsigned char, 32>, PubKey> ShadowPoWOrderSorter =
          PoWOrderSorter;

      // Add shard guards to "FilteredPoWOrderSorter"
      // Remove it from "ShadowPoWOrderSorter"
      for (auto kv = PoWOrderSorter.begin();
           (kv != PoWOrderSorter.end()) && (count < numNodesTrimmed); kv++) {
        if (Guard::GetInstance().IsNodeInShardGuardList(kv->second)) {
          if (count == trimmedGuardCount) {
            LOG_GENERAL(
                INFO,
                "Did not manage to form max number of shard. Only allowed "
                    << trimmedGuardCount << " shard guards");
            break;
          }
          FilteredPoWOrderSorter.emplace(*kv);
          ShadowPoWOrderSorter.erase(kv->first);
          count++;
        }
      }

      // Assign non shard guards if there is any slots
      for (auto kv = ShadowPoWOrderSorter.begin();
           (kv != ShadowPoWOrderSorter.end()) && (count < numNodesTrimmed);
           kv++) {
        FilteredPoWOrderSorter.emplace(*kv);
        count++;
      }

      // Sort "FilteredPoWOrderSorter" and stored it in "sortedPoWSolns"
      for (auto kv : FilteredPoWOrderSorter) {
        sortedPoWSolns.emplace_back(kv);
      }
      LOG_GENERAL(INFO, "trimmedGuardCount: "
                            << trimmedGuardCount
                            << " trimmedNonGuardCount: " << trimmedNonGuardCount
                            << " Total number of accepted soln: "
                            << sortedPoWSolns.size());
    }

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

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I am the leader DS node. Creating DS block.");

  lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
  lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

  auto sortedDSPoWSolns = SortPoWSoln(m_allDSPoWs);
  auto sortedPoWSolns = SortPoWSoln(m_allPoWs, true);

  map<PubKey, Peer> powDSWinners;
  MapOfPubKeyPoW dsWinnerPoWs;
  uint8_t dsDifficulty = 0;
  uint8_t difficulty = 0;
  uint64_t blockNum = 0;
  BlockHash prevHash;

  unsigned int numOfProposedDSMembers = ComputeDSBlockParameters(
      sortedDSPoWSolns, sortedPoWSolns, powDSWinners, dsWinnerPoWs,
      dsDifficulty, difficulty, blockNum, prevHash);

  InjectPoWForDSNode(sortedPoWSolns, numOfProposedDSMembers);
  if (DEBUG_LEVEL >= 5) {
    for (const auto& pairPoWKey : sortedPoWSolns) {
      LOG_GENERAL(INFO, "[POWS]"
                            << DataConversion::charArrToHexStr(pairPoWKey.first)
                            << " " << pairPoWKey.second);
    }
  }

  ClearReputationOfNodeWithoutPoW();
  ComputeSharding(sortedPoWSolns);

  vector<Peer> proposedDSMembersInfo;
  for (const auto& proposedMember : sortedDSPoWSolns) {
    proposedDSMembersInfo.emplace_back(m_allPoWConns[proposedMember.second]);
  }

  ComputeTxnSharingAssignments(proposedDSMembersInfo);

  // Compute the DSBlockHashSet member of the DSBlockHeader
  DSBlockHashSet dsBlockHashSet;
  if (!Messenger::GetShardingStructureHash(m_shards,
                                           dsBlockHashSet.m_shardingHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetShardingStructureHash failed.");
    return false;
  }

  BlockStorage::GetBlockStorage().PutShardStructure(
      m_shards, m_mediator.m_node->m_myshardId);

  if (!Messenger::GetTxSharingAssignmentsHash(m_DSReceivers, m_shardReceivers,
                                              m_shardSenders,
                                              dsBlockHashSet.m_txSharingHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetTxSharingAssignmentsHash failed.");
    return false;
  }

  // Compute the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }

  // Assemble DS block
  // To-do: Handle exceptions.
  // TODO: Revise DS block structure
  {
    lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
    m_pendingDSBlock.reset(
        new DSBlock(DSBlockHeader(dsDifficulty, difficulty, prevHash,
                                  m_mediator.m_selfKey.second, blockNum,
                                  m_mediator.m_currentEpochNum,
                                  GetNewGasPrice(), m_mediator.m_curSWInfo,
                                  powDSWinners, dsBlockHashSet, committeeHash),
                    CoSignatures(m_mediator.m_DSCommittee->size())));
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
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
        WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am suspending myself to test viewchange (VC_TEST_DS_SUSPEND_1)");
    return false;
  }
#endif  // VC_TEST_DS_SUSPEND_1

#ifdef VC_TEST_DS_SUSPEND_3
  if (m_mode == PRIMARY_DS && m_viewChangeCounter < 3) {
    LOG_EPOCH(
        WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am suspending myself to test viewchange (VC_TEST_DS_SUSPEND_3)");
    return false;
  }
#endif  // VC_TEST_DS_SUSPEND_3

  m_consensusObject.reset(new ConsensusLeader(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_mediator.m_selfKey.first, *m_mediator.m_DSCommittee,
      static_cast<unsigned char>(DIRECTORY),
      static_cast<unsigned char>(DSBLOCKCONSENSUS),
      NodeCommitFailureHandlerFunc(), ShardCommitFailureHandlerFunc()));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "WARNING: Unable to create consensus object");
    return false;
  }

  ConsensusLeader* cl = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());

  LOG_STATE(
      "[DSCON]["
      << std::setw(15) << std::left
      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
      << m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() + 1
      << "] BGIN");

  // Refer to Effective mordern C++. Item 32: Use init capture to move objects
  // into closures.
  auto announcementGeneratorFunc =
      [this, dsWinnerPoWs = std::move(dsWinnerPoWs)](
          vector<unsigned char>& dst, unsigned int offset,
          const uint32_t consensusID, const uint64_t blockNumber,
          const vector<unsigned char>& blockHash, const uint16_t leaderID,
          const pair<PrivKey, PubKey>& leaderKey,
          vector<unsigned char>& messageToCosign) mutable -> bool {
    return Messenger::SetDSDSBlockAnnouncement(
        dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey,
        *m_pendingDSBlock, m_shards, m_DSReceivers, m_shardReceivers,
        m_shardSenders, m_allPoWs, dsWinnerPoWs, messageToCosign);
  };

  cl->StartConsensus(announcementGeneratorFunc, BROADCAST_GOSSIP_MODE);
  return true;
}

void DirectoryService::ProcessTxnBodySharingAssignment() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::ProcessTxnBodySharingAssignment not "
                "expected to be called from LookUp node.");
    return;
  }

  bool i_am_forwarder = false;
  for (const auto& receiver : m_DSReceivers) {
    if (receiver == m_mediator.m_selfPeer) {
      m_mediator.m_node->m_txnSharingIAmSender = true;
      i_am_forwarder = true;
      break;
    }
  }

  unsigned int num_ds_nodes = m_DSReceivers.size();

  m_sharingAssignment.clear();

  if ((i_am_forwarder) && (m_mediator.m_DSCommittee->size() > num_ds_nodes)) {
    for (const auto& ds : *m_mediator.m_DSCommittee) {
      bool is_a_receiver = false;

      if (num_ds_nodes > 0) {
        for (const auto& receiver : m_DSReceivers) {
          if (ds.second == receiver) {
            is_a_receiver = true;
            break;
          }
        }
        num_ds_nodes--;
      }

      if (!is_a_receiver) {
        m_sharingAssignment.emplace_back(ds.second);
      }
    }
  }
}

bool DirectoryService::DSBlockValidator(
    const vector<unsigned char>& message, unsigned int offset,
    [[gnu::unused]] vector<unsigned char>& errorMsg, const uint32_t consensusID,
    const uint64_t blockNumber, const vector<unsigned char>& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey,
    vector<unsigned char>& messageToCosign) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::DSBlockValidator not "
                "expected to be called from LookUp node.");
    return true;
  }

  m_tempDSReceivers.clear();
  m_tempShardReceivers.clear();
  m_tempShardSenders.clear();
  m_tempShards.clear();

  lock(m_mutexPendingDSBlock, m_mutexAllPoWConns);
  lock_guard<mutex> g(m_mutexPendingDSBlock, adopt_lock);
  lock_guard<mutex> g2(m_mutexAllPoWConns, adopt_lock);

  m_pendingDSBlock.reset(new DSBlock);

  MapOfPubKeyPoW allPoWsFromLeader;
  MapOfPubKeyPoW dsWinnerPoWsFromLeader;

  if (!Messenger::GetDSDSBlockAnnouncement(
          message, offset, consensusID, blockNumber, blockHash, leaderID,
          leaderKey, *m_pendingDSBlock, m_tempShards, m_tempDSReceivers,
          m_tempShardReceivers, m_tempShardSenders, allPoWsFromLeader,
          dsWinnerPoWsFromLeader, messageToCosign)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSDSBlockAnnouncement failed.");
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

  // Check timestamp (must be greater than timestamp of last Tx block header in
  // the Tx blockchain)
  if (m_mediator.m_txBlockChain.GetBlockCount() > 0) {
    const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
    uint64_t thisDSTimestamp = m_pendingDSBlock->GetTimestamp();
    uint64_t lastTxBlockTimestamp = lastTxBlock.GetTimestamp();
    if (thisDSTimestamp <= lastTxBlockTimestamp) {
      LOG_GENERAL(WARNING, "Timestamp check failed. Last Tx Block: "
                               << lastTxBlockTimestamp
                               << " DSBlock: " << thisDSTimestamp);
      return false;
    }
  }

  // Verify the DSBlockHashSet member of the DSBlockHeader
  ShardingHash shardingHash;
  if (!Messenger::GetShardingStructureHash(m_tempShards, shardingHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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

  TxSharingHash txSharingHash;
  if (!Messenger::GetTxSharingAssignmentsHash(
          m_tempDSReceivers, m_tempShardReceivers, m_tempShardSenders,
          txSharingHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetTxSharingAssignmentsHash failed.");
    return false;
  }
  if (txSharingHash != m_pendingDSBlock->GetHeader().GetTxSharingHash()) {
    LOG_GENERAL(WARNING,
                "Tx sharing structure hash in newly received DS Block doesn't "
                "match. Calculated: "
                    << txSharingHash << " Received: "
                    << m_pendingDSBlock->GetHeader().GetTxSharingHash());
    return false;
  }

  // Verify the CommitteeHash member of the BlockHeaderBase
  CommitteeHash committeeHash;
  if (!Messenger::GetDSCommitteeHash(*m_mediator.m_DSCommittee,
                                     committeeHash)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Messenger::GetDSCommitteeHash failed.");
    return false;
  }
  if (committeeHash != m_pendingDSBlock->GetHeader().GetCommitteeHash()) {
    LOG_GENERAL(WARNING,
                "DS committee hash in newly received DS Block doesn't match. "
                "Calculated: "
                    << committeeHash << " Received: "
                    << m_pendingDSBlock->GetHeader().GetCommitteeHash());
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

  // Start to verify difficulty from DS block number 2.
  if (m_pendingDSBlock->GetHeader().GetBlockNum() > 1) {
    if (!VerifyDifficulty()) {
      return false;
    }
  }

  if (!ProcessShardingStructure(m_tempShards, m_tempPublicKeyToshardIdMap,
                                m_tempMapNodeReputation)) {
    return false;
  }

  if (!VerifyPoWOrdering(m_tempShards, allPoWsFromLeader)) {
    LOG_GENERAL(WARNING, "Failed to verify ordering");
    return false;
  }

  if (!VerifyGasPrice(m_pendingDSBlock->GetHeader().GetGasPrice())) {
    LOG_GENERAL(WARNING, "Failed to verify gas price");
    return false;
  }

  ClearReputationOfNodeWithoutPoW();
  if (!VerifyNodePriority(m_tempShards)) {
    LOG_GENERAL(WARNING, "Failed to verify node priority");
    return false;
  }
  // ProcessTxnBodySharingAssignment();

  auto func = [this]() mutable -> void {
    lock_guard<mutex> g(m_mediator.m_mutexCurSWInfo);
    if (m_mediator.m_curSWInfo != m_pendingDSBlock->GetHeader().GetSWInfo()) {
      if (UpgradeManager::GetInstance().DownloadSW()) {
        m_mediator.m_curSWInfo =
            *UpgradeManager::GetInstance().GetLatestSWInfo();
      }
    }
  };
  DetachedFunction(1, func);

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
  if (m_consensusMyID == 3) {
    LOG_EPOCH(
        WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am suspending myself to test viewchange (VC_TEST_VC_PRECHECK_1)");
    this_thread::sleep_for(chrono::seconds(45));
    return false;
  }
#endif  // VC_TEST_VC_PRECHECK_1

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "I am a backup DS node. Waiting for DS block announcement. "
            "Leader is at index  "
                << m_consensusLeaderID << " "
                << m_mediator.m_DSCommittee->at(m_consensusLeaderID).second);

  // Dummy values for now
  uint32_t consensusID = 0x0;
  m_consensusBlockHash =
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes();

  auto func = [this](const vector<unsigned char>& input, unsigned int offset,
                     vector<unsigned char>& errorMsg,
                     const uint32_t consensusID, const uint64_t blockNumber,
                     const vector<unsigned char>& blockHash,
                     const uint16_t leaderID, const PubKey& leaderKey,
                     vector<unsigned char>& messageToCosign) mutable -> bool {
    return DSBlockValidator(input, offset, errorMsg, consensusID, blockNumber,
                            blockHash, leaderID, leaderKey, messageToCosign);
  };

  m_consensusObject.reset(new ConsensusBackup(
      consensusID, m_mediator.m_currentEpochNum, m_consensusBlockHash,
      m_consensusMyID, m_consensusLeaderID, m_mediator.m_selfKey.first,
      *m_mediator.m_DSCommittee, static_cast<unsigned char>(DIRECTORY),
      static_cast<unsigned char>(DSBLOCKCONSENSUS), func));

  if (m_consensusObject == nullptr) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
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

  for (unsigned int i = 0; i < shards.size(); i++) {
    for (const auto& shardNode : shards.at(i)) {
      const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(shardNode);

      mapNodeReputation[pubKey] = std::get<SHARD_NODE_REP>(shardNode);

      auto storedMember = m_allPoWConns.find(pubKey);

      // I know the member but the member IP given by the leader is different!
      if (storedMember != m_allPoWConns.end()) {
        if (storedMember->second != std::get<SHARD_NODE_PEER>(shardNode)) {
          LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                    "IP of the member different "
                    "from what was in m_allPoWConns???");
          LOG_GENERAL(WARNING, "Stored  "
                                   << storedMember->second << " Received"
                                   << std::get<SHARD_NODE_PEER>(shardNode));
          return false;
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

void DirectoryService::RunConsensusOnDSBlock(bool isRejoin) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::RunConsensusOnDSBlock not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "Number of PoW recvd: " << m_allPoWs.size() << ", DS PoW recvd: "
                                    << m_allDSPoWs.size());

  LOG_MARKER();
  SetState(DSBLOCK_CONSENSUS_PREP);

  {
    lock_guard<mutex> h(m_mutexCoinbaseRewardees);
    m_coinbaseRewardees.clear();
  }

  {
    lock_guard<mutex> g(m_mutexAllPOW);
    LOG_STATE("[POWR][" << std::setw(15) << std::left
                        << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                        << m_allPoWs.size() << "] ");

    if (m_allPoWs.size() == 0) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "To-do: Code up the logic for if we didn't get any "
                "submissions at all");
      // throw exception();
      if (!isRejoin) {
        return;
      }
    }
  }

  m_mediator.m_node->m_txnSharingIAmSender = false;

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
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Initiated DS block view change. ");
    auto func = [this]() -> void { RunConsensusOnViewChange(); };
    DetachedFunction(1, func);
  }
}
