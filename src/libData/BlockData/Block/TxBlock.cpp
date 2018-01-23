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

#include "libUtils/Logger.h"
#include "TxBlock.h"

using namespace std;
using namespace boost::multiprecision;

uint32_t TxBlock::SerializeIsMicroBlockEmpty() const
{
    int ret = 0;
    for(int i = m_isMicroBlockEmpty.size() - 1; i >= 0; --i)
    {
        ret = 2 * ret + m_isMicroBlockEmpty[i];
    }
    return ret;
}

unsigned int TxBlock::Serialize(vector<unsigned char> & dst, unsigned int offset) const
{
    LOG_MARKER();

    assert(m_header.GetNumMicroBlockHashes() == m_microBlockHashes.size());

    unsigned int header_size_needed = TxBlockHeader::SIZE;
    unsigned int size_needed = header_size_needed + BLOCK_SIG_SIZE + sizeof(uint32_t) +
                               m_header.GetNumMicroBlockHashes() * TRAN_HASH_SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    unsigned int curOffset = offset + header_size_needed;

    copy(m_headerSig.begin(), m_headerSig.end(), dst.begin() + curOffset);
    curOffset += BLOCK_SIG_SIZE;

    SetNumber<uint32_t>(dst, curOffset, SerializeIsMicroBlockEmpty(), sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    
    for (unsigned int i = 0; i < m_header.GetNumMicroBlockHashes(); i++)
    {
        const TxnHash & microBlockHash = m_microBlockHashes.at(i);
        copy(microBlockHash.asArray().begin(), microBlockHash.asArray().end(),
             dst.begin() + curOffset);
        curOffset += TRAN_HASH_SIZE;
    }

    return size_needed;
}

vector<bool> TxBlock::DeserializeIsMicroBlockEmpty(uint32_t arg)
{
    vector<bool> ret;
    for (int i=0; i < m_header.GetNumMicroBlockHashes(); ++i)
    {
        ret.push_back((bool)(arg % 2));
        arg /= 2;
    }
    return ret;
}

void TxBlock::Deserialize(const vector<unsigned char> & src, unsigned int offset)
{
    LOG_MARKER();

    unsigned int header_size_needed = TxBlockHeader::SIZE;

    TxBlockHeader header(src, offset);
    m_header = header;

    unsigned int curOffset = offset + header_size_needed;

    copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_SIG_SIZE, m_headerSig.begin());
    curOffset += BLOCK_SIG_SIZE;
    
    m_isMicroBlockEmpty = DeserializeIsMicroBlockEmpty(GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t)));
    curOffset += sizeof(uint32_t);

    for (unsigned int i = 0; i < m_header.GetNumMicroBlockHashes(); i++)
    {
        TxnHash microBlockHash;
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_HASH_SIZE,
             microBlockHash.asArray().begin());
        curOffset += TRAN_HASH_SIZE;
        m_microBlockHashes.push_back(microBlockHash);
    }
    assert(m_header.GetNumMicroBlockHashes() == m_microBlockHashes.size());    
}

unsigned int TxBlock::GetSerializedSize() const
{
    unsigned int header_size_needed = TxBlockHeader::SIZE;
    unsigned int block_size_needed = BLOCK_SIG_SIZE + sizeof(uint32_t) +
                                     (m_microBlockHashes.size() * TRAN_HASH_SIZE);

    return header_size_needed + block_size_needed;
}

unsigned int TxBlock::GetMinSize()
{
    unsigned int header_size_needed = TxBlockHeader::SIZE;

    return header_size_needed;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
TxBlock::TxBlock()
{
}

TxBlock::TxBlock(const vector<unsigned char> & src, unsigned int offset)
{
    Deserialize(src, offset);
}

TxBlock::TxBlock
    (
        const TxBlockHeader & header,
        const array<unsigned char, BLOCK_SIG_SIZE> & signature,
        const vector<bool> & isMicroBlockEmpty,
        const vector<TxnHash> & microBlockTxHashes
    ) : m_header(header), m_headerSig(signature), m_isMicroBlockEmpty(isMicroBlockEmpty), 
        m_microBlockHashes(microBlockTxHashes)
{
    assert(m_header.GetNumMicroBlockHashes() == m_microBlockHashes.size());
}

const TxBlockHeader & TxBlock::GetHeader() const
{
    return m_header;
}

const array<unsigned char, BLOCK_SIG_SIZE> & TxBlock::GetHeaderSig() const
{
    return m_headerSig;
}

const std::vector<bool> & TxBlock::GetIsMicroBlockEmpty() const
{
    return m_isMicroBlockEmpty;
}

const vector<TxnHash> & TxBlock::GetMicroBlockHashes() const
{
    return m_microBlockHashes;
}

bool TxBlock::operator==(const TxBlock & block) const
{
    return
        (
            (m_header == block.m_header) &&
            (m_headerSig == block.m_headerSig) &&
            (m_microBlockHashes == block.m_microBlockHashes)
        );
}

bool TxBlock::operator<(const TxBlock & block) const
{
    if (m_header < block.m_header)
    {
        return true;
    }
    else if (m_header > block.m_header)
    {
        return false;
    }
    else if (m_headerSig < block.m_headerSig)
    {
        return true;
    }
    else if (m_headerSig > block.m_headerSig)
    {
        return false;
    }
    else if (m_microBlockHashes < block.m_microBlockHashes)
    {
        return true;
    }
    else if (m_microBlockHashes > block.m_microBlockHashes)
    {
        return false;
    }
    else
    {
        return false;
    }
}

bool TxBlock::operator>(const TxBlock & block) const
{
    return !((*this == block) || (*this < block));
}
