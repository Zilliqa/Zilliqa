/**
* Copyright (c) 2017 Zilliqa 
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


#ifndef __ZILLIQA_H__
#define __ZILLIQA_H__

#include <vector>

#include "libConsensus/ConsensusUser.h"
#include "libDirectoryService/DirectoryService.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Peer.h"
#include "libNetwork/PeerStore.h"
#include "libNetwork/PeerManager.h"
#include "libNode/Node.h"

/// Main Zilliqa class.
class Zilliqa
{
    PeerManager m_pm;
    Mediator m_mediator;
    DirectoryService m_ds;
    Lookup m_lookup;
    Node m_n;
    ConsensusUser m_cu; // Note: This is just a test class to demo Consensus usage

public:

    /// Constructor.
    Zilliqa(const std::pair<PrivKey, PubKey> & key, const Peer & peer, bool loadConfig);

    /// Destructor.
    ~Zilliqa();

    /// Forwards an incoming message for processing by the appropriate subclass.
    void Dispatch(const std::vector<unsigned char> & message, const Peer & from);

    /// Returns a list of broadcast peers based on the specified message and instruction types.
    std::vector<Peer> RetrieveBroadcastList(unsigned char msg_type, unsigned char ins_type, const Peer & from);
};

#endif // __ZILLIQA_H__
