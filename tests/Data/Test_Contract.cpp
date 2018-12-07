/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <array>
#include <regex>
#include <string>
#include <vector>
#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#include "CrowdFundingCodes.h"

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

Address toAddress;
PrivKey priv1(
    DataConversion::HexStrToUint8Vec(
        "1658F915F3F9AE35E6B471B7670F53AD1A5BE15D7331EC7FD5E503F21D3450C8"),
    0),
    priv2(
        DataConversion::HexStrToUint8Vec(
            "0FC87BC5ACF5D1243DE7301972B9649EE31688F291F781396B0F67AD98A88147"),
        0);
KeyPair sender(priv1, {priv1}), sender2(priv2, {priv2});
Address fromAddr, fromAddr2;
uint64_t nonce = 0;

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContract) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (SCILLA_ROOT.empty()) {
    return;
  }

  AccountStore::GetInstance().Init();

  // PrivKey senderPrivKey(
  //     DataConversion::HexStrToUint8Vec(
  //         "1658F915F3F9AE35E6B471B7670F53AD1A5BE15D7331EC7FD5E503F21D3450C8"),
  //     0);
  // PubKey senderPubKey(senderPrivKey);
  // sender.first = senderPrivKey;
  // sender.second = senderPubKey;

  fromAddr = Account::GetAddressFromPublicKey(sender.second);

  // PrivKey senderPrivKey2(
  //     DataConversion::HexStrToUint8Vec(
  //         "0FC87BC5ACF5D1243DE7301972B9649EE31688F291F781396B0F67AD98A88147"),
  //     0);
  // PubKey senderPubKey2(senderPrivKey2);
  // sender2.first = senderPrivKey2;
  // sender2.second = senderPubKey2;

  fromAddr2 = Account::GetAddressFromPublicKey(sender2.second);

  AccountStore::GetInstance().AddAccount(fromAddr, {2000000, nonce});

  toAddress = Account::GetAddressForContract(fromAddr, nonce);
  LOG_GENERAL(INFO, "CrowdFunding Address: " << toAddress);

  {
    std::vector<unsigned char> code(cfCodeStr.begin(), cfCodeStr.end());

    string initStr =
        regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + toAddress.hex());
    std::vector<unsigned char> data(initStr.begin(), initStr.end());

    Transaction tx0(1, nonce, NullAddress, sender, 0, PRECISION_MIN_VALUE, 5000,
                    code, data);

    TransactionReceipt tr0;
    AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx0, tr0);

    bool checkToAddr = true;
    Account* account = AccountStore::GetInstance().GetAccount(toAddress);
    if (account == nullptr) {
      checkToAddr = false;
    } else {
      nonce++;
    }
    BOOST_CHECK_MESSAGE(checkToAddr, "Error with creation of contract account");

    cfOutStr0.erase(std::remove(cfOutStr0.begin(), cfOutStr0.end(), ' '),
                    cfOutStr0.end());
    cfOutStr0.erase(std::remove(cfOutStr0.begin(), cfOutStr0.end(), '\n'),
                    cfOutStr0.end());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr0 == output_file,
                        "Error: didn't get desired output");

    LOG_GENERAL(INFO, "[Create] Sender1 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr));
  }

  std::vector<unsigned char> dataDonate(cfDataDonateStr.begin(),
                                        cfDataDonateStr.end());

  {
    Transaction tx1(1, nonce, toAddress, sender, 100, PRECISION_MIN_VALUE, 5000,
                    {}, dataDonate);
    TransactionReceipt tr1;
    if (AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx1, tr1)) {
      nonce++;
    }

    cfOutStr1.erase(std::remove(cfOutStr1.begin(), cfOutStr1.end(), ' '),
                    cfOutStr1.end());
    cfOutStr1.erase(std::remove(cfOutStr1.begin(), cfOutStr1.end(), '\n'),
                    cfOutStr1.end());

    cfOutStr1 =
        regex_replace(cfOutStr1, regex("\\$ADDR"), "0x" + fromAddr.hex());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr1 == output_file,
                        "Error: didn't get desired output");

    LOG_GENERAL(INFO, "[Call1] Sender1 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr));
    LOG_GENERAL(INFO, "[Call1] Contract balance: "
                          << AccountStore::GetInstance().GetBalance(toAddress));
  }

  {
    Transaction tx2(1, nonce, toAddress, sender2, 200, PRECISION_MIN_VALUE,
                    5000, {}, dataDonate);
    TransactionReceipt tr2;
    if (AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx2, tr2)) {
      nonce++;
    }

    cfOutStr2.erase(std::remove(cfOutStr2.begin(), cfOutStr2.end(), ' '),
                    cfOutStr2.end());
    cfOutStr2.erase(std::remove(cfOutStr2.begin(), cfOutStr2.end(), '\n'),
                    cfOutStr2.end());

    cfOutStr2 =
        regex_replace(cfOutStr2, regex("\\$ADDR1"), "0x" + fromAddr.hex());
    cfOutStr2 =
        regex_replace(cfOutStr2, regex("\\$ADDR2"), "0x" + fromAddr2.hex());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr2 == output_file,
                        "Error: didn't get desired output");

    LOG_GENERAL(INFO, cfOutStr2);
    LOG_GENERAL(INFO, output_file);

    LOG_GENERAL(INFO, "[Call2] Sender1 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr));
    LOG_GENERAL(INFO, "[Call2] Sender2 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr2));
    LOG_GENERAL(INFO, "[Call2] Contract balance: "
                          << AccountStore::GetInstance().GetBalance(toAddress));
  }

  std::vector<unsigned char> dataGetFunds(cfDataGetFundsStr.begin(),
                                          cfDataGetFundsStr.end());

  {
    Transaction tx3(1, nonce, toAddress, sender2, 0, PRECISION_MIN_VALUE, 5000,
                    {}, dataGetFunds);
    TransactionReceipt tr3;
    if (AccountStore::GetInstance().UpdateAccounts(200, 1, true, tx3, tr3)) {
      nonce++;
    }

    cfOutStr3.erase(std::remove(cfOutStr3.begin(), cfOutStr3.end(), ' '),
                    cfOutStr3.end());
    cfOutStr3.erase(std::remove(cfOutStr3.begin(), cfOutStr3.end(), '\n'),
                    cfOutStr3.end());

    cfOutStr3 =
        regex_replace(cfOutStr3, regex("\\$ADDR1"), "0x" + fromAddr.hex());
    cfOutStr3 =
        regex_replace(cfOutStr3, regex("\\$ADDR2"), "0x" + fromAddr2.hex());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr3 == output_file,
                        "Error: didn't get desired output");

    LOG_GENERAL(INFO, cfOutStr3);
    LOG_GENERAL(INFO, output_file);

    LOG_GENERAL(INFO, "[Call3] Sender1 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr));
    LOG_GENERAL(INFO, "[Call3] Sender2 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr2));
    LOG_GENERAL(INFO, "[Call3] Contract balance: "
                          << AccountStore::GetInstance().GetBalance(toAddress));
  }

  std::vector<unsigned char> dataClaimBack(cfDataClaimBackStr.begin(),
                                           cfDataClaimBackStr.end());

  {
    Transaction tx4(1, nonce, toAddress, sender, 0, PRECISION_MIN_VALUE, 5000,
                    {}, dataClaimBack);
    TransactionReceipt tr4;
    if (AccountStore::GetInstance().UpdateAccounts(300, 1, true, tx4, tr4)) {
      nonce++;
    }

    cfOutStr4.erase(std::remove(cfOutStr4.begin(), cfOutStr4.end(), ' '),
                    cfOutStr4.end());
    cfOutStr4.erase(std::remove(cfOutStr4.begin(), cfOutStr4.end(), '\n'),
                    cfOutStr4.end());

    cfOutStr4 =
        regex_replace(cfOutStr4, regex("\\$ADDR1"), "0x" + fromAddr.hex());
    cfOutStr4 =
        regex_replace(cfOutStr4, regex("\\$ADDR2"), "0x" + fromAddr2.hex());

    ifstream infile{OUTPUT_JSON};
    std::string output_file{istreambuf_iterator<char>(infile),
                            istreambuf_iterator<char>()};

    output_file.erase(std::remove(output_file.begin(), output_file.end(), ' '),
                      output_file.end());
    output_file.erase(std::remove(output_file.begin(), output_file.end(), '\n'),
                      output_file.end());

    BOOST_CHECK_MESSAGE(cfOutStr4 == output_file,
                        "Error: didn't get desired output");

    LOG_GENERAL(INFO, "[Call4] Sender1 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr));
    LOG_GENERAL(INFO, "[Call4] Sender2 balance: "
                          << AccountStore::GetInstance().GetBalance(fromAddr2));
    LOG_GENERAL(INFO, "[Call4] Contract balance: "
                          << AccountStore::GetInstance().GetBalance(toAddress));
  }
}

BOOST_AUTO_TEST_SUITE_END()
