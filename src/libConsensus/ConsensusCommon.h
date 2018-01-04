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

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <deque>

#include "libCrypto/MultiSig.h"
#include "libNetwork/PeerStore.h"
#include "libUtils/TimeLockedFunction.h"

typedef std::function<bool(const std::vector<unsigned char> &)> MsgContentValidatorFunc;

unsigned int GetBitVectorLengthInBytes(unsigned int length_in_bits);
vector<bool> GetBitVector(const vector<unsigned char> & src, 
                            unsigned int offset, 
                            unsigned int expected_length);
unsigned int SetBitVector(vector<unsigned char> & dst, 
                            unsigned int offset, 
                            const vector<bool> & value);

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
        FINALCOLLECTIVESIG = 0x8
    };

    State m_state;
    const double TOLERANCE_FRACTION;
    uint32_t m_consensusID;
    std::vector<unsigned char> m_blockHash;
    uint16_t m_myID;
    PrivKey m_myPrivKey;
    std::deque<PubKey> m_pubKeys;
    std::deque<Peer> m_peerInfo;
    std::vector<unsigned char> m_message;
    unsigned char m_classByte;
    unsigned char m_insByte;

    // Generated collective signature
    Signature m_collectiveSig;

    // Response map for the generated collective signature
    std::vector<bool> m_responseMap;

    ConsensusCommon
    (
        uint32_t consensus_id,
        const std::vector<unsigned char> & block_hash,
        uint16_t my_id,
        const PrivKey & privkey,
        const std::deque<PubKey> & pubkeys,
        const std::deque<Peer> & peer_info,
        unsigned char class_byte,
        unsigned char ins_byte
    );

    ~ConsensusCommon();

    Signature SignMessage(const std::vector<unsigned char> & msg, unsigned int offset, unsigned int size);
    bool VerifyMessage(const std::vector<unsigned char> & msg, unsigned int offset, unsigned int size, const Signature & toverify, uint16_t peer_id);
    PubKey AggregateKeys(const std::vector<bool> peer_map);
    CommitPoint AggregateCommits(const std::vector<CommitPoint> & commits);
    Response AggregateResponses(const std::vector<Response> & responses);
    Signature AggregateSign(const Challenge & challenge, const Response & aggregated_response);
    Challenge GetChallenge(const std::vector<unsigned char> & msg, unsigned int offset, unsigned int size, const CommitPoint & aggregated_commit, const PubKey & aggregated_key);

public:

    virtual bool ProcessMessage(const std::vector<unsigned char> & message, unsigned int offset)
    {
        return false; // Should be implemented by ConsensusLeader and ConsensusBackup
    }

    // Function to retrieve the state of this consensus session
    State GetState() const;

    // Functions to retrieve the final collective signature and bit map
    bool RetrieveCollectiveSig(std::vector<unsigned char> & dst, unsigned int offset);
    uint16_t RetrieveCollectiveSigBitmap(std::vector<unsigned char> & dst, unsigned int offset);
};

#endif // __CONSENSUSCOMMON_H__
