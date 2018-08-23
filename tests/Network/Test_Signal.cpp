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

#include "libNetwork/P2PComm.h"
#include "libUtils/Logger.h"
#include <csignal>
#include <thread>

#define BOOST_TEST_MODULE signal
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(test_signal)

BOOST_AUTO_TEST_CASE(test_sigint_termination)
{
    INIT_STDOUT_LOGGER();

    auto message_handler
        = [](pair<vector<unsigned char>, Peer>* message) { (void)message; };

    std::thread deferred_signal([]() {
        std::this_thread::sleep_for(100ms);
        std::raise(SIGINT);
        LOG_GENERAL(INFO, "Testing signal SIGINT sent");
    });

    P2PComm::GetInstance().RegisterExitSignal({SIGINT});

    P2PComm::GetInstance().StartMessagePump(30303, message_handler, nullptr);

    deferred_signal.join();
}

BOOST_AUTO_TEST_CASE(test_multiple_termination)
{
    INIT_STDOUT_LOGGER();

    auto message_handler
        = [](pair<vector<unsigned char>, Peer>* message) { (void)message; };

    std::thread deferred_signal([]() {
        std::this_thread::sleep_for(100ms);
        std::raise(SIGINT);
        LOG_GENERAL(INFO, "Testing signal SIGINT sent");

        LOG_GENERAL(INFO, "Not interrupted as SIGINT not registered");

        std::this_thread::sleep_for(100ms);
        std::raise(SIGTERM);
        LOG_GENERAL(INFO, "Testing signal SIGTERM sent");
    });

    P2PComm::GetInstance().RegisterExitSignal({SIGTERM});

    P2PComm::GetInstance().StartMessagePump(30303, message_handler, nullptr);

    deferred_signal.join();
}

BOOST_AUTO_TEST_SUITE_END()
