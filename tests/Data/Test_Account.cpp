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

#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include <array>
#include <string>

#define BOOST_TEST_MODULE accounttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(accounttest)

BOOST_AUTO_TEST_CASE(test1)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    PubKey pubKey = Schnorr::GetInstance().GenKeyPair().second;

    Account acc1(100, 0);

    acc1.IncreaseBalance(10);
    acc1.DecreaseBalance(120);
    LOG_GENERAL(INFO, "Account1 balance: " << acc1.GetBalance());

    std::vector<unsigned char> message1;
    acc1.Serialize(message1, 0);

    LOG_PAYLOAD(INFO, "Account1 serialized", message1,
                Logger::MAX_BYTES_TO_DISPLAY)

    Account acc2(message1, 0);

    std::vector<unsigned char> message2;
    acc2.Serialize(message2, 0);
    LOG_PAYLOAD(INFO, "Account2 serialized", message2,
                Logger::MAX_BYTES_TO_DISPLAY);

    boost::multiprecision::uint256_t acc2Balance = acc2.GetBalance();
    LOG_GENERAL(INFO, "Account2 balance: " << acc2Balance);
    BOOST_CHECK_MESSAGE(acc2Balance == 110,
                        "expected: " << 100 << " actual: " << acc2Balance
                                     << "\n");
}

BOOST_AUTO_TEST_SUITE_END()
