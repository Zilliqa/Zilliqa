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

#include <utility>

#include "MicroBlock.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int MicroBlock::Serialize(vector<unsigned char>& dst,
                                   unsigned int offset) const
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());

    unsigned int size_needed = GetSerializedSize();

    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    unsigned int curOffset = offset + MicroBlockHeader::SIZE;

    for (unsigned int i = 0; i < m_header.GetNumTxs(); i++)
    {
        const TxnHash& tran_hash = m_tranHashes.at(i);
        copy(tran_hash.asArray().begin(), tran_hash.asArray().end(),
             dst.begin() + curOffset);
        curOffset += TRAN_HASH_SIZE;
    }

    BlockBase::Serialize(dst, curOffset);

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
            LOG_GENERAL(WARNING, "We failed to deserialize MicroBlockHeader.");
            return -1;
        }
        m_header = header;

        unsigned int curOffset = offset + MicroBlockHeader::SIZE;

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

        BlockBase::Deserialize(src, curOffset);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with MicroBlock::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

unsigned int MicroBlock::GetSerializedSize() const
{
    return MicroBlockHeader::SIZE + (m_tranHashes.size() * TRAN_HASH_SIZE)
        + BlockBase::GetSerializedSize();
}

unsigned int MicroBlock::GetMinSize() { return MicroBlockHeader::SIZE; }

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
MicroBlock::MicroBlock() {}

MicroBlock::MicroBlock(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init MicroBlock.");
    }
}

MicroBlock::MicroBlock(MicroBlockHeader&& header,
                       const vector<TxnHash>& tranHashes, CoSignatures&& cosigs)
    : m_header(move(header))
    , m_tranHashes(tranHashes)
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());
    m_cosigs = move(cosigs);
}

MicroBlock::MicroBlock(MicroBlockHeader&& header, vector<TxnHash>&& tranHashes,
                       CoSignatures&& cosigs)
    : m_header(move(header))
    , m_tranHashes(move(tranHashes))
{
    assert(m_header.GetNumTxs() == m_tranHashes.size());
    m_cosigs = move(cosigs);
}

const MicroBlockHeader& MicroBlock::GetHeader() const { return m_header; }

const vector<TxnHash>& MicroBlock::GetTranHashes() const
{
    return m_tranHashes;
}

bool MicroBlock::operator==(const MicroBlock& block) const
{
    return ((m_header == block.m_header)
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
    else
    {
        return false;
    }
}

bool MicroBlock::operator>(const MicroBlock& block) const
{
    return !((*this == block) || (*this < block));
}
