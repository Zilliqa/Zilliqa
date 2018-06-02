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

#ifndef __CONSENSUSCOMMON_H__
#define __CONSENSUSCOMMON_H__

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "libCrypto/MultiSig.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/TimeLockedFunction.h"

typedef std::function<bool(const std::vector<unsigned char>& input,
                           std::vector<unsigned char>& errorMsg)>
    MsgContentValidatorFunc;

/// Implements base functionality shared between all consensus committee members
class ConsensusCommon
{
public:
    enum State
    {
        INITIAL = 0x00,
        ANNOUNCE_DONE,
        COMMIT_DONE,
        CHALLENGE_DONE,
        RESPONSE_DONE,
        COLLECTIVESIG_DONE,
        FINALCOMMIT_DONE,
        FINALCHALLENGE_DONE,
        FINALRESPONSE_DONE,
        DONE,
        ERROR
    };

protected:
    enum ConsensusMessageType : unsigned char
    {
        ANNOUNCE = 0x00,
        COMMIT = 0x01,
        CHALLENGE = 0x02,
        RESPONSE = 0x03,
        COLLECTIVESIG = 0x04,
        FINALCOMMIT = 0x05,
        FINALCHALLENGE = 0x06,
        FINALRESPONSE = 0x07,
        FINALCOLLECTIVESIG = 0x08,
        COMMITFAILURE = 0x09,
        CONSENSUSFAILURE = 0x10,
    };

    /// State of the active consensus session.
    State m_state;

    /// The minimum fraction of peers necessary to achieve consensus.
    static constexpr double TOLERANCE_FRACTION = 0.667;

    /// The unique ID assigned to the active consensus session.
    uint32_t m_consensusID;

    /// [TODO] The unique block hash assigned to the active consensus session.
    std::vector<unsigned char> m_blockHash;

    /// The ID assigned to this peer (equal to its index in the peer table).
    uint16_t m_myID;

    /// Private key of this peer.
    PrivKey m_myPrivKey;

    /// List of public keys for the committee.
    std::deque<PubKey> m_pubKeys;

    /// List of peers for the committee.
    std::deque<Peer> m_peerInfo;

    /// The payload to be evaluated for the active consensus session.
    std::vector<unsigned char> m_message;

    /// The class byte value for the next consensus message to be composed.
    unsigned char m_classByte;

    /// The instruction byte value for the next consensus message to be composed.
    unsigned char m_insByte;

    /// Generated collective signature
    Signature m_collectiveSig;

    /// Response map for the generated collective signature
    std::vector<bool> m_responseMap;

    /// Co-sig for first round
    Signature m_CS1;

    /// Co-sig bitmap for first round
    std::vector<bool> m_B1;

    /// Co-sig for second round
    Signature m_CS2;

    /// Co-sig bitmap for second round
    std::vector<bool> m_B2;

    /// Length of the part of the message to co-sign
    uint32_t m_lengthToCosign;

    /// Generated commit secret
    std::shared_ptr<CommitSecret> m_commitSecret;

    /// Generated commit point
    std::shared_ptr<CommitPoint> m_commitPoint;

    /// Constructor.
    ConsensusCommon(uint32_t consensus_id,
                    const std::vector<unsigned char>& block_hash,
                    uint16_t my_id, const PrivKey& privkey,
                    const std::deque<PubKey>& pubkeys,
                    const std::deque<Peer>& peer_info, unsigned char class_byte,
                    unsigned char ins_byte);

    /// Destructor.
    ~ConsensusCommon();

    /// Generates the signature over a consensus message.
    Signature SignMessage(const std::vector<unsigned char>& msg,
                          unsigned int offset, unsigned int size);

    /// Verifies the signature attached to a consensus message.
    bool VerifyMessage(const std::vector<unsigned char>& msg,
                       unsigned int offset, unsigned int size,
                       const Signature& toverify, uint16_t peer_id);

    /// Aggregates public keys according to the response map.
    PubKey AggregateKeys(const std::vector<bool> peer_map);

    /// Aggregates the list of received commits.
    CommitPoint AggregateCommits(const std::vector<CommitPoint>& commits);

    /// Aggregates the list of received responses.
    Response AggregateResponses(const std::vector<Response>& responses);

    /// Generates the collective signature.
    Signature AggregateSign(const Challenge& challenge,
                            const Response& aggregated_response);

    /// Generates the challenge according to the aggregated commit and key.
    Challenge GetChallenge(const std::vector<unsigned char>& msg,
                           unsigned int offset, unsigned int size,
                           const CommitPoint& aggregated_commit,
                           const PubKey& aggregated_key);

public:
    /// Consensus message processing function
    virtual bool ProcessMessage(const std::vector<unsigned char>& message,
                                unsigned int offset, const Peer& from)
    {
        return false; // Should be implemented by ConsensusLeader and ConsensusBackup
    }

    /// Returns the state of the active consensus session
    State GetState() const;

    /// Returns the co-sig for first round
    const Signature& GetCS1() const;

    /// Returns the co-sig bitmap for first round
    const std::vector<bool>& GetB1() const;

    /// Returns the co-sig for second round
    const Signature& GetCS2() const;

    /// Returns the co-sig bitmap for second round
    const std::vector<bool>& GetB2() const;

    /// Returns the fraction of the shard required to achieve consensus
    static unsigned int NumForConsensus(unsigned int shardSize);
};

#endif // __CONSENSUSCOMMON_H__
