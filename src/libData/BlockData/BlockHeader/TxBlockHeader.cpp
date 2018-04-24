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

#include "TxBlockHeader.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

TxBlockHeader::TxBlockHeader()
{
    m_blockNum = (boost::multiprecision::uint256_t)-1;
    m_viewChangeCounter = 0;
}

TxBlockHeader::TxBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init TxBlockHeader.");
    }
}

TxBlockHeader::TxBlockHeader(
    uint8_t type, uint32_t version, const uint256_t& gasLimit,
    const uint256_t& gasUsed, const BlockHash& prevHash,
    const uint256_t& blockNum, const uint256_t& timestamp,
    const TxnHash& txRootHash, const StateHash& stateRootHash, uint32_t numTxs,
    uint32_t numMicroBlockHashes, const PubKey& minerPubKey,
    const uint256_t& dsBlockNum, const BlockHash& dsBlockHeader,
    unsigned int viewChangeCounter)
    : m_type(type)
    , m_version(version)
    , m_gasLimit(gasLimit)
    , m_gasUsed(gasUsed)
    , m_prevHash(prevHash)
    , m_blockNum(blockNum)
    , m_timestamp(timestamp)
    , m_txRootHash(txRootHash)
    , m_stateRootHash(stateRootHash)
    , m_numTxs(numTxs)
    , m_numMicroBlockHashes(numMicroBlockHashes)
    , m_minerPubKey(minerPubKey)
    , m_dsBlockNum(dsBlockNum)
    , m_dsBlockHeader(dsBlockHeader)
    , m_viewChangeCounter(viewChangeCounter)
{
}

