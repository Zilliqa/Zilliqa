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

#include "libNetwork/ReputationManager.h"

#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ReputationManager
#define BOOST_TEST_DYN_LINK
#include <arpa/inet.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(reputationManager)

BOOST_AUTO_TEST_CASE(test_fundamental)
{
    INIT_STDOUT_LOGGER();
    ReputationManager& rm = ReputationManager::GetInstance();
    rm.Clear();

    boost::multiprecision::uint128_t node1
        = IPConverter::ToNumericalIPFromStr("127.0.0.1");
    boost::multiprecision::uint128_t node2
        = IPConverter::ToNumericalIPFromStr("192.168.1.1");
    // Setup
    rm.AddNodeIfNotKnown(node1);
    rm.AddNodeIfNotKnown(node2);

    // Check reputation of new node
    int32_t result = rm.GetReputation(node1);
    int32_t expected = 0;
    BOOST_CHECK_MESSAGE(result == expected,
                        "Check reputation of new node. Result: "
                            + to_string(result)
                            + ". Expected: " + to_string(expected) + ".");

    // Check status of node that are not ban
    bool result_bool = rm.IsNodeBanned(node1);
    BOOST_CHECK_MESSAGE(result_bool == false,
                        "Test ban for unban node. Expected: false");

    // Check status of banned node
    LOG_GENERAL(INFO,
                IPConverter::ToStrFromNumericalIP(node1) + ": "
                    << rm.GetReputation(node1));
    rm.PunishNode(node1, ReputationManager::ScoreType::REPTHRESHHOLD - 1);
    LOG_GENERAL(INFO,
                IPConverter::ToStrFromNumericalIP(node1) + ": "
                    << rm.GetReputation(node1));
    result_bool = rm.IsNodeBanned(node1);
    BOOST_CHECK_MESSAGE(result_bool == true,
                        "Test ban for banned node. Expected: true");

    // Check new reputation
    result = rm.GetReputation(node1);
    expected = 0 + ReputationManager::ScoreType::REPTHRESHHOLD - 1
        - (ReputationManager::ScoreType::BAN_MULTIPLIER
           * ReputationManager::ScoreType::AWARD_FOR_GOOD_NODES);

    BOOST_CHECK_MESSAGE(result == expected,
                        "Check rep score after getting banned. Result: "
                            << result << ". Expected: " << expected);

    // Check ban status after awarding rep score
    rm.AwardAllNodes();
    LOG_GENERAL(INFO,
                IPConverter::ToStrFromNumericalIP(node1) + ": "
                    << rm.GetReputation(node1));
    BOOST_CHECK_MESSAGE(
        rm.IsNodeBanned(node1) == true,
        IPConverter::ToStrFromNumericalIP(node1)
            << " ban status after 1 round of reward) expected to be "
               "true but false was obtained.");

    LOG_GENERAL(INFO,
                IPConverter::ToStrFromNumericalIP(node2) + ": "
                    << rm.GetReputation(node1));
    BOOST_CHECK_MESSAGE(
        rm.IsNodeBanned(node2) == false,
        IPConverter::ToStrFromNumericalIP(node2)
            << " ban status after 1 round of reward) expected to be "
               "false but true was obtained.");
    /**
    // Test 4
    rm.IsNodeBanned((boost::multiprecision::uint128_t)1);
    rm.IsNodeBanned((boost::multiprecision::uint128_t)2);
    BOOST_CHECK(rm.GetReputation((boost::multiprecision::uint128_t)1)
                <= ScoreType::REPTHRESHHOLD + AWARD_FOR_GOOD_NODES);

    // Reward lots of time. Check upper bound.
    for (int i = 0; 1 < 1000; i++)
    {
        rm.AwardAllNodes();
    }

    // Check lower bound
    rm.PunishNode((boost::multiprecision::uint128_t)1);

    // Clean up
    rm.Clear();
    **/
}

BOOST_AUTO_TEST_SUITE_END()
