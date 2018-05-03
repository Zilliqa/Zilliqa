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

#include "VCBlock.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
VCBlock::VCBlock() {}

// To-do: handle exceptions. Will be deprecated.
VCBlock::VCBlock(const vector<unsigned char>& src, unsigned int offset)
{
    if (Deserialize(src, offset) != 0)
    {
        LOG_MESSAGE("Error. We failed to initialize VCBlock.");
    }
}

VCBlock::VCBlock(const VCBlockHeader& header,
                 const array<unsigned char, BLOCK_SIG_SIZE>& signature1)
    : m_header(header)
    , m_signature1(signature1)
{
}

unsigned int VCBlock::Serialize(vector<unsigned char>& dst,
                                unsigned int offset) const
{

    unsigned int size_needed = VCBlockHeader::SIZE + BLOCK_SIG_SIZE;
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    copy(m_signature1.begin(), m_signature1.end(),
         dst.begin() + offset + VCBlockHeader::SIZE);

    return size_needed;
}

int VCBlock::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{

    try
    {
        VCBlockHeader header;
        if (header.Deserialize(src, offset) != 0)
        {
            LOG_MESSAGE("Error. We failed to init DSBlockHeader.");
            return -1;
        }
        m_header = header;
        copy(src.begin() + offset + VCBlockHeader::SIZE,
             src.begin() + offset + VCBlockHeader::SIZE + BLOCK_SIG_SIZE,
             m_signature1.begin());
    }
    catch (const std::exception& e)
    {
        LOG_MESSAGE("ERROR: Error with VCBlock::Deserialize." << ' '
                                                              << e.what());
        return -1;
    }
    return 0;
}

unsigned int VCBlock::GetSerializedSize()
{
    unsigned int size_needed = VCBlockHeader::SIZE + BLOCK_SIG_SIZE;
    return size_needed;
}

const VCBlockHeader& VCBlock::GetHeader() const { return m_header; }

const array<unsigned char, BLOCK_SIG_SIZE>& VCBlock::GetSignature1() const
{
    return m_signature1;
}

const array<unsigned char, BLOCK_SIG_SIZE>& VCBlock::GetSignature2() const
{
    return m_signature2;
}

void VCBlock::SetSignature1(const vector<unsigned char>& signature1)
{
    assert(signature1.size() == BLOCK_SIG_SIZE);
    copy(signature1.begin(), signature1.end(), m_signature1.begin());
}

void VCBlock::SetSignature2(const vector<unsigned char>& signature2)
{
    assert(signature1.size() == BLOCK_SIG_SIZE);
    copy(signature2.begin(), signature2.end(), m_signature2.begin());
}

// TODO
bool VCBlock::operator==(const VCBlock& block) const
{
    // Once cosig_2 and bitmap 1 and 2 is in, update this code
    return ((m_header == block.m_header)
            && (m_signature1 == block.m_signature1));
}

//TODO
bool VCBlock::operator<(const VCBlock& block) const
{
    // Once cosig_2 and bitmap 1 and 2 is in, update this code
    if (m_header < block.m_header)
    {
        return true;
    }
    else if (m_header > block.m_header)
    {
        return false;
    }
    else if (m_signature1 < block.m_signature1)
    {
        return true;
    }
    else
    {
        return false;
    }
}

//TODO:
bool VCBlock::operator>(const VCBlock& block) const
{
    return !((*this == block) || (*this < block));
}