unsigned int TxBlockHeader::Serialize(vector<unsigned char>& dst,
                                      unsigned int offset) const
{
    // LOG_MARKER();

    unsigned int size_needed = TxBlockHeader::SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    unsigned int curOffset = offset;

    SetNumber<uint8_t>(dst, curOffset, m_type, sizeof(uint8_t));
    curOffset += sizeof(uint8_t);
    SetNumber<uint32_t>(dst, curOffset, m_version, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint256_t>(dst, curOffset, m_gasLimit, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_gasUsed, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_prevHash.asArray().begin(), m_prevHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += BLOCK_HASH_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_blockNum, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_timestamp, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_txRootHash.asArray().begin(), m_txRootHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += TRAN_HASH_SIZE;
    copy(m_stateRootHash.asArray().begin(), m_stateRootHash.asArray().end(),
         dst.begin() + curOffset);
    curOffset += TRAN_HASH_SIZE;
    SetNumber<uint32_t>(dst, curOffset, m_numTxs, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    SetNumber<uint32_t>(dst, curOffset, m_numMicroBlockHashes,
                        sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_minerPubKey.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    SetNumber<uint256_t>(dst, curOffset, m_dsBlockNum, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    copy(m_dsBlockHeader.asArray().begin(), m_dsBlockHeader.asArray().end(),
         dst.begin() + curOffset);
    curOffset += BLOCK_HASH_SIZE;
    SetNumber<unsigned int>(dst, curOffset, m_viewChangeCounter,
                            sizeof(unsigned int));
    curOffset += sizeof(unsigned int);
    return size_needed;
}

int TxBlockHeader::Deserialize(const vector<unsigned char>& src,
                               unsigned int offset)
{
    // LOG_MARKER();
    try
    {
        unsigned int curOffset = offset;
        m_type = GetNumber<uint8_t>(src, curOffset, sizeof(uint8_t));
        curOffset += sizeof(uint8_t);
        m_version = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_gasLimit = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        m_gasUsed = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_HASH_SIZE,
             m_prevHash.asArray().begin());
        curOffset += BLOCK_HASH_SIZE;
        m_blockNum = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        m_timestamp = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_HASH_SIZE,
             m_txRootHash.asArray().begin());
        curOffset += TRAN_HASH_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_HASH_SIZE,
             m_stateRootHash.asArray().begin());
        curOffset += TRAN_HASH_SIZE;
        m_numTxs = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        m_numMicroBlockHashes
            = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
        curOffset += sizeof(uint32_t);
        // m_minerPubKey.Deserialize(src, curOffset);
        if (m_minerPubKey.Deserialize(src, curOffset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init m_minerPubKey.");
            return -1;
        }
        curOffset += PUB_KEY_SIZE;
        m_dsBlockNum = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
        curOffset += UINT256_SIZE;
        copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_HASH_SIZE,
             m_dsBlockHeader.asArray().begin());
        curOffset += BLOCK_HASH_SIZE;
        m_viewChangeCounter
            = GetNumber<unsigned int>(src, curOffset, sizeof(unsigned int));
        curOffset += sizeof(unsigned int);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with TxBlockHeader::Deserialize." << ' '
                                                             << e.what());
        return -1;
    }
    return 0;
}

const uint8_t& TxBlockHeader::GetType() const { return m_type; }

const uint32_t& TxBlockHeader::GetVersion() const { return m_version; }

const uint256_t& TxBlockHeader::GetGasLimit() const { return m_gasLimit; }

const uint256_t& TxBlockHeader::GetGasUsed() const { return m_gasUsed; }

const BlockHash& TxBlockHeader::GetPrevHash() const { return m_prevHash; }

const uint256_t& TxBlockHeader::GetBlockNum() const { return m_blockNum; }

const uint256_t& TxBlockHeader::GetTimestamp() const { return m_timestamp; }

const TxnHash& TxBlockHeader::GetTxRootHash() const { return m_txRootHash; }

const StateHash& TxBlockHeader::GetStateRootHash() const
{
    return m_stateRootHash;
}

const uint32_t& TxBlockHeader::GetNumTxs() const { return m_numTxs; }

const uint32_t& TxBlockHeader::GetNumMicroBlockHashes() const
{
    return m_numMicroBlockHashes;
}

const PubKey& TxBlockHeader::GetMinerPubKey() const { return m_minerPubKey; }

const uint256_t& TxBlockHeader::GetDSBlockNum() const { return m_dsBlockNum; }

const BlockHash& TxBlockHeader::GetDSBlockHeader() const
{
    return m_dsBlockHeader;
}

const unsigned int TxBlockHeader::GetViewChangeCounter() const
{
    return m_viewChangeCounter;
}

bool TxBlockHeader::operator==(const TxBlockHeader& header) const
{
    return ((m_type == header.m_type) && (m_version == header.m_version)
            && (m_gasLimit == header.m_gasLimit)
            && (m_gasUsed == header.m_gasUsed)
            && (m_prevHash == header.m_prevHash)
            && (m_blockNum == header.m_blockNum)
            && (m_timestamp == header.m_timestamp)
            && (m_txRootHash == header.m_txRootHash)
            && (m_stateRootHash == header.m_stateRootHash)
            && (m_numTxs == header.m_numTxs)
            && (m_numMicroBlockHashes == header.m_numMicroBlockHashes)
            && (m_minerPubKey == header.m_minerPubKey)
            && (m_dsBlockHeader == header.m_dsBlockHeader)
            && (m_viewChangeCounter == header.m_viewChangeCounter));
}

bool TxBlockHeader::operator<(const TxBlockHeader& header) const
{
    if (m_type < header.m_type)
    {
        return true;
    }
    else if (m_type > header.m_type)
    {
        return false;
    }
    else if (m_version < header.m_version)
    {
        return true;
    }
    else if (m_version > header.m_version)
    {
        return false;
    }
    else if (m_gasLimit < header.m_gasLimit)
    {
        return true;
    }
    else if (m_gasUsed > header.m_gasUsed)
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
    else if (m_timestamp > header.m_timestamp)
    {
        return false;
    }
    else if (m_txRootHash < header.m_txRootHash)
    {
        return true;
    }
    else if (m_txRootHash > header.m_txRootHash)
    {
        return false;
    }
    else if (m_stateRootHash < header.m_stateRootHash)
    {
        return true;
    }
    else if (m_stateRootHash > header.m_stateRootHash)
    {
        return false;
    }
    else if (m_numTxs < header.m_numTxs)
    {
        return true;
    }
    else if (m_numTxs > header.m_numTxs)
    {
        return false;
    }
    else if (m_numMicroBlockHashes < header.m_numMicroBlockHashes)
    {
        return true;
    }
    else if (m_numMicroBlockHashes > header.m_numMicroBlockHashes)
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
    else if (m_dsBlockNum < header.m_dsBlockNum)
    {
        return true;
    }
    else if (m_dsBlockNum > header.m_dsBlockNum)
    {
        return false;
    }
    else if (m_dsBlockHeader < header.m_dsBlockHeader)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool TxBlockHeader::operator>(const TxBlockHeader& header) const
{
    return !((*this == header) || (*this < header));
}
