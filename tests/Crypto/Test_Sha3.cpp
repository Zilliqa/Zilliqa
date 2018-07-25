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

#include "libCrypto/Sha3.h"
#include "libUtils/DataConversion.h"
#include <iomanip>

#define BOOST_TEST_MODULE sha3test
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(sha3test)

BOOST_AUTO_TEST_CASE(SHA256_check_896bitsx3)
{
    const unsigned char input[]
        = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned int inputSize = strlen((const char*)input);
    vector<unsigned char> vec;
    copy(input, input + inputSize, back_inserter(vec));

    SHA3<HASH_TYPE::HASH_VARIANT_256> sha3;
    sha3.Update(vec);
    sha3.Update(vec);
    sha3.Update(vec);
    vector<unsigned char> output = sha3.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "30e7724208ee5cae243e7586d0021c865d55ba0a99d46ddd1da55b528baffc40");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha3.Reset();
    sha3.Update(vec);
    output = sha3.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376");
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

    SHA3<HASH_TYPE::HASH_VARIANT_256> sha3;
    sha3.Update(vec, 0, inputSize);
    sha3.Update(vec, 0, inputSize);
    sha3.Update(vec, 0, inputSize);
    vector<unsigned char> output = sha3.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "30e7724208ee5cae243e7586d0021c865d55ba0a99d46ddd1da55b528baffc40");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha3.Reset();
    sha3.Update(vec, 0, inputSize);
    output = sha3.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "41c0dba2a9d6240849100376a8235e2c82e1b9998a999e21db32dd97496d3376");
    is_equal = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_CASE(SHA512_check_896bitsx3)
{
    const unsigned char input[]
        = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned int inputSize = strlen((const char*)input);
    vector<unsigned char> vec;
    copy(input, input + inputSize, back_inserter(vec));

    SHA3<HASH_TYPE::HASH_VARIANT_512> sha3;
    sha3.Update(vec);
    sha3.Update(vec);
    sha3.Update(vec);
    vector<unsigned char> output = sha3.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "946729b1e315ec31a40467e16f9aa20ae7ef24702052369345587ec626dd8317db84e9"
        "099cdba1096f478a37d0f4f49145a31c311fdffa23f3a9bac1a8ff22a2");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha3.Reset();
    sha3.Update(vec);
    output = sha3.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "04a371e84ecfb5b8b77cb48610fca8182dd457ce6f326a0fd3d7ec2f1e91636dee691f"
        "be0c985302ba1b0d8dc78c086346b533b49c030d99a27daf1139d6e75e");
    is_equal = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_CASE(SHA512_check_896bitsx3_updatewithoffset)
{
    const unsigned char input[]
        = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned int inputSize = strlen((const char*)input);
    vector<unsigned char> vec;
    copy(input, input + inputSize, back_inserter(vec));

    SHA3<HASH_TYPE::HASH_VARIANT_512> sha3;
    sha3.Update(vec, 0, inputSize);
    sha3.Update(vec, 0, inputSize);
    sha3.Update(vec, 0, inputSize);
    vector<unsigned char> output = sha3.Finalize();

    std::vector<unsigned char> expected;
    expected = DataConversion::HexStrToUint8Vec(
        "946729b1e315ec31a40467e16f9aa20ae7ef24702052369345587ec626dd8317db84e9"
        "099cdba1096f478a37d0f4f49145a31c311fdffa23f3a9bac1a8ff22a2");
    bool is_equal
        = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);

    sha3.Reset();
    sha3.Update(vec, 0, inputSize);
    output = sha3.Finalize();
    expected = DataConversion::HexStrToUint8Vec(
        "04a371e84ecfb5b8b77cb48610fca8182dd457ce6f326a0fd3d7ec2f1e91636dee691f"
        "be0c985302ba1b0d8dc78c086346b533b49c030d99a27daf1139d6e75e");
    is_equal = std::equal(expected.begin(), expected.end(), output.begin());
    BOOST_CHECK_EQUAL(is_equal, true);
}

BOOST_AUTO_TEST_SUITE_END()
