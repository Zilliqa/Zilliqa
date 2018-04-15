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

#include "MicroBlock.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int MicroBlock::Serialize(vector<unsigned char>& dst,
                                   unsigned int offset) const
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());

    unsigned int size_needed = MicroBlockHeader::HEADER_SIZE_NEEDED
        + BLOCK_SIG_SIZE
        + BitVector::GetBitVectorSerializedSize(m_headerSigBitmap.size())
        + m_header.GetNumTxs() * TRAN_HASH_SIZE;

    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    unsigned int curOffset = offset + MicroBlockHeader::HEADER_SIZE_NEEDED;

    m_headerSig.Serialize(dst, curOffset);
    curOffset += BLOCK_SIG_SIZE;

    curOffset += BitVector::SetBitVector(dst, curOffset, m_headerSigBitmap);

    for (unsigned int i = 0; i < m_header.GetNumTxs(); i++)
    {
        const TxnHash& tran_hash = m_tranHashes.at(i);
        copy(tran_hash.asArray().begin(), tran_hash.asArray().end(),
             dst.begin() + curOffset);
        curOffset += TRAN_HASH_SIZE;
    }

    return size_needed;
}

int MicroBlock::Deserialize(const vector<unsigned char>& src,
                            unsigned int offset)
{
    try
    {
        // MicroBlockHeader header(src, offset);
        MicroBlockHeader header;
        if (header.Deserialize(src, offset) != 0)
        {
            LOG_MESSAGE("Error. We failed to deserialize MicroBlockHeader.");
            return -1;
        }
        m_header = header;

        unsigned int curOffset = offset + MicroBlockHeader::HEADER_SIZE_NEEDED;

        m_headerSig.Deserialize(src, curOffset);
        curOffset += BLOCK_SIG_SIZE;

        m_headerSigBitmap = BitVector::GetBitVector(src, curOffset);
        curOffset
            += BitVector::GetBitVectorSerializedSize(m_headerSigBitmap.size());

        for (unsigned int i = 0; i < m_header.GetNumTxs(); i++)
        {
            TxnHash tranHash;
            copy(src.begin() + curOffset,
                 src.begin() + curOffset + TRAN_HASH_SIZE,
                 tranHash.asArray().begin());
            curOffset += TRAN_HASH_SIZE;
            m_tranHashes.push_back(tranHash);
        }
        assert(m_header.GetNumTxs() == m_tranHashes.size());
    }
    catch (const std::exception& e)
    {
        LOG_MESSAGE("ERROR: Error with MicroBlock::Deserialize." << ' '
                                                                 << e.what());
        return -1;
    }
    return 0;
}

unsigned int MicroBlock::GetSerializedSize() const
{
    return MicroBlockHeader::HEADER_SIZE_NEEDED + BLOCK_SIG_SIZE
        + BitVector::GetBitVectorSerializedSize(m_headerSigBitmap.size())
        + (m_tranHashes.size() * TRAN_HASH_SIZE);
}

unsigned int MicroBlock::GetMinSize()
{
    return MicroBlockHeader::HEADER_SIZE_NEEDED;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
MicroBlock::MicroBlock() {}

MicroBlock::MicroBlock(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_MESSAGE("Error. We failed to init MicroBlock.");
    }
}

MicroBlock::MicroBlock(const MicroBlockHeader& header,
                       const Signature& signature,
                       const vector<bool>& signatureBitmap,
                       const vector<TxnHash>& tranHashes)
    : m_header(header)
    , m_headerSig(signature)
    , m_headerSigBitmap(signatureBitmap)
    , m_tranHashes(tranHashes)
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());
}

const MicroBlockHeader& MicroBlock::GetHeader() const { return m_header; }

const Signature& MicroBlock::GetHeaderSig() const { return m_headerSig; }

const vector<bool>& MicroBlock::GetHeaderSigBitmap() const
{
    return m_headerSigBitmap;
}

void MicroBlock::SetHeaderSig(const Signature& signature)
{
    m_headerSig = signature;
}

void MicroBlock::SetHeaderSigBitmap(const std::vector<bool>& signatureBitmap)
{
    m_headerSigBitmap = signatureBitmap;
}

const vector<TxnHash>& MicroBlock::GetTranHashes() const
{
    return m_tranHashes;
}

bool MicroBlock::operator==(const MicroBlock& block) const
{
    return ((m_header == block.m_header) && (m_headerSig == block.m_headerSig)
            && (m_tranHashes == block.m_tranHashes));
}

bool MicroBlock::operator<(const MicroBlock& block) const
{
    if (m_header < block.m_header)
    {
        return true;
    }
    else if (m_header > block.m_header)
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

bool MicroBlock::operator>(const MicroBlock& block) const
{
    return !((*this == block) || (*this < block));
}
