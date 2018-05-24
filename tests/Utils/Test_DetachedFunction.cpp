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

#include "libUtils/DetachedFunction.h"
#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"
#include <memory>
#include <mutex>

#define BOOST_TEST_MODULE utils
#include <boost/test/included/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

mutex m;

void test2(shared_ptr<vector<string>> s)
{
    LOG_MARKER();

    lock_guard<mutex> guard(m);
    LOG_GENERAL(INFO, s->back().c_str());
    s->pop_back();
}

void test1()
{
    LOG_MARKER();

    shared_ptr<vector<string>> s = make_shared<vector<string>>();
    s->push_back("one");
    s->push_back("two");
    s->push_back("three");

    DetachedFunction(3, test2, s);
}

BOOST_AUTO_TEST_CASE(testDetachedFunction)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    JoinableFunction(
        1,
        test1); // check that test1 can terminate even while test2 threads are still running

    this_thread::sleep_for(chrono::seconds(
        2)); // just a short delay so test2 threads can finish before program terminates
}

BOOST_AUTO_TEST_SUITE_END()
