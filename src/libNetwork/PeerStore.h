/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/

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
class PeerStore
{
    mutable std::mutex m_mutexStore;
    std::map<PubKey, Peer> m_store;
    PeerStore();
    ~PeerStore();

public:
    /// Returns the singleton PeerStore instance.
    static PeerStore& GetStore();

    /// Adds a Peer to the table.
    void AddPeer(const PubKey& key, const Peer& peer);

    /// Returns the number of peers in the table.
    unsigned int GetPeerCount() const;

    /// Returns the Peer associated with the specified PubKey.
    Peer GetPeer(const PubKey& key);

    /// Returns a list of all peers in the table.
    std::vector<Peer> GetAllPeers() const;

    /// Returns a list of all public keys in the table.
    std::vector<PubKey> GetAllKeys() const;

    /// Removes the Peer associated with the specified PubKey from the table.
    void RemovePeer(const PubKey& key);

    /// Clears the Peer table.
    void RemoveAllPeers();
};

#endif // __PEER_STORE_H__