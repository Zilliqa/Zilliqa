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
        LOG_GENERAL(WARNING, "Error. We failed to initialize VCBlock.");
    }
}

VCBlock::VCBlock(VCBlockHeader&& header, CoSignatures&& cosigs)
    : m_header(move(header))
{
    m_cosigs = move(cosigs);
}

unsigned int VCBlock::Serialize(vector<unsigned char>& dst,
                                unsigned int offset) const
{
    unsigned int size_needed = GetSerializedSize();
    unsigned int size_remaining = dst.size() - offset;

    if (size_remaining < size_needed)
    {
        dst.resize(size_needed + offset);
    }

    m_header.Serialize(dst, offset);

    BlockBase::Serialize(dst, offset + VCBlockHeader::SIZE);

    return size_needed;
}

int VCBlock::Deserialize(const vector<unsigned char>& src, unsigned int offset)
{
    LOG_MARKER();
    try
    {
        VCBlockHeader header;
        if (header.Deserialize(src, offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
            return -1;
        }
        m_header = move(header);

        BlockBase::Deserialize(src, offset + VCBlockHeader::SIZE);
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "ERROR: Error with VCBlock::Deserialize." << ' '
                                                              << e.what());
        return -1;
    }
    return 0;
}

unsigned int VCBlock::GetSerializedSize() const
{
    return VCBlockHeader::SIZE + BlockBase::GetSerializedSize();
}

unsigned int VCBlock::GetMinSize()
{
    return VCBlockHeader::SIZE + BlockBase::GetMinSize();
}

const VCBlockHeader& VCBlock::GetHeader() const { return m_header; }

bool VCBlock::operator==(const VCBlock& block) const
{
    return (m_header == block.m_header);
}

bool VCBlock::operator<(const VCBlock& block) const
{
    return m_header < block.m_header;
}

bool VCBlock::operator>(const VCBlock& block) const
{
    return !((*this == block) || (*this < block));
}
