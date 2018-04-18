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

#include "DSBlock.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
DSBlock::DSBlock() {}

// To-do: handle exceptions. Will be deprecated.
DSBlock::DSBlock(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to init DSBlock.");
    }
}

DSBlock::DSBlock(const DSBlockHeader& header,
                 const array<unsigned char, BLOCK_SIG_SIZE>& signature)
    : m_header(header)
    , m_signature(signature)
{
}

unsigned int DSBlock::Serialize(vector<unsigned char>& dst,
                                unsigned int offset) const
{
    LOG_MARKER();

    unsigned int header_size_needed = sizeof(uint8_t) + BLOCK_HASH_SIZE
        + UINT256_SIZE + PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE
        + UINT256_SIZE + sizeof(unsigned int);
    unsigned int size_needed = header_size_needed + BLOCK_SIG_SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);
    copy(m_signature.begin(), m_signature.end(),
         dst.begin() + offset + header_size_needed);

    return size_needed;
}

int DSBlock::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    LOG_MARKER();

    try
    {
        unsigned int header_size_needed = sizeof(uint8_t) + BLOCK_HASH_SIZE
            + UINT256_SIZE + PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE
            + UINT256_SIZE + sizeof(unsigned int);

        DSBlockHeader header;
        if (header.Deserialize(src, offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
            return -1;
        }
        m_header = header;
        copy(src.begin() + offset + header_size_needed,
             src.begin() + offset + header_size_needed + BLOCK_SIG_SIZE,
             m_signature.begin());
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with DSBlock::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

unsigned int DSBlock::GetSerializedSize()
{
    unsigned int header_size_needed = sizeof(uint8_t) + BLOCK_HASH_SIZE
        + UINT256_SIZE + PUB_KEY_SIZE + PUB_KEY_SIZE + UINT256_SIZE
        + UINT256_SIZE + sizeof(unsigned int);
    unsigned int size_needed = header_size_needed + BLOCK_SIG_SIZE;

    return size_needed;
}

const DSBlockHeader& DSBlock::GetHeader() const { return m_header; }

const array<unsigned char, BLOCK_SIG_SIZE>& DSBlock::GetSignature() const
{
    return m_signature;
}

void DSBlock::SetSignature(const vector<unsigned char>& signature)
{
    assert(signature.size() == BLOCK_SIG_SIZE);
    copy(signature.begin(), signature.end(), m_signature.begin());
}

bool DSBlock::operator==(const DSBlock& block) const
{
    return ((m_header == block.m_header) && (m_signature == block.m_signature));
}

bool DSBlock::operator<(const DSBlock& block) const
{
    if (m_header < block.m_header)
    {
        return true;
    }
    else if (m_header > block.m_header)
    {
        return false;
    }
    else if (m_signature < block.m_signature)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool DSBlock::operator>(const DSBlock& block) const
{
    return !((*this == block) || (*this < block));
}