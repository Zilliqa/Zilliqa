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

#ifndef __BROADCASTABLE_H__
#define __BROADCASTABLE_H__

#include "libNetwork/PeerStore.h"
#include "libUtils/Logger.h"
#include <vector>

/// Specifies the interface required for classes that maintain broadcast lists.
class Broadcastable
{
public:
    /// Returns the list of destination peers for a message with the specified instruction type.
    virtual std::vector<Peer> GetBroadcastList(unsigned char ins_type,
                                               const Peer& broadcast_originator)
    {
        LOG_MARKER();
        std::vector<Peer> peers = PeerStore::GetStore().GetAllPeers();
        for (std::vector<Peer>::iterator peer = peers.begin();
             peer != peers.end(); peer++)
        {
            if ((peer->m_ipAddress == broadcast_originator.m_ipAddress)
                && (peer->m_listenPortHost
                    == broadcast_originator.m_listenPortHost))
            {
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

#endif // __BROADCASTABLE_H__
