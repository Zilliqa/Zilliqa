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

Address fromAddr, fromAddr2;
Address toAddress;
KeyPair sender, sender2;
uint128_t nonce = 0;

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testContract) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  sender = Schnorr::GetInstance().GenKeyPair();
  sender2 = Schnorr::GetInstance().GenKeyPair();

  fromAddr = Account::GetAddressFromPublicKey(sender.second);
  fromAddr2 = Account::GetAddressFromPublicKey(sender2.second);

  std::vector<unsigned char> code(cfCodeStr.begin(), cfCodeStr.end());

  toAddress = Account::GetAddressForContract(fromAddr, nonce);
  LOG_GENERAL(INFO, "CrowdFunding Address: " << toAddress);
  string initStr =
      regex_replace(cfInitStr, regex("\\$ADDR"), "0x" + toAddress.hex());
  std::vector<unsigned char> data(initStr.begin(), initStr.end());

  Transaction tx1(1, nonce, NullAddress, sender, 0, PRECISION_MIN_VALUE, 50,
                  code, data);

  TransactionReceipt tr1;
  AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx1, tr1);

  bool checkToAddr = true;
  Account* account = AccountStore::GetInstance().GetAccount(toAddress);
  if (account == nullptr) {
    checkToAddr = false;
  } else {
    nonce++;
  }
  BOOST_CHECK_MESSAGE(checkToAddr, "Error with creation of contract account");

  LOG_GENERAL(INFO, "[Create] Sender1 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr));

  std::vector<unsigned char> dataDonate(cfDataDonateStr.begin(),
                                        cfDataDonateStr.end());

  Transaction tx2(1, nonce, toAddress, sender, 100, PRECISION_MIN_VALUE, 10, {},
                  dataDonate);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx2, tr2)) {
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

  // BOOST_CHECK_MESSAGE(cfOutStr == output_file,
  //                     "Error: didn't get desired output");

  LOG_GENERAL(INFO, "[Call1] Sender1 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr));
  LOG_GENERAL(INFO, "[Call1] Contract balance: "
                        << AccountStore::GetInstance().GetBalance(toAddress));

  Transaction tx3(1, nonce, toAddress, sender2, 200, PRECISION_MIN_VALUE, 10,
                  {}, dataDonate);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccounts(100, 1, true, tx3, tr3)) {
    nonce++;
  }

  LOG_GENERAL(INFO, "[Call3] Sender1 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr));
  LOG_GENERAL(INFO, "[Call3] Sender2 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr2));
  LOG_GENERAL(INFO, "[Call3] Contract balance: "
                        << AccountStore::GetInstance().GetBalance(toAddress));

  std::vector<unsigned char> dataGetFunds(cfDataGetFundsStr.begin(),
                                          cfDataGetFundsStr.end());

  Transaction tx4(1, nonce, toAddress, sender2, 0, PRECISION_MIN_VALUE, 10, {},
                  dataGetFunds);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccounts(200, 1, true, tx4, tr4)) {
    nonce++;
  }

  LOG_GENERAL(INFO, "[Call4] Sender1 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr));
  LOG_GENERAL(INFO, "[Call4] Sender2 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr2));
  LOG_GENERAL(INFO, "[Call4] Contract balance: "
                        << AccountStore::GetInstance().GetBalance(toAddress));

  std::vector<unsigned char> dataClaimBack(cfDataClaimBackStr.begin(),
                                           cfDataClaimBackStr.end());

  Transaction tx5(1, nonce, toAddress, sender, 0, PRECISION_MIN_VALUE, 10, {},
                  dataClaimBack);
  TransactionReceipt tr5;
  if (AccountStore::GetInstance().UpdateAccounts(300, 1, true, tx5, tr5)) {
    nonce++;
  }

  LOG_GENERAL(INFO, "[Call5] Sender1 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr));
  LOG_GENERAL(INFO, "[Call5] Sender2 balance: "
                        << AccountStore::GetInstance().GetBalance(fromAddr2));
  LOG_GENERAL(INFO, "[Call5] Contract balance: "
                        << AccountStore::GetInstance().GetBalance(toAddress));
}

BOOST_AUTO_TEST_SUITE_END()
