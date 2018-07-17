/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
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

#ifndef __CONSENSUSUSER_H__
#define __CONSENSUSUSER_H__

#include "Consensus.h"
#include "common/Broadcastable.h"
#include "common/Executable.h"
#include <shared_mutex>

/// [TEST ONLY] Internal class for testing consensus.
class ConsensusUser : public Executable, public Broadcastable
{
private:
    bool ProcessSetLeader(const std::vector<unsigned char>& message,
                          unsigned int offset, const Peer& from);
    bool ProcessStartConsensus(const std::vector<unsigned char>& message,
                               unsigned int offset, const Peer& from);
    bool ProcessConsensusMessage(const std::vector<unsigned char>& message,
                                 unsigned int offset, const Peer& from);

    std::pair<PrivKey, PubKey> m_selfKey;
    Peer m_selfPeer;
    bool m_leaderOrBackup; // false = leader, true = backup
    std::shared_ptr<ConsensusCommon> m_consensus;

    std::mutex m_mutexProcessConsensusMessage;
    std::condition_variable cv_processConsensusMessage;

public:
    enum InstructionType : unsigned char
    {
        SETLEADER = 0x00,
        STARTCONSENSUS = 0x01,
        CONSENSUS
        = 0x02 // These are messages that ConsensusLeader or ConsensusBackup will process (transparent to user)
    };

    ConsensusUser(const std::pair<PrivKey, PubKey>& key, const Peer& peer);
    ~ConsensusUser();

    bool Execute(const std::vector<unsigned char>& message, unsigned int offset,
                 const Peer& from);

    bool MyMsgValidatorFunc(
        const std::vector<unsigned char>& message,
        std::vector<unsigned char>& errorMsg); // Needed by backup
};

#endif // __CONSENSUSUSER_H__