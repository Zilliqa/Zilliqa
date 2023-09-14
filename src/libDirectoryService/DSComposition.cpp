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
#include "libNetwork/Blacklist.h"

using namespace std;

void UpdateDSCommitteeCompositionCore(const PubKey& selfKeyPub,
                                      DequeOfNode& dsComm,
                                      const DSBlock& dsblock) {
  MinerInfoDSComm dummy;
  UpdateDSCommitteeCompositionCore(selfKeyPub, dsComm, dsblock, dummy);
}

void UpdateDSCommitteeCompositionCore(const PubKey& selfKeyPub,
                                      DequeOfNode& dsComm,
                                      const DSBlock& dsblock,
                                      MinerInfoDSComm& minerInfo,
                                      const bool showLogs) {
  if (showLogs) {
    LOG_MARKER();
  }

  // Get the map of all pow winners from the DS Block
  const auto& NewDSMembers = dsblock.GetHeader().GetDSPoWWinners();
  unsigned int NumWinners = NewDSMembers.size();

  LOG_GENERAL(WARNING, "BZ UpdateDSCommitteeCompositionCore enter, winners: "
                           << NumWinners);

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

    if (showLogs) {
      LOG_GENERAL(
          DEBUG,
          "Shuffling non-performant node to the back of the DS Composition: "
              << RemovedNode);
    }

    // Move the candidate to the back of the committee and continue processing
    // other candidates. Only reorders the Committee. The size is not changed.
    dsComm.emplace_back(*it);
    dsComm.erase(it);

    LOG_GENERAL(WARNING, "BZ Pushing node to the end: "
                             << it->second.GetPrintableIPAddress());

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
        LOG_GENERAL(WARNING, "BZ Myself Pushing non-guard to front: "
                                 << DSPowWinner.second.GetPrintableIPAddress());
      } else {
        // Calculate the position to insert the current winner.
        it = dsComm.begin() + (Guard::GetInstance().GetNumOfDSGuard());
        // Place my node's information in front of the DS Committee Community
        // Nodes.
        LOG_GENERAL(WARNING, "BZ Myself Pushing guard to proper position: "
                                 << DSPowWinner.second.GetPrintableIPAddress());
        dsComm.emplace(it, selfKeyPub, Peer());
      }
    } else {
      if (!GUARD_MODE) {
        // Place the current winner node's information in front of the DS
        // Committee.
        dsComm.emplace_front(DSPowWinner);
        LOG_GENERAL(WARNING, "BZ Other Pushing non-guard to front: "
                                 << DSPowWinner.second.GetPrintableIPAddress());
      } else {
        // Calculate the position to insert the current winner.
        it = dsComm.begin() + (Guard::GetInstance().GetNumOfDSGuard());
        // Place the winner's information in front of the DS Committee Community
        // Nodes.
        LOG_GENERAL(WARNING, "BZ Other Pushing guard to proper position: "
                                 << DSPowWinner.second.GetPrintableIPAddress());
        dsComm.emplace(it, DSPowWinner);
      }
    }
  }

  // Print some statistics.
  unsigned int NumLosers = removeDSNodePubkeys.size();
  unsigned int NumExpiring = NumWinners - NumLosers;
  if (showLogs) {
    LOG_GENERAL(INFO, "Total winners inserted: " << NumWinners);
    LOG_GENERAL(INFO, "Total non-performant nodes re-shuffled: " << NumLosers);
    LOG_GENERAL(INFO, "Nodes expiring due to old age: " << NumExpiring);
  }

  const bool bStoreDSCommittee =
      (dsblock.GetHeader().GetBlockNum() % STORE_DS_COMMITTEE_INTERVAL) == 0;
  if (LOOKUP_NODE_MODE) {
    minerInfo.m_dsNodes.clear();
    minerInfo.m_dsNodesEjected.clear();
  }

  // Remove one node for every winner, maintaining the size of the DS Committee.
  for (uint32_t i = 0; i < NumWinners; ++i) {
    // One item is always removed every winner, with removal priority given to
    // 'loser' candidates before expiring nodes.
    if (showLogs) {
      LOG_GENERAL(DEBUG,
                  "Node dropped from DS Committee: " << dsComm.back().first);
    }

    if (LOOKUP_NODE_MODE && !bStoreDSCommittee) {
      LOG_GENERAL(WARNING, "BZ Adding ejected node: "
                               << dsComm.back().second.GetPrintableIPAddress());
      minerInfo.m_dsNodesEjected.emplace_back(dsComm.back().first);
    }

    // Remove this node from blacklist if it exists
    LOG_GENERAL(WARNING, "BZ Removing from dsComm node: "
                             << dsComm.back().second.GetPrintableIPAddress());
    Peer& p = dsComm.back().second;
    Blacklist::GetInstance().Remove({p.GetIpAddress(),p.GetListenPortHost(),p.GetNodeIndentifier()});
    dsComm.pop_back();
  }

  if (LOOKUP_NODE_MODE && bStoreDSCommittee) {
    for (const auto& dsnode : dsComm) {
      if (!Guard::GetInstance().IsNodeInDSGuardList(dsnode.first)) {
        minerInfo.m_dsNodes.emplace_back(dsnode.first);
      }
    }
  }
}
