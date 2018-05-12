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

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "ConsensusCommon.h"
#include "libCrypto/MultiSig.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/TimeLockedFunction.h"

typedef std::function<bool(const vector<unsigned char>& errorMsg, unsigned int,
                           const Peer& from)>
    NodeCommitFailureHandlerFunc;
typedef std::function<bool(std::map<unsigned int, std::vector<unsigned char>>)>
    ShardCommitFailureHandlerFunc;

/// Implements the functionality for the consensus committee leader.
class ConsensusLeader : public ConsensusCommon
{
    enum Action
    {
        SEND_ANNOUNCEMENT = 0x00,
        PROCESS_COMMITFAILURE,
        PROCESS_COMMIT,
        PROCESS_RESPONSE,
        PROCESS_FINALCOMMIT,
        PROCESS_FINALRESPONSE,
    };

    friend std::ostream& operator<<(std::ostream& out, const Action value);

    // Timer values
    static const unsigned int COMMIT_WINDOW_IN_SECONDS = 60;
    static const unsigned int DELAY_BEFORE_STARTING_SUBSET_IN_SECONDS = 5;

    // Consensus subset count
    // Each subset will attempt to reach consensus with NumForConsensus() number of peers who initially committed
    static const unsigned int NUM_CONSENSUS_SUBSETS = 50;

    // Consensus session settings
    unsigned int m_numForConsensus;
    unsigned int m_numForConsensusFailure;

    // Main mutex
    std::mutex m_mutex;

    // Received commits
    unsigned int m_commitCounter;
    std::vector<CommitPoint>
        m_commitPointMap; // ordered list of commits of size = committee size
    unsigned int m_commitFailureCounter;
    std::map<unsigned int, std::vector<unsigned char>> m_commitFailureMap;

    // Tracking data for each consensus subset
    // TODO: the vectors should be replaced by more space efficient DS
    struct ConsensusSubset
    {
        std::vector<CommitPoint>
            m_commitPointMap; // Ordered list of commits of fixed size = committee size
        unsigned int m_commitOrResponseCounter;
        Challenge m_challenge; // Challenge / Finalchallenge value generated
        std::vector<Response>
            m_responseDataMap; // Ordered list of responses of fixed size = committee size
        Signature m_CS1; // Generated collective signature
        Signature m_CS2; // Generated final collective signature
        State m_state; // Subset consensus state
    };
    std::vector<ConsensusSubset> m_consensusSubsets;
    unsigned int m_numSubsetsRunning;

    // Received responses
    unsigned int m_responseCounter;
    std::vector<Response> m_responseDataMap;
    std::vector<Response> m_responseData;

    NodeCommitFailureHandlerFunc m_nodeCommitFailureHandlerFunc;
    ShardCommitFailureHandlerFunc m_shardCommitFailureHandlerFunc;

    // Internal functions
    bool CheckState(Action action);
    bool CheckStateSubset(unsigned int subsetID, Action action);
    void SetStateSubset(unsigned int subsetID, State newState);
    void GenerateConsensusSubsets();
    void StartConsensusSubsets();
    void SubsetEnded(unsigned int subsetID);
    bool ProcessMessageCommitCore(const std::vector<unsigned char>& commit,
                                  unsigned int offset, Action action,
                                  ConsensusMessageType returnmsgtype,
                                  State nextstate);
    bool ProcessMessageCommit(const std::vector<unsigned char>& commit,
                              unsigned int offset);
    bool ProcessMessageCommitFailure(const std::vector<unsigned char>& commit,
                                     unsigned int offset, const Peer& from);
    bool GenerateChallengeMessage(std::vector<unsigned char>& challenge,
                                  unsigned int offset, unsigned int subsetID,
                                  Action action);
    bool ProcessMessageResponseCore(const std::vector<unsigned char>& response,
                                    unsigned int offset, Action action,
                                    ConsensusMessageType returnmsgtype,
                                    State nextstate);
    bool ProcessMessageResponse(const std::vector<unsigned char>& response,
                                unsigned int offset);
    bool GenerateCollectiveSigMessage(std::vector<unsigned char>& collectivesig,
                                      unsigned int offset,
                                      unsigned int subsetID, Action action);
    bool
    ProcessMessageFinalCommit(const std::vector<unsigned char>& finalcommit,
                              unsigned int offset);
    bool
    ProcessMessageFinalResponse(const std::vector<unsigned char>& finalresponse,
                                unsigned int offset);

public:
    /// Constructor.
    ConsensusLeader(
        uint32_t consensus_id, // unique identifier for this consensus session
        const std::vector<unsigned char>&
            block_hash, // unique identifier for this consensus session
        uint16_t
            node_id, // leader's identifier (= index in some ordered lookup table shared by all nodes)
        const PrivKey& privkey, // leader's private key
        const std::deque<PubKey>&
            pubkeys, // ordered lookup table of pubkeys for this committee (includes leader)
        const std::deque<Peer>& peer_info, // IP addresses of all peers
        unsigned char
            class_byte, // class byte representing Executable class using this instance of ConsensusLeader
        unsigned char
            ins_byte, // instruction byte representing consensus messages for the Executable class
        NodeCommitFailureHandlerFunc nodeCommitFailureHandlerFunc,
        ShardCommitFailureHandlerFunc shardCommitFailureHandlerFunc);

    /// Destructor.
    ~ConsensusLeader();

    /// Triggers the start of consensus on a particular message (e.g., DS block).
    bool StartConsensus(const std::vector<unsigned char>& message,
                        uint32_t lengthToCosign);

    /// Function to process any consensus message received.
    bool ProcessMessage(const std::vector<unsigned char>& message,
                        unsigned int offset, const Peer& from);

    /// Returns the state of the active consensus session
    State GetState() const;
};

std::ostream& operator<<(std::ostream& out,
                         const ConsensusLeader::Action value);

#endif // __CONSENSUSLEADER_H__