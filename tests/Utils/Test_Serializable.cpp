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

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

#include "common/Serializable.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE utils
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

template<class number_type>
void test(const char* number_label, unsigned int size)
{
    LOG_MARKER();

    vector<unsigned char> v;
    number_type n = 65539;

    LOG_GENERAL(INFO, number_label << " value = " << n);

    Serializable::SetNumber<number_type>(v, 0, n, size);
    LOG_PAYLOAD(INFO, "serialized", v, Logger::MAX_BYTES_TO_DISPLAY);

    n = Serializable::GetNumber<number_type>(v, 0, size);
    LOG_GENERAL(INFO, "deserialized = " << n);
}

BOOST_AUTO_TEST_CASE(testSerializable)
{
    INIT_STDOUT_LOGGER();

    test<unsigned int>("unsigned int",
                       sizeof(unsigned int)); // native, machine-dependent size
    test<uint32_t>("uint32_t", sizeof(uint32_t)); // cstdint, fixed size
    test<boost::multiprecision::uint256_t>("uint256_t",
                                           32); // boost, fixed size
}

BOOST_AUTO_TEST_SUITE_END()
