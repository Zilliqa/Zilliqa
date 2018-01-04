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
#include "MicroBlock.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int MicroBlock::Serialize(vector<unsigned char> & dst, unsigned int offset) const
{
    LOG_MARKER();

    assert(m_header.GetNumTxs() == m_tranHashes.size());

    unsigned int header_size_needed = sizeof(uint8_t) + sizeof(uint32_t) + UINT256_SIZE + UINT256_SIZE + 
                                        BLOCK_HASH_SIZE + UINT256_SIZE + UINT256_SIZE + TRAN_HASH_SIZE + 
                                        sizeof(uint32_t) + PUB_KEY_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE;

    unsigned int size_needed = header_size_needed + BLOCK_SIG_SIZE + m_header.GetNumTxs() * TRAN_HASH_SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    unsigned int curOffset = offset + header_size_needed;

    copy(m_headerSig.begin(), m_headerSig.end(), dst.begin() + curOffset);
    curOffset += BLOCK_SIG_SIZE;
    for (unsigned int i = 0; i < m_header.GetNumTxs(); i++)
    {
        const TxnHash & tran_hash = m_tranHashes.at(i);
        copy(tran_hash.asArray().begin(), tran_hash.asArray().end(), dst.begin() + curOffset);
        curOffset += TRAN_HASH_SIZE;
    }

    return size_needed;
}

void MicroBlock::Deserialize(const vector<unsigned char> & src, unsigned int offset)
{
    LOG_MARKER();

    unsigned int header_size_needed = sizeof(uint8_t) + sizeof(uint32_t) + UINT256_SIZE + UINT256_SIZE + 
                                        BLOCK_HASH_SIZE + UINT256_SIZE + UINT256_SIZE + TRAN_HASH_SIZE + 
                                        sizeof(uint32_t) + PUB_KEY_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE;

    MicroBlockHeader header(src, offset);
    m_header = header;

    unsigned int curOffset = offset + header_size_needed;

    copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_SIG_SIZE, m_headerSig.begin());
    curOffset += BLOCK_SIG_SIZE;
    for (unsigned int i = 0; i < m_header.GetNumTxs(); i++)
    {
        TxnHash tranHash;
        copy(src.begin() + curOffset, src.begin() + curOffset + TRAN_HASH_SIZE, tranHash.asArray().begin());
        curOffset += TRAN_HASH_SIZE;
        m_tranHashes.push_back(tranHash);
    }
    assert(m_header.GetNumTxs() == m_tranHashes.size());
}

unsigned int MicroBlock::GetSerializedSize() const
{
    unsigned int header_size_needed = sizeof(uint8_t) + sizeof(uint32_t) + UINT256_SIZE + UINT256_SIZE + 
                                      BLOCK_HASH_SIZE + UINT256_SIZE + UINT256_SIZE + TRAN_HASH_SIZE + 
                                      sizeof(uint32_t) + PUB_KEY_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE;
    unsigned int block_size_needed = BLOCK_SIG_SIZE + (m_tranHashes.size() * TRAN_HASH_SIZE);

    return header_size_needed + block_size_needed;
}

unsigned int MicroBlock::GetMinSize()
{
    unsigned int header_size_needed = sizeof(uint8_t) + sizeof(uint32_t) + UINT256_SIZE + UINT256_SIZE + 
                                      BLOCK_HASH_SIZE + UINT256_SIZE + UINT256_SIZE + TRAN_HASH_SIZE + 
                                      sizeof(uint32_t) + PUB_KEY_SIZE + UINT256_SIZE + BLOCK_HASH_SIZE;

    return header_size_needed;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
MicroBlock::MicroBlock()
{

}

MicroBlock::MicroBlock(const vector<unsigned char> & src, unsigned int offset)
{
    Deserialize(src, offset);
}

MicroBlock::MicroBlock
(
    const MicroBlockHeader & header,
    const array<unsigned char, BLOCK_SIG_SIZE> & signature,
    const vector<TxnHash> & tranHashes
) : m_header(header), m_headerSig(signature), m_tranHashes(tranHashes)
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());
}

const MicroBlockHeader & MicroBlock::GetHeader() const
{
    return m_header;
}

const array<unsigned char, BLOCK_SIG_SIZE> & MicroBlock::GetHeaderSig() const
{
    return m_headerSig;
}

const vector<TxnHash> & MicroBlock::GetTranHashes() const
{
    return m_tranHashes;
}

bool MicroBlock::operator==(const MicroBlock & block) const
{
    return
        (
            (m_header == block.m_header) &&
            (m_headerSig == block.m_headerSig) &&
            (m_tranHashes == block.m_tranHashes)
        );
}

bool MicroBlock::operator<(const MicroBlock & block) const
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
    else if (m_tranHashes < block.m_tranHashes)
    {
        return true;
    }
    else if (m_tranHashes > block.m_tranHashes)
    {
        return false;
    }
    else
    {
        return false;
    }
}

bool MicroBlock::operator>(const MicroBlock & block) const
{
    return !((*this == block) || (*this < block));
}
