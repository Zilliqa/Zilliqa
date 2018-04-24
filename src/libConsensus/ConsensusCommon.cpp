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

#include "ConsensusCommon.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;

unsigned int GetBitVectorLengthInBytes(unsigned int length_in_bits)
{
    return (((length_in_bits & 0x07) > 0) ? (length_in_bits >> 3) + 1
                                          : length_in_bits >> 3);
}

vector<bool> GetBitVector(const vector<unsigned char>& src, unsigned int offset,
                          unsigned int expected_length)
{
    vector<bool> result;
    unsigned int actual_length = 0;
    unsigned int actual_length_bytes = 0;

    if ((src.size() - offset) >= 2)
    {
        actual_length = (src.at(offset) << 8) + src.at(offset + 1);
        actual_length_bytes = GetBitVectorLengthInBytes(actual_length);
    }

    if ((actual_length_bytes == expected_length)
        && ((src.size() - offset - 2) >= actual_length_bytes))
    {
        result.reserve(actual_length);
        for (unsigned int index = 0; index < actual_length; index++)
        {
            result.push_back(src.at(offset + 2 + (index >> 3))
                             & (1 << (7 - (index & 0x07))));
        }
    }

    return result;
}

unsigned int SetBitVector(vector<unsigned char>& dst, unsigned int offset,
                          const vector<bool>& value)
{
    const unsigned int length_available = dst.size() - offset;
    const unsigned int length_needed
        = 2 + GetBitVectorLengthInBytes(value.size());

    if (length_available < length_needed)
    {
        dst.resize(dst.size() + length_needed - length_available);
    }
    fill(dst.begin() + offset, dst.begin() + offset + length_needed, 0x00);

    dst.at(offset) = value.size() >> 8;
    dst.at(offset + 1) = value.size();

    unsigned int index = 0;
    for (bool b : value)
    {
        if (b)
        {
            dst.at(offset + 2 + (index >> 3)) |= (1 << (7 - (index & 0x07)));
        }
        index++;
    }

    return length_needed;
}

ConsensusCommon::ConsensusCommon(uint32_t consensus_id,
                                 const vector<unsigned char>& block_hash,
                                 uint16_t my_id, const PrivKey& privkey,
                                 const deque<PubKey>& pubkeys,
                                 const deque<Peer>& peer_info,
                                 unsigned char class_byte,
                                 unsigned char ins_byte)
    : TOLERANCE_FRACTION((double)0.667)
    , m_blockHash(block_hash)
    , m_myPrivKey(privkey)
    , m_pubKeys(pubkeys)
    , m_peerInfo(peer_info)
    , m_responseMap(pubkeys.size(), false)
{
    m_consensusID = consensus_id;
    m_myID = my_id;
    m_classByte = class_byte;
    m_insByte = ins_byte;
}

ConsensusCommon::~ConsensusCommon() {}

Signature ConsensusCommon::SignMessage(const vector<unsigned char>& msg,
                                       unsigned int offset, unsigned int size)
{
    LOG_MARKER();

    Signature signature;
    bool result = Schnorr::GetInstance().Sign(msg, offset, size, m_myPrivKey,
                                              m_pubKeys.at(m_myID), signature);
    if (result == false)
    {
        return Signature();
    }
    return signature;
}

bool ConsensusCommon::VerifyMessage(const vector<unsigned char>& msg,
                                    unsigned int offset, unsigned int size,
                                    const Signature& toverify, uint16_t peer_id)
{
    LOG_MARKER();
    bool result = Schnorr::GetInstance().Verify(msg, offset, size, toverify,
                                                m_pubKeys.at(peer_id));

    if (result == false)
    {
        LOG_GENERAL(INFO,
                    "Peer id: " << peer_id << " pubkey: 0x"
                                << DataConversion::SerializableToHexStr(
                                       m_pubKeys.at(peer_id)));
        LOG_GENERAL(INFO, "pubkeys size: " << m_pubKeys.size());
    }
    return result;
}

PubKey ConsensusCommon::AggregateKeys(const vector<bool> peer_map)
{
    LOG_MARKER();

    vector<PubKey> keys;
    deque<PubKey>::const_iterator j = m_pubKeys.begin();
    for (unsigned int i = 0; i < peer_map.size(); i++, j++)
    {
        if (peer_map.at(i) == true)
        {
            keys.push_back(*j);
        }
    }

    shared_ptr<PubKey> result = MultiSig::AggregatePubKeys(keys);
    if (result == nullptr)
    {
        return PubKey();
    }

    return *result;
}

CommitPoint
ConsensusCommon::AggregateCommits(const vector<CommitPoint>& commits)
{
    LOG_MARKER();

    shared_ptr<CommitPoint> aggregated_commit
        = MultiSig::AggregateCommits(commits);
    if (aggregated_commit == nullptr)
    {
        return CommitPoint();
    }

    return *aggregated_commit;
}

Response ConsensusCommon::AggregateResponses(const vector<Response>& responses)
{
    LOG_MARKER();

    shared_ptr<Response> aggregated_response
        = MultiSig::AggregateResponses(responses);
    if (aggregated_response == nullptr)
    {
        return Response();
    }

    return *aggregated_response;
}

Signature ConsensusCommon::AggregateSign(const Challenge& challenge,
                                         const Response& aggregated_response)
{
    LOG_MARKER();

    shared_ptr<Signature> result
        = MultiSig::AggregateSign(challenge, aggregated_response);
    if (result == nullptr)
    {
        return Signature();
    }

    return *result;
}

Challenge ConsensusCommon::GetChallenge(const vector<unsigned char>& msg,
                                        unsigned int offset, unsigned int size,
                                        const CommitPoint& aggregated_commit,
                                        const PubKey& aggregated_key)
{
    LOG_MARKER();

    return Challenge(aggregated_commit, aggregated_key, m_message);
}

ConsensusCommon::State ConsensusCommon::GetState() const { return m_state; }

bool ConsensusCommon::RetrieveCollectiveSig(vector<unsigned char>& dst,
                                            unsigned int offset)
{
    LOG_MARKER();

    if (m_state != DONE)
    {
        LOG_GENERAL(WARNING,
                    "Retrieving collectivesig when consensus is still ongoing");
        return false;
    }

    m_collectiveSig.Serialize(dst, offset);

    return true;
}

uint16_t
ConsensusCommon::RetrieveCollectiveSigBitmap(vector<unsigned char>& dst,
                                             unsigned int offset)
{
    LOG_MARKER();

    if (m_state != DONE)
    {
        LOG_GENERAL(WARNING,
                    "Retrieving collectivesig bit map when consensus is "
                    "still ongoing");
        return 0;
    }

    return SetBitVector(dst, offset, m_responseMap);
}
