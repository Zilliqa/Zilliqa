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

#include <array>
#include <string>

#include "libData/DataStructures/CircularArray.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE circulararraytest
#include <boost/test/included/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(circulararraytest)

BOOST_AUTO_TEST_CASE(CircularArray_test)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    CircularArray<int> arr;
    arr.resize(100);

    arr.insert_new(arr.size(), 1);
    arr.insert_new(arr.size(), 2);

    BOOST_CHECK_MESSAGE(arr[0] == 1, "arr[0] != 1!");
    BOOST_CHECK_MESSAGE(arr[1] == 2, "arr[1] != 2!");

    BOOST_CHECK_MESSAGE(arr.back() == 2, "arr.back() != 2!");

    for (int i = 0; i < 100; i++)
    {
        arr.insert_new(arr.size(), 11);
    }

    BOOST_CHECK_MESSAGE(arr[101] == 11, "arr[101] != 2!");

    arr[101] = 12;
    BOOST_CHECK_MESSAGE(arr[101] == 12, "arr[101] != 12!");
    BOOST_CHECK_MESSAGE(arr[101] != 11, "arr[101] == 11!");

    int value = -1;
    arr.insert_new(102, value);
    BOOST_CHECK_MESSAGE(arr[102] == -1, "arr[102] != -1!");

    arr.insert_new(arr.size(), 2);
    BOOST_CHECK_MESSAGE(arr[103] == 2, "arr[103] != 2!");
}

BOOST_AUTO_TEST_SUITE_END()
