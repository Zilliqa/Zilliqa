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

#include "BitVector.h"

using namespace std;

unsigned int BitVector::GetBitVectorLengthInBytes(unsigned int length_in_bits)
{
    return (((length_in_bits & 0x07) > 0) ? (length_in_bits >> 3) + 1
                                          : length_in_bits >> 3);
}

unsigned int BitVector::GetBitVectorSerializedSize(unsigned int length_in_bits)
{
    return 2 + GetBitVectorLengthInBytes(length_in_bits);
}

std::vector<bool> BitVector::GetBitVector(const std::vector<unsigned char>& src,
                                          unsigned int offset,
                                          unsigned int expected_length)
{
    std::vector<bool> result;
    unsigned int actual_length = 0;
    unsigned int actual_length_bytes = 0;

    if ((src.size() - offset) >= 2)
    {
        actual_length = (src.at(offset) << 8) + src.at(offset + 1);
        actual_length_bytes = GetBitVectorLengthInBytes(actual_length);
    }

    if ((actual_length_bytes == expected_length)
        && ((src.size() - offset - 2) >= actual_length_bytes))
    {
        result.reserve(actual_length);
        for (unsigned int index = 0; index < actual_length; index++)
        {
            result.push_back(src.at(offset + 2 + (index >> 3))
                             & (1 << (7 - (index & 0x07))));
        }
    }

    return result;
}

std::vector<bool> BitVector::GetBitVector(const std::vector<unsigned char>& src,
                                          unsigned int offset)
{
    std::vector<bool> result;
    unsigned int actual_length = 0;
    unsigned int actual_length_bytes = 0;

    if ((src.size() - offset) >= 2)
    {
        actual_length = (src.at(offset) << 8) + src.at(offset + 1);
        actual_length_bytes = GetBitVectorLengthInBytes(actual_length);
    }

    if ((src.size() - offset - 2) >= actual_length_bytes)
    {
        result.reserve(actual_length);
        for (unsigned int index = 0; index < actual_length; index++)
        {
            result.push_back(src.at(offset + 2 + (index >> 3))
                             & (1 << (7 - (index & 0x07))));
        }
    }

    return result;
}

unsigned int BitVector::SetBitVector(std::vector<unsigned char>& dst,
                                     unsigned int offset,
                                     const std::vector<bool>& value)
{
    const unsigned int length_available = dst.size() - offset;
    const unsigned int length_needed = GetBitVectorSerializedSize(value.size());

    if (length_available < length_needed)
    {
        dst.resize(dst.size() + length_needed - length_available);
    }
    fill(dst.begin() + offset, dst.begin() + offset + length_needed, 0x00);

    dst.at(offset) = value.size() >> 8;
    dst.at(offset + 1) = value.size();

    unsigned int index = 0;
    for (bool b : value)
    {
        if (b)
        {
            dst.at(offset + 2 + (index >> 3)) |= (1 << (7 - (index & 0x07)));
        }
        index++;
    }

    return length_needed;
}