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

#ifndef __CONSENSUSLEADER_H__
#define __CONSENSUSLEADER_H__

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <deque>

#include "ConsensusCommon.h"
#include "libCrypto/MultiSig.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/TimeLockedFunction.h"

/// Implements the functionality for the consensus committee leader.
class ConsensusLeader : public ConsensusCommon
{
    enum Action
    {
        SEND_ANNOUNCEMENT = 0x00,
        PROCESS_COMMIT,
        PROCESS_RESPONSE,
        PROCESS_FINALCOMMIT,
        PROCESS_FINALRESPONSE
    };

    // Consensus session settings
    unsigned int m_numForConsensus;
    unsigned int m_numForConsensusFailure;

    // Received commits
    std::mutex m_mutex;
    unsigned int m_commitCounter;
    std::vector<bool> m_commitMap;
    std::vector<CommitPoint> m_commitPointMap; // ordered list of commits of size = committee size
    std::vector<CommitPoint> m_commitPoints;   // unordered list of commits of size = 2/3 of committee size + 1
    unsigned int m_commitRedundantCounter;
    std::vector<bool> m_commitRedundantMap;
    std::vector<CommitPoint> m_commitRedundantPointMap; // ordered list of redundant commits of size = 1/3 of committee size
    // Generated challenge
    Challenge m_challenge;

    unsigned int m_commitFailureCounter;
    std::vector<std::vector<unsigned char>> m_commitFailureMap;

    // Received responses
    unsigned int m_responseCounter;
    std::vector<Response> m_responseDataMap;
    std::vector<Response> m_responseData;

    // Internal functions
    bool CheckState(Action action);
    bool ProcessMessageCommitCore(const std::vector<unsigned char> & commit, unsigned int offset, Action action, ConsensusMessageType returnmsgtype, State nextstate);
    bool ProcessMessageCommit(const std::vector<unsigned char> & commit, unsigned int offset);
    bool ProcessMessageCommitFailure(const std::vector<unsigned char> & commit, unsigned int offset);    
    bool GenerateChallengeMessage(std::vector<unsigned char> & challenge, unsigned int offset);
    bool ProcessMessageResponseCore(const std::vector<unsigned char> & response, unsigned int offset, Action action, ConsensusMessageType returnmsgtype, State nextstate);
    bool ProcessMessageResponse(const std::vector<unsigned char> & response, unsigned int offset);
    bool GenerateCollectiveSigMessage(std::vector<unsigned char> & collectivesig, unsigned int offset);
    bool ProcessMessageFinalCommit(const std::vector<unsigned char> & finalcommit, unsigned int offset);
    bool ProcessMessageFinalResponse(const std::vector<unsigned char> & finalresponse, unsigned int offset);

public:

    /// Constructor.
    ConsensusLeader
    (
        uint32_t consensus_id,                         // unique identifier for this consensus session
        const std::vector<unsigned char> & block_hash, // unique identifier for this consensus session
        uint16_t node_id,                              // leader's identifier (= index in some ordered lookup table shared by all nodes)
        const PrivKey & privkey,                       // leader's private key
        const std::deque<PubKey> & pubkeys,            // ordered lookup table of pubkeys for this committee (includes leader)
        const std::deque<Peer> & peer_info,            // IP addresses of all peers
        unsigned char class_byte,                      // class byte representing Executable class using this instance of ConsensusLeader
        unsigned char ins_byte                         // instruction byte representing consensus messages for the Executable class
    );

    /// Destructor.
    ~ConsensusLeader();

    /// Triggers the start of consensus on a particular message (e.g., DS block).
    bool StartConsensus(const std::vector<unsigned char> & message);

    /// Function to process any consensus message received.
    bool ProcessMessage(const std::vector<unsigned char> & message, unsigned int offset);
};

#endif // __CONSENSUSLEADER_H__
