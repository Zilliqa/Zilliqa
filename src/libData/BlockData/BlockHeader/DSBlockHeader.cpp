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

DSBlockHeader::DSBlockHeader() { m_blockNum = (uint64_t)-1; }

DSBlockHeader::DSBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
    }
}

DSBlockHeader::DSBlockHeader(const uint8_t difficulty,
                             const BlockHash& prevHash, const uint256_t& nonce,
                             const PubKey& minerPubKey,
                             const PubKey& leaderPubKey,
                             const uint64_t& blockNum,
                             const uint256_t& timestamp)
    : m_difficulty(difficulty)
    , m_prevHash(prevHash)
    , m_nonce(nonce)
    , m_minerPubKey(minerPubKey)
    , m_leaderPubKey(leaderPubKey)
    , m_blockNum(blockNum)
    , m_timestamp(timestamp)
{
}

unsigned int DSBlockHeader::Serialize(vector<unsigned char>& dst,
                                      unsigned int offset) const
{
    LOG_MARKER();

    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < SIZE)
    {
        dst.resize(SIZE + offset);
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
    SetNumber<uint64_t>(dst, curOffset, m_blockNum, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    SetNumber<uint256_t>(dst, curOffset, m_timestamp, UINT256_SIZE);
    curOffset += UINT256_SIZE;

    return SIZE;
}

int DSBlockHeader::Deserialize(const vector<unsigned char>& src,
                               unsigned int offset)
{
    LOG_MARKER();

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
            LOG_GENERAL(WARNING, "We failed to init m_minerPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        // m_leaderPubKey.Deserialize(src, curOffset);
        if (m_leaderPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init m_minerPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        m_blockNum = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
        curOffset += sizeof(uint64_t);
        m_timestamp = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with DSBlockHeader::Deserialize." << ' '
                                                             << e.what());
        return -1;
    }
    return 0;
}

const uint8_t& DSBlockHeader::GetDifficulty() const { return m_difficulty; }

const BlockHash& DSBlockHeader::GetPrevHash() const { return m_prevHash; }

const uint256_t& DSBlockHeader::GetNonce() const { return m_nonce; }

const PubKey& DSBlockHeader::GetMinerPubKey() const { return m_minerPubKey; }

const PubKey& DSBlockHeader::GetLeaderPubKey() const { return m_leaderPubKey; }

const uint64_t& DSBlockHeader::GetBlockNum() const { return m_blockNum; }

const uint256_t& DSBlockHeader::GetTimestamp() const { return m_timestamp; }

bool DSBlockHeader::operator==(const DSBlockHeader& header) const
{
    return ((m_difficulty == header.m_difficulty)
            && (m_prevHash == header.m_prevHash) && (m_nonce == header.m_nonce)
            && (m_minerPubKey == header.m_minerPubKey)
            && (m_leaderPubKey == header.m_leaderPubKey)
            && (m_blockNum == header.m_blockNum)
            && (m_timestamp == header.m_timestamp));
}

// TODO: Review this logic. It is wrong. Issue #163
bool DSBlockHeader::operator<(const DSBlockHeader& header) const
{
    if (m_difficulty < header.m_difficulty)
    {
        return true;
    }
    else if (m_difficulty > header.m_difficulty)
    {
        return false;
    }
    else if (m_prevHash < header.m_prevHash)
    {
        return true;
    }
    else if (m_prevHash > header.m_prevHash)
    {
        return false;
    }
    else if (m_nonce < header.m_nonce)
    {
        return true;
    }
    else if (m_nonce > header.m_nonce)
    {
        return false;
    }
    else if (m_minerPubKey < header.m_minerPubKey)
    {
        return true;
    }
    else if (m_minerPubKey > header.m_minerPubKey)
    {
        return false;
    }
    else if (m_leaderPubKey < header.m_leaderPubKey)
    {
        return true;
    }
    else if (m_leaderPubKey > header.m_leaderPubKey)
    {
        return false;
    }
    else if (m_blockNum < header.m_blockNum)
    {
        return true;
    }
    else if (m_blockNum > header.m_blockNum)
    {
        return false;
    }
    else if (m_timestamp < header.m_timestamp)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool DSBlockHeader::operator>(const DSBlockHeader& header) const
{
    return !((*this == header) || (*this < header));
}