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
*
* Test cases obtained from https://www.di-mgt.com.au/sha_testvectors.html
**/

#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"
#include <iomanip>

#define BOOST_TEST_MODULE sha2test
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

// Just an alloca "wrapper" to silence uint64_t to size_t conversion warnings in windows
// consider replacing alloca calls with something better though!
#define our_alloca(param__) alloca((size_t)(param__))

BOOST_AUTO_TEST_SUITE(sha2test)

BOOST_AUTO_TEST_CASE(SHA256_check_896bitsx3)
{
    const unsigned char input[]
        = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned int inputSize = strlen((const char*)input);
    vector<unsigned char> vec;
    copy(input, input + inputSize, back_inserter(vec));

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    sha2.Update(vec);
    sha2.Update(vec);
    vector<unsigned char> output = sha2.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha2.Reset();
    sha2.Update(vec);
    output = sha2.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
    is_equal = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_CASE(SHA256_check_896bitsx3_updatewithoffset)
{
    const unsigned char input[]
        = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned int inputSize = strlen((const char*)input);
    vector<unsigned char> vec;
    copy(input, input + inputSize, back_inserter(vec));

    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec, 0, inputSize);
    sha2.Update(vec, 0, inputSize);
    sha2.Update(vec, 0, inputSize);
    vector<unsigned char> output = sha2.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "50EA825D9684F4229CA29F1FEC511593E281E46A140D81E0005F8F688669A06C");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha2.Reset();
    sha2.Update(vec, 0, inputSize);
    output = sha2.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
    is_equal = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_SUITE_END()
