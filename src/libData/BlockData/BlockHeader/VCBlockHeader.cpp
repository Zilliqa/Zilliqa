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

#include "DSBlockHeader.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

VCBlockHeader::VCBlockHeader()
{
    m_CandidateLeaderIndex = 1; 
}

VCBlockHeader::VCBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_MESSAGE("Error. We failed to init VCBlockHeader.");
    }
}

VCBlockHeader::VCBlockHeader(const uint8_t difficulty,
                             const BlockHash& prevHash, const uint256_t& nonce,
                             const PubKey& minerPubKey,
                             const PubKey& leaderPubKey,
                             const uint256_t& blockNum,
                             const uint256_t& timestamp,
                             const unsigned int viewChangeCount)
    : m_difficulty(difficulty)
    , m_prevHash(prevHash)
    , m_nonce(nonce)
    , m_minerPubKey(minerPubKey)
    , m_leaderPubKey(leaderPubKey)
    , m_blockNum(blockNum)
    , m_timestamp(timestamp)
    , m_viewChangeCounter(viewChangeCount)
{
}

unsigned int VCBlockHeader::Serialize(vector<unsigned char>& dst,
                                      unsigned int offset) const
{
    LOG_MARKER();
    /*
    unsigned int size_needed = sizeof(uint8_t) + BLOCK_HASH_SIZE + UINT256_SIZE
        + PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE + UINT256_SIZE
        + sizeof(unsigned int);
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    SetNumber<uint8_t>(dst, curOffset, m_difficulty, sizeof(uint8_t));
    curOffset += sizeof(uint8_t);
    copy(m_prevHash.asArray().begin(), m_prevHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += BLOCK_HASH_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_nonce, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    m_minerPubKey.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    m_leaderPubKey.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_blockNum, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_timestamp, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<unsigned int>(dst, curOffset, m_viewChangeCounter,
                            sizeof(unsigned int));
    curOffset += sizeof(unsigned int);

    return size_needed;
    */
    return 1; 
}

int VCBlockHeader::Deserialize(const vector<unsigned char>& src,
                               unsigned int offset)
{
    LOG_MARKER();
    /*
    unsigned int curOffset = offset;
    try
    {
        m_difficulty = GetNumber<uint8_t>(src, curOffset, sizeof(uint8_t));
        curOffset += sizeof(uint8_t);
        copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_HASH_SIZE,
             m_prevHash.asArray().begin());
        curOffset += BLOCK_HASH_SIZE;
        m_nonce = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        // m_minerPubKey.Deserialize(src, curOffset);
        if (m_minerPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_MESSAGE("Error. We failed to init m_minerPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        // m_leaderPubKey.Deserialize(src, curOffset);
        if (m_leaderPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_MESSAGE("Error. We failed to init m_minerPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        m_blockNum = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        m_timestamp = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        m_viewChangeCounter
            = GetNumber<unsigned int>(src, curOffset, sizeof(unsigned int));
        curOffset += sizeof(unsigned int);
    }
    catch (const std::exception& e)
    {
        LOG_MESSAGE("ERROR: Error with VCBlockHeader::Deserialize."
                    << ' ' << e.what());
        return -1;
    }
    */
    return 0;
}

const unsigned int VCBlockHeader::GetViewChangeEpochNo() const 
{
    return m_VieWChangeEpochNo; 
}

const unsigned char VCBlockHeader::GetViewChangeState() const
{
    return m_ViewChangeState; 
}

const unsigned int VCBlockHeader::GetCandidateLeaderIndex() const
{
    return m_CandidateLeaderIndex; 
}

const Peer& VCBlockHeader::GetCandidateLeaderNetworkInfo() const
{
    return m_CandidateLeaderNetworkInfo; 
}   

const PubKey& VCBlockHeader::GetCandidateLeaderPubKey() const
{
    return m_CandidateLeaderPubKey; 
}

const unsigned int VCBlockHeader::GetViewChangeCounter() const
{
    return m_VCCounter; 
}

bool VCBlockHeader::operator==(const VCBlockHeader& header) const
{
    return ((m_VieWChangeEpochNo == header.m_VieWChangeEpochNo)
            && (m_ViewChangeState == header.m_ViewChangeState) 
            && (m_CandidateLeaderIndex == header.m_CandidateLeaderIndex)
            && (m_CandidateLeaderNetworkInfo == header.m_CandidateLeaderNetworkInfo)
            && (m_CandidateLeaderPubKey == header.m_CandidateLeaderPubKey)
            && (m_VCCounter == header.m_VCCounter));
}


bool VCBlockHeader::operator<(const VCBlockHeader& header) const
{
    // TODO
    return true; 

}

bool VCBlockHeader::operator>(const VCBlockHeader& header) const
{
    return !((*this == header) || (*this < header));
}