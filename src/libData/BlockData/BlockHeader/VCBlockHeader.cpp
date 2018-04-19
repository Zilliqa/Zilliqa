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

#include "VCBlockHeader.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

VCBlockHeader::VCBlockHeader() { m_CandidateLeaderIndex = 1; }

VCBlockHeader::VCBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_MESSAGE("Error. We failed to initialize VCBlockHeader.");
    }
}

VCBlockHeader::VCBlockHeader(
    const boost::multiprecision::uint256_t& vieWChangeDSEpochNo,
    const boost::multiprecision::uint256_t& viewChangeEpochNo,
    const unsigned int viewChangeState,
    const unsigned int expectedCandidateLeaderIndex,
    const Peer& candidateLeaderNetworkInfo, const PubKey& candidateLeaderPubKey,
    const unsigned int vcCounter,
    const boost::multiprecision::uint256_t& timestamp)
    : m_VieWChangeDSEpochNo(vieWChangeDSEpochNo)
    , m_VieWChangeEpochNo(viewChangeEpochNo)
    , m_ViewChangeState(viewChangeState)
    , m_CandidateLeaderIndex(expectedCandidateLeaderIndex)
    , m_CandidateLeaderNetworkInfo(candidateLeaderNetworkInfo)
    , m_CandidateLeaderPubKey(candidateLeaderPubKey)
    , m_VCCounter(vcCounter)
    , m_Timestamp(timestamp)
{
}

unsigned int VCBlockHeader::Serialize(vector<unsigned char>& dst,
                                      unsigned int offset) const
{
    LOG_MARKER();

    unsigned int size_needed = VCBlockHeader::SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;
    SetNumber<uint256_t>(dst, curOffset, m_VieWChangeDSEpochNo, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_VieWChangeEpochNo, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<unsigned char>(dst, curOffset, m_ViewChangeState,
                             sizeof(unsigned char));
    curOffset += sizeof(unsigned char);
    SetNumber<unsigned int>(dst, curOffset, m_CandidateLeaderIndex,
                            sizeof(unsigned int));
    curOffset += sizeof(unsigned int);
    curOffset += m_CandidateLeaderNetworkInfo.Serialize(dst, curOffset);
    curOffset += m_CandidateLeaderPubKey.Serialize(dst, curOffset);
    SetNumber<unsigned int>(dst, curOffset, m_VCCounter, sizeof(unsigned int));
    curOffset += sizeof(unsigned int);
    SetNumber<uint256_t>(dst, curOffset, m_Timestamp, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    return size_needed;
}

int VCBlockHeader::Deserialize(const vector<unsigned char>& src,
                               unsigned int offset)
{
    LOG_MARKER();

    unsigned int curOffset = offset;
    try
    {
        m_VieWChangeDSEpochNo
            = GetNumber<uint256_t>(src, curOffset, sizeof(uint256_t));
        curOffset += sizeof(uint256_t);
        m_VieWChangeEpochNo
            = GetNumber<uint256_t>(src, curOffset, sizeof(uint256_t));
        curOffset += sizeof(uint256_t);
        m_ViewChangeState
            = GetNumber<unsigned char>(src, curOffset, sizeof(unsigned char));
        curOffset += sizeof(unsigned char);

        if (m_CandidateLeaderNetworkInfo.Deserialize(src, curOffset) != 0)
        {
            LOG_MESSAGE(
                "Error. We failed to deserialize CandidateLeaderNetworkInfo.");
            return -1;
        }
        curOffset += IP_SIZE + PORT_SIZE;

        if (m_CandidateLeaderPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_MESSAGE(
                "Error. We failed to deserialize m_CandidateLeaderPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;

        m_VCCounter
            = GetNumber<unsigned int>(src, curOffset, sizeof(unsigned int));
        curOffset += sizeof(unsigned int);
        m_Timestamp = GetNumber<uint256_t>(src, curOffset, sizeof(uint256_t));
        curOffset += sizeof(uint256_t);
    }
    catch (const std::exception& e)
    {
        LOG_MESSAGE("ERROR: Error with VCBlockHeader::Deserialize."
                    << ' ' << e.what());
        return -1;
    }

    return 0;
}

const boost::multiprecision::uint256_t&
VCBlockHeader::GetVieWChangeDSEpochNo() const
{
    return m_VieWChangeDSEpochNo;
}

const boost::multiprecision::uint256_t&
VCBlockHeader::GetViewChangeEpochNo() const
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

const boost::multiprecision::uint256_t& VCBlockHeader::GetTimeStamp() const
{
    return m_Timestamp;
}

bool VCBlockHeader::operator==(const VCBlockHeader& header) const
{
    return (
        (m_VieWChangeDSEpochNo == header.m_VieWChangeDSEpochNo)
        && (m_VieWChangeEpochNo == header.m_VieWChangeEpochNo)
        && (m_ViewChangeState == header.m_ViewChangeState)
        && (m_CandidateLeaderIndex == header.m_CandidateLeaderIndex)
        && (m_CandidateLeaderNetworkInfo == header.m_CandidateLeaderNetworkInfo)
        && (m_CandidateLeaderPubKey == header.m_CandidateLeaderPubKey)
        && (m_VCCounter == header.m_VCCounter)
        && (m_Timestamp == header.m_Timestamp));
}

bool VCBlockHeader::operator<(const VCBlockHeader& header) const
{
    // To compare, first they must be of identical epochno and state
    if ((m_VieWChangeDSEpochNo == header.m_VieWChangeDSEpochNo)
        && (m_VieWChangeEpochNo == header.m_VieWChangeEpochNo)
        && (m_ViewChangeState == header.m_ViewChangeState)
        && (m_Timestamp == header.m_Timestamp)
        && (m_VCCounter < header.m_VCCounter))
    {
        return true;
    }
    else
    {
        // Cannot comparse different header or
        // it is not smaller than the header we are comparing
        return false;
    }
}

bool VCBlockHeader::operator>(const VCBlockHeader& header) const
{
    // To compare, first they must be of identical epochno and state
    if ((m_VieWChangeDSEpochNo == header.m_VieWChangeDSEpochNo)
        && (m_VieWChangeEpochNo == header.m_VieWChangeEpochNo)
        && (m_ViewChangeState == header.m_ViewChangeState)
        && (m_Timestamp == header.m_Timestamp)
        && (m_VCCounter > header.m_VCCounter))
    {
        return true;
    }
    else
    {
        // Cannot comparse different header or
        // it is not bigger than the header we are comparing
        return false;
    }
}