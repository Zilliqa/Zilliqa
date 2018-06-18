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

#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include <array>
#include <regex>
#include <string>
#include <vector>

#include "CrowdFundingCodes.h"

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

Address fromAddr;
Address toAddress;
KeyPair sender;
uint256_t nonce = 0;

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContract)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    AccountStore::GetInstance().Init();

    sender = Schnorr::GetInstance().GenKeyPair();

    fromAddr = Account::GetAddressFromPublicKey(sender.second);

    std::vector<unsigned char> code(cfCodeStr.begin(), cfCodeStr.end());

    toAddress = Account::GetAddressForContract(fromAddr, nonce);
    LOG_GENERAL(INFO, "CrowdFunding Address: " << toAddress);
    string initStr
        = regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + toAddress.hex());
    std::vector<unsigned char> data(initStr.begin(), initStr.end());

    Transaction tx1(1, nonce, NullAddress, sender, 0, 11, 66, code, data);

    /// Comment this part until the interpreter can be called
    AccountStore::GetInstance().UpdateAccounts(1, tx1);

    bool checkToAddr = true;
    Account* account = AccountStore::GetInstance().GetAccount(toAddress);
    if (account == nullptr)
    {
        checkToAddr = false;
    }
    else
    {
        nonce++;
    }
    BOOST_CHECK_MESSAGE(checkToAddr, "Error with creation of contract account");

    std::vector<unsigned char> data2(cfDataStr.begin(), cfDataStr.end());

    std::vector<unsigned char> vec2;
    Transaction tx2(1, nonce, toAddress, sender, 0, 11, 66, vec2, data2);
    if (AccountStore::GetInstance().UpdateAccounts(1, tx2))
    {
        nonce++;
    }

    cfOutStr.erase(std::remove(cfOutStr.begin(), cfOutStr.end(), ' '),
                   cfOutStr.end());
    cfOutStr.erase(std::remove(cfOutStr.begin(), cfOutStr.end(), '\n'),
                   cfOutStr.end());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr == output_file,
                        "Error: didn't get desired output");

    std::vector<unsigned char> data3(cfDataStr3.begin(), cfDataStr3.end());

    std::vector<unsigned char> vec3;
    Transaction tx3(1, nonce, toAddress, sender, 0, 11, 66, vec3, data3);
    if (AccountStore::GetInstance().UpdateAccounts(1, tx3))
    {
        nonce++;
    }

    std::vector<unsigned char> data4(cfDataStr4.begin(), cfDataStr4.end());

    std::vector<unsigned char> vec4;
    Transaction tx4(1, nonce, toAddress, sender, 0, 11, 66, vec4, data4);
    if (AccountStore::GetInstance().UpdateAccounts(1, tx4))
    {
        nonce++;
    }
}

BOOST_AUTO_TEST_SUITE_END()
