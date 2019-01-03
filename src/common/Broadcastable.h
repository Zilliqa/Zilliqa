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

#ifndef __BROADCASTABLE_H__
#define __BROADCASTABLE_H__

#include <vector>
#include "libNetwork/PeerStore.h"
#include "libUtils/Logger.h"

/// Specifies the interface required for classes that maintain broadcast lists.
class Broadcastable {
 public:
  /// Returns the list of destination peers for a message with the specified
  /// instruction type.
  virtual std::vector<Peer> GetBroadcastList(
      [[gnu::unused]] unsigned char ins_type,
      const Peer& broadcast_originator) {
    LOG_MARKER();
    std::vector<Peer> peers = PeerStore::GetStore().GetAllPeers();
    for (std::vector<Peer>::iterator peer = peers.begin(); peer != peers.end();
         peer++) {
      if ((peer->m_ipAddress == broadcast_originator.m_ipAddress) &&
          (peer->m_listenPortHost == broadcast_originator.m_listenPortHost)) {
        *peer = std::move(peers.back());
        peers.pop_back();
        break;
      }
    }
    LOG_GENERAL(INFO, "Number of peers to broadcast = " << peers.size());
    return peers;
  }

  /// Virtual destructor.
  virtual ~Broadcastable() {}
};

#endif  // __BROADCASTABLE_H__
