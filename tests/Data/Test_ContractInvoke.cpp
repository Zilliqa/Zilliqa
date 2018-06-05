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
#include "InvokeCrowdFundingCodes.h"

#define BOOST_TEST_MODULE contractinvokingtest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contractinvokingtest)

Address fromAddr;
Address cfAddress, icfAddress;
KeyPair sender;
uint256_t nonce = 0;
std::vector<unsigned char> emptyCode;

bool InvokeFunction(string icfDataStr, string icfOutStr, int blockNum,
                    string funcName)
{
    std::vector<unsigned char> icfData(icfDataStr.begin(), icfDataStr.end());
    Transaction icfTx(1, nonce, icfAddress, sender, 0, 11, 99, emptyCode,
                      icfData);

    AccountStore::GetInstance().UpdateAccounts(blockNum, icfTx);

    string outStr = icfOutStr;

    outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
    outStr.erase(std::remove(outStr.begin(), outStr.end(), '\n'), outStr.end());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    LOG_GENERAL(INFO, funcName << ":" << endl << output_file << endl);

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    // BOOST_CHECK_MESSAGE(outStr == output_file,
    //                     "Error: didn't get desired output for" << funcName);
    return true;
}

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContractInvoking)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    AccountStore::GetInstance().Init();

    sender = Schnorr::GetInstance().GenKeyPair();

    std::vector<unsigned char> vec;
    sender.second.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    const std::vector<unsigned char>& output = sha2.Finalize();

    copy(output.end() - ACC_ADDR_SIZE, output.end(),
         fromAddr.asArray().begin());

    cfAddress = Account::GetAddressForContract(fromAddr, nonce);
    LOG_GENERAL(INFO, "CrowdFunding Address: " << cfAddress);

    // Create CrowdFunding Contract
    std::vector<unsigned char> cfCode(cfCodeStr.begin(), cfCodeStr.end());
    cfInitStr
        = regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + cfAddress.hex());
    std::vector<unsigned char> cfInitData(cfInitStr.begin(), cfInitStr.end());

    Transaction cfCreateTx(1, nonce, dev::h160(), sender, 0, 11, 66, cfCode,
                           cfInitData);

    AccountStore::GetInstance().UpdateAccounts(100, cfCreateTx);

    bool checkAddr = false;
    Account* account = AccountStore::GetInstance().GetAccount(cfAddress);
    if (account != nullptr)
    {
        checkAddr = true;
        nonce++;
    }
    BOOST_CHECK_MESSAGE(checkAddr,
                        "Error with creation of crowdfunding contract account");

    // Create Invoker Contract
    std::vector<unsigned char> icfCode(icfCodeStr.begin(), icfCodeStr.end());
    icfInitStr
        = regex_replace(icfInitStr, regex("\\$ADDR"), "0x" + cfAddress.hex());
    std::vector<unsigned char> icfInitData(icfInitStr.begin(),
                                           icfInitStr.end());

    Transaction icfCreateTx(1, nonce, dev::h160(), sender, 0, 11, 66, icfCode,
                            icfInitData);

    AccountStore::GetInstance().UpdateAccounts(100, icfCreateTx);

    icfAddress = Account::GetAddressForContract(fromAddr, nonce);
    LOG_GENERAL(INFO, "Invoker Address: " << icfAddress);

    checkAddr = false;
    account = AccountStore::GetInstance().GetAccount(icfAddress);
    if (account != nullptr)
    {
        checkAddr = true;
        nonce++;
    }
    BOOST_CHECK_MESSAGE(checkAddr,
                        "Error with creation of invoker contract account");

    bool checkInvoke = false;
    // State 1
    //  State 1 Invoke 1
    if (InvokeFunction(icfDataStr1, icfOutStr1, 100, "State 1 Invoke 1"))
    {
        checkInvoke = true;
        nonce++;
    }

    (void)checkInvoke;
    //  State 1 Invoke 2

    //  State 2 Invoke 3

    // Call Crowdfunding to State 2

    // State 2
    //  State 2 Invoke 1

    //  State 2 Invoke 2

    //  State 2 Invoke 3

    // Call Crowdfunding to State 3

    // State 3
    //  State 3 Invoke 1

    //  State 3 Invoke 2

    //  State 3 Invoke 3

    // Call Crowdfunding to State 4

    // State 4
    //  State 4 Invoke 1

    //  State 4 Invoke 2

    //  State 4 Invoke 3

    // Call Crowdfunding to State 5

    // State 5
    //  State 5 Invoke 1

    //  State 5 Invoke 2

    //  State 5 Invoke 3

    // std::vector<unsigned char> data2(dataStr.begin(), dataStr.end());

    // std::vector<unsigned char> vec2;
    // Transaction tx2(1, nonce, toAddress, sender, 0, 11, 66, vec2, data2);
    // AccountStore::GetInstance().UpdateAccounts(1, tx2);

    // outStr.erase(std::remove(outStr.begin(), outStr.end(), ' '), outStr.end());
    // outStr.erase(std::remove(outStr.begin(), outStr.end(), '\n'), outStr.end());

    // ifstream infile{OUTPUT_JSON};
    // std::string output_file{istreambuf_iterator<char>(infile),
    //                         istreambuf_iterator<char>()};

    // output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
    //                   output_file.end());
    // output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
    //                   output_file.end());

    // BOOST_CHECK_MESSAGE(outStr == output_file,
    //                     "Error: didn't get desired output");

    // std::vector<unsigned char> data3(dataStr3.begin(), dataStr3.end());

    // std::vector<unsigned char> vec3;
    // Transaction tx3(1, nonce, toAddress, sender, 0, 11, 66, vec3, data3);
    // AccountStore::GetInstance().UpdateAccounts(1, tx3);

    // std::vector<unsigned char> data4(dataStr4.begin(), dataStr4.end());

    // std::vector<unsigned char> vec4;
    // Transaction tx4(1, nonce, toAddress, sender, 0, 11, 66, vec4, data4);
    // AccountStore::GetInstance().UpdateAccounts(1, tx4);
}

BOOST_AUTO_TEST_SUITE_END()
