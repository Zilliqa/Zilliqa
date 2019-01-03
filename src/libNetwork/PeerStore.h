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

#ifndef __PEER_STORE_H__
#define __PEER_STORE_H__

#include <array>
#include <map>
#include <mutex>

#include "Peer.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"

/// Maintains the Peer-PubKey lookup table.
class PeerStore {
  mutable std::mutex m_mutexStore;
  std::map<PubKey, Peer> m_store;
  PeerStore();
  ~PeerStore();

 public:
  /// Returns the singleton PeerStore instance.
  static PeerStore& GetStore();

  /// Adds a Peer to the table.
  void AddPeerPair(const PubKey& key, const Peer& peer);

  /// Returns the number of peers in the table.
  unsigned int GetPeerCount() const;

  /// Returns the Peer associated with the specified PubKey.
  Peer GetPeer(const PubKey& key);

  /// Returns a list of all public keys and peers in the table.
  std::vector<std::pair<PubKey, Peer>> GetAllPeerPairs() const;

  /// Returns a list of all peers in the table.
  std::vector<Peer> GetAllPeers() const;

  /// Returns a list of all public keys in the table.
  std::vector<PubKey> GetAllKeys() const;

  /// Removes the Peer associated with the specified PubKey from the table.
  void RemovePeer(const PubKey& key);

  /// Clears the Peer table.
  void RemoveAllPeers();
};

#endif  // __PEER_STORE_H__
