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

#include "libUtils/Logger.h"
#include "libUtils/TimeLockedFunction.h"
#include <cstring>
#include <iostream>
#include <mutex>

#define BOOST_TEST_MODULE utils
#include <boost/test/included/unit_test.hpp>

using namespace std;

mutex m;
int counter;

void main_function(int count_up_to)
{
    LOG_MARKER();

    counter = 0;
    for (int i = 0; i < count_up_to; i++)
    {
        {
            lock_guard<mutex> guard(m);
            counter++;
        }
        this_thread::sleep_for(chrono::seconds(1));
    }
}

void expiry_function(int count_up_to)
{
    LOG_MARKER();

    lock_guard<mutex> guard(m);

    if (counter == count_up_to)
    {
        LOG_GENERAL(INFO,
                    "Last count = " << counter
                                    << " => main_func executed on time!");
    }
    else
    {
        LOG_GENERAL(INFO,
                    "Last count = " << counter
                                    << " => main_func executed too slow!");
    }
}

void test(int target, int delay)
{
    LOG_MARKER();

    LOG_GENERAL(
        INFO, "Test: Count to " << target << " before " << delay << " seconds");

    auto main_func = [target]() -> void { main_function(target); };
    auto expiry_func = [target]() -> void { expiry_function(target); };
    TimeLockedFunction tlf(delay, main_func, expiry_func, true);
}

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(testTimeLockedFunction)
{
    INIT_STDOUT_LOGGER();

    test(5, 4);
    test(5, 5);
    test(5, 10);
}

BOOST_AUTO_TEST_SUITE_END()
