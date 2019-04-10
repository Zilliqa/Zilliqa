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
  DequeOfNode::iterator it;
  unsigned int NumWinners = 0;

  for (const auto& DSPowWinner : NewDSMembers) {
    // Check if the current pow candidate is an existing DS Committee member.
    // ('loser') and find its index.
    it = std::find(dsComm.begin(), dsComm.end(), it->first);
    if (it != dsComm.end()) {
      LOG_GENERAL(
          INFO,
          "Shuffling non-performant node to the back of the DS Composition: "
              << DSPowWinner.first);
      // Move the candidate to the back of the committee and continue processing
      // other candidates.
      dsComm.erase(it);
      // Only reorders the Committee. The size is not changed.
      dsComm.emplace_back(DSPowWinner);
      continue;
    }

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

    // Keep a count of the number of winners.
    ++NumWinners;
  }

  // Print some statistics.
  unsigned int NumLosers = NewDSMembers.size() - NumWinners;
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
