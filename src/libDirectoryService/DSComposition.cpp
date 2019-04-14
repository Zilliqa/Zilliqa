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

#include "DSComposition.h"

using namespace std;

void InternalUpdateDSCommitteeComposition(const PubKey& selfKeyPub,
                                          DequeOfNode& dsComm,
                                          const DSBlock& dsblock) {
  LOG_MARKER();

  // Get the map of all pow winners from the DS Block
  const auto& NewDSMembers = dsblock.GetHeader().GetDSPoWWinners();
  unsigned int NumWinners = NewDSMembers.size();

  // Get the vector of all non-performant nodes to be removed.
  const auto& removeDSNodePubkeys = dsblock.GetHeader().GetDSRemovePubKeys();

  // Shuffle the non-performant nodes to the back.
  DequeOfNode::iterator it;
  for (const auto& RemovedNode : removeDSNodePubkeys) {
    // Find the pubkey in our view of the DS Committee.
    for (it = dsComm.begin(); it != dsComm.end(); ++it) {
      if (RemovedNode == it->first) {
        break;
      }
    }
    if (it == dsComm.end()) {
      LOG_GENERAL(WARNING,
                  "[FATAL] The DS member "
                      << RemovedNode
                      << " for removal was not found in our DS Committee.");
      continue;
    }

    LOG_GENERAL(
        INFO,
        "Shuffling non-performant node to the back of the DS Composition: "
            << RemovedNode);

    // Move the candidate to the back of the committee and continue processing
    // other candidates. Only reorders the Committee. The size is not changed.
    dsComm.emplace_back(*it);
    dsComm.erase(it);

    continue;
  }

  // Add the new winners.
  for (const auto& DSPowWinner : NewDSMembers) {
    // If the current iterated winner is my node.
    if (selfKeyPub == DSPowWinner.first) {
      if (!GUARD_MODE) {
        // Place my node's information in front of the DS Committee
        // Peer() is required because my own node's network information is
        // zeroed out.
        dsComm.emplace_front(selfKeyPub, Peer());
      } else {
        // Calculate the position to insert the current winner.
        it = dsComm.begin() + (Guard::GetInstance().GetNumOfDSGuard());
        // Place my node's information in front of the DS Committee Community
        // Nodes.
        dsComm.emplace(it, selfKeyPub, Peer());
      }
    } else {
      if (!GUARD_MODE) {
        // Place the current winner node's information in front of the DS
        // Committee.
        dsComm.emplace_front(DSPowWinner);
      } else {
        // Calculate the position to insert the current winner.
        it = dsComm.begin() + (Guard::GetInstance().GetNumOfDSGuard());
        // Place the winner's information in front of the DS Committee Community
        // Nodes.
        dsComm.emplace(it, DSPowWinner);
      }
    }
  }

  // Print some statistics.
  unsigned int NumLosers = removeDSNodePubkeys.size();
  unsigned int NumExpiring = NumWinners - NumLosers;
  LOG_GENERAL(INFO, "Total winners inserted: " << NumWinners);
  LOG_GENERAL(INFO, "Total non-performant nodes re-shuffled: " << NumLosers);
  LOG_GENERAL(INFO, "Nodes expiring due to old age: " << NumExpiring);

  // Remove one node for every winner, maintaining the size of the DS Committee.
  for (uint32_t i = 0; i < NumWinners; ++i) {
    // One item is always removed every winner, with removal priority given to
    // 'loser' candidates before expiring nodes.
    LOG_GENERAL(INFO,
                "Node dropped from DS Committee: " << dsComm.back().first);
    dsComm.pop_back();
  }
}

unsigned int InternalDetermineByzantineNodes(
    unsigned int numOfProposedDSMembers,
    std::vector<PubKey>& removeDSNodePubkeys, uint64_t currentEpochNum,
    unsigned int numOfFinalBlock, double performanceThreshold,
    unsigned int maxByzantineRemoved, DequeOfNode& dsComm,
    std::map<PubKey, uint32_t>& dsMemberPerformance) {
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

void InternalSaveDSPerformance(
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

  // Go through the coinbase rewardees and tally the number of co-sigs.
  // For each TX epoch,
  for (auto const& epochNum : coinbaseRewardees) {
    // Find the DS Shard.
    for (auto const& shard : epochNum.second) {
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
