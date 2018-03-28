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

#ifndef __PEERMANAGER_H__
#define __PEERMANAGER_H__

#include <array>
#include <vector>

#include "PeerStore.h"
#include "common/Broadcastable.h"
#include "common/Executable.h"

/// Processes messages related to PeerStore management.
class PeerManager : public Executable, public Broadcastable
{
    std::pair<PrivKey, PubKey> m_selfKey;
    Peer m_selfPeer;

    bool ProcessHello(const std::vector<unsigned char>& message,
                      unsigned int offset, const Peer& from);
    bool ProcessAddPeer(const std::vector<unsigned char>& message,
                        unsigned int offset, const Peer& from);
    bool ProcessPing(const std::vector<unsigned char>& message,
                     unsigned int offset, const Peer& from);
    bool ProcessPingAll(const std::vector<unsigned char>& message,
                        unsigned int offset, const Peer& from);
    bool ProcessBroadcast(const std::vector<unsigned char>& message,
                          unsigned int offset, const Peer& from);

public:
    enum InstructionType : unsigned char
    {
        HELLO = 0x00,
        ADDPEER = 0x01,
        PING = 0x02,
        PINGALL = 0x03,
        BROADCAST = 0x04,
    };

    /// Constructor.
    PeerManager(const std::pair<PrivKey, PubKey>& key, const Peer& peer,
                bool loadConfig);

    /// Destructor.
    ~PeerManager();

    /// Implements the Execute function inherited from Executable.
    bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
                 const Peer& from);

    /// Implements the GetBroadcastList function inherited from Broadcastable.
    std::vector<Peer> GetBroadcastList(unsigned char ins_type,
                                       const Peer& broadcast_originator);
};

#endif // __PEERMANAGER_H__