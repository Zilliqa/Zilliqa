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
#include "libUtils/BitVector.h"

#define BOOST_TEST_MODULE utils
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(testSizeCalculation)
{
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes(""s.size()), 0);

    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("1"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("0"s.size()), 1);

    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("11"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("111"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("1111"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("11111"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("111111"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("11111111"s.size()), 1);
    BOOST_CHECK_EQUAL(BitVector::GetBitVectorLengthInBytes("111111111"s.size()), 2);
}

BOOST_AUTO_TEST_CASE(testGetter)
{
    unsigned char nBits = 8;
    unsigned char nBytes = 1;

    std::vector<unsigned char> bitVec_8{0, nBits, 0b1010'0000};
    std::vector<bool> expected_8{true, false, true, false, false, false, false, false};
    BOOST_CHECK(BitVector::GetBitVector(bitVec_8, 0, nBytes) == expected_8);

    nBits = 7;
    std::vector<unsigned char> bitVec_7{0, nBits, 0b101'1010 << 1};
    std::vector<bool> expected_7{true, false, true, true, false, true, false};
    BOOST_CHECK(BitVector::GetBitVector(bitVec_7, 0, nBytes) == expected_7);

    nBits = 6;
    std::vector<unsigned char> bitVec_6{0, nBits, 0b11'0001 << 2};
    std::vector<bool> expected_6{true, true, false, false, false, true};
    BOOST_CHECK(BitVector::GetBitVector(bitVec_6, 0, nBytes) == expected_6);

    nBits = 5;
    std::vector<unsigned char> bitVec_5{0, nBits, 0b11'0001 << 2};
    std::vector<bool> expected_5{true, true, false, false, false, true};
    BOOST_CHECK(BitVector::GetBitVector(bitVec_6, 0, nBytes) == expected_5);
}

BOOST_AUTO_TEST_CASE(testSetter)
{
    unsigned char nBits = 8;

    std::vector<unsigned char> bitVec_8{0, nBits, 0};
    std::vector<bool> targetValue{true, false, true, false, false, false, true, true};

    BitVector::SetBitVector(bitVec_8, 0, targetValue);

    BOOST_CHECK(bitVec_8.at(2) == 0b10100011);
}

BOOST_AUTO_TEST_SUITE_END()
