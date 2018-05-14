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
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include <array>
#include <string>
#include <vector>

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

// Init Account Store
BOOST_AUTO_TEST_CASE(initAccountStore)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    AccountStore::GetInstance().Init();
}

Address fromAddr;
Address toAddress;
KeyPair sender;
uint256_t nonce = 0;

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(createContract)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    sender = Schnorr::GetInstance().GenKeyPair();

    std::vector<unsigned char> vec;
    sender.second.Serialize(vec, 0);
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    sha2.Update(vec);
    const std::vector<unsigned char>& output = sha2.Finalize();

    copy(output.end() - ACC_ADDR_SIZE, output.end(),
         fromAddr.asArray().begin());

    string codeStr = "Demo Code";
    std::vector<unsigned char> code(codeStr.begin(), codeStr.end());

    string dataStr
        = "[{\"vname\" : \"owner\",\"type\" : \"Address\", \"value\" : "
          "\"0x1234567890123456789012345678901234567890\"},{\"vname\" "
          ": \"max_block\",\"type\" : \"BNum\" ,\"value\" : \"199\"},{ "
          "\"vname\" : \"goal\",\"type\" : \"Int\",\"value\" : "
          "\"500\"}]";
    std::vector<unsigned char> data(dataStr.begin(), dataStr.end());

    toAddress = NullAddress;

    Transaction tx1(1, nonce, toAddress, sender, 200, 11, 22, code, data);

    AccountStore::GetInstance().UpdateAccounts(1, tx1);

    toAddress = Account::GetAddressForContract(fromAddr, nonce);

    bool checkToAddr = true;
    Account* account = AccountStore::GetInstance().GetAccount(toAddress);
    if (account == nullptr)
    {
        checkToAddr = false;
    }
    BOOST_CHECK_MESSAGE(checkToAddr, "Error with creation of contract account");
}

// Create Transaction to call contract
BOOST_AUTO_TEST_CASE(callContract)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    string dataStr
        = "[{\"vname\" : \"backers\",\"vtype\" : \"Map\", \"mutable\": "
          "\"True\", \"value\" :[{\"keyType\" : \"address\",\"valType\" : "
          "\"Int\"},{\"key\" : \"a0x32a3aa32456f\",\"val\" : \"100\"},{\"key\" "
          ": \"a0x32a2ba32454a\",\"val\" : \"10\"}{\"vname\" : "
          "\"funded\"\"type\" : \"Bool\"\"value\" : \"True\"}{\"vname\" : "
          "\"_balance\"\"type\" : \"Int\"\"value\" : \"200\"}]";

    std::vector<unsigned char> data(dataStr.begin(), dataStr.end());

    std::vector<unsigned char> vec;
    Transaction tx2(1, nonce, toAddress, sender, 100, 11, 22, vec, data);
    // AccountStore::GetInstance().UpdateAccounts(1, tx2);
}

BOOST_AUTO_TEST_SUITE_END()