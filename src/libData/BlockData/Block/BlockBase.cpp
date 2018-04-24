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

#include "BlockBase.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

BlockBase::BlockBase() {}

unsigned int BlockBase::GetSerializedSize() const
{
    return BLOCK_SIG_SIZE
        + BitVector::GetBitVectorSerializedSize(m_cosigs.m_B1.size())
        + BLOCK_SIG_SIZE
        + BitVector::GetBitVectorSerializedSize(m_cosigs.m_B2.size());
}

unsigned int BlockBase::GetMinSize()
{
    return BLOCK_SIG_SIZE + BitVector::GetBitVectorSerializedSize(1)
        + BLOCK_SIG_SIZE + BitVector::GetBitVectorSerializedSize(1);
}

unsigned int BlockBase::Serialize(vector<unsigned char>& dst,
                                  unsigned int offset) const
{
    unsigned int curOffset = offset;

    m_cosigs.m_CS1.Serialize(dst, curOffset);
    curOffset += BLOCK_SIG_SIZE;

    curOffset += BitVector::SetBitVector(dst, curOffset, m_cosigs.m_B1);

    m_cosigs.m_CS2.Serialize(dst, curOffset);
    curOffset += BLOCK_SIG_SIZE;

    curOffset += BitVector::SetBitVector(dst, curOffset, m_cosigs.m_B2);

    return curOffset - offset;
}

int BlockBase::Deserialize(const vector<unsigned char>& src,
                           unsigned int offset)
{
    try
    {
        m_cosigs.m_CS1.Deserialize(src, offset);
        offset += BLOCK_SIG_SIZE;

        m_cosigs.m_B1 = BitVector::GetBitVector(src, offset);
        offset += BitVector::GetBitVectorSerializedSize(m_cosigs.m_B1.size());

        m_cosigs.m_CS2.Deserialize(src, offset);
        offset += BLOCK_SIG_SIZE;

        m_cosigs.m_B2 = BitVector::GetBitVector(src, offset);
        offset += BitVector::GetBitVectorSerializedSize(m_cosigs.m_B2.size());
    }
    catch (const std::exception& e)
    {
        LOG_GENERAL(WARNING,
                    "Error with BlockBase::Deserialize." << ' ' << e.what());
        return -1;
    }
    return 0;
}

const Signature& BlockBase::GetCS1() const { return m_cosigs.m_CS1; }

const vector<bool>& BlockBase::GetB1() const { return m_cosigs.m_B1; }

const Signature& BlockBase::GetCS2() const { return m_cosigs.m_CS2; }

const vector<bool>& BlockBase::GetB2() const { return m_cosigs.m_B2; }

void BlockBase::SetCoSignatures(const ConsensusCommon& src)
{
    m_cosigs.m_CS1 = src.GetCS1();
    m_cosigs.m_B1 = src.GetB1();
    m_cosigs.m_CS2 = src.GetCS2();
    m_cosigs.m_B2 = src.GetB2();
}