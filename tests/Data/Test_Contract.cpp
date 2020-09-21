/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <array>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include <openssl/rand.h>
#include <boost/filesystem.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/TxnStatus.h"
#include "depends/common/CommonIO.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

#include "ScillaTestUtil.h"

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

PrivKey priv1, priv2, priv3, priv4;

void setup() {
  bytes priv1bytes, priv2bytes, priv3bytes;
  DataConversion::HexStrToUint8Vec(
      "1658F915F3F9AE35E6B471B7670F53AD1A5BE15D7331EC7FD5E503F21D3450C8",
      priv1bytes);
  DataConversion::HexStrToUint8Vec(
      "0FC87BC5ACF5D1243DE7301972B9649EE31688F291F781396B0F67AD98A88147",
      priv2bytes);
  DataConversion::HexStrToUint8Vec(
      "0AB52CF5D3F9A1E730243DB96419729EE31688F29B0F67AD98A881471F781396",
      priv3bytes);
  priv1.Deserialize(priv1bytes, 0);
  priv2.Deserialize(priv2bytes, 0);
  priv3.Deserialize(priv3bytes, 0);
}

BOOST_AUTO_TEST_SUITE(contracttest)

BOOST_AUTO_TEST_CASE(loopytreecall) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  LOG_GENERAL(INFO, "loopy-tree-call started")

  PairOfKey owner = Schnorr::GenKeyPair();
  Address ownerAddr, contrAddr0, contrAddr1, contrAddr2, contrAddr3, contrAddr4;
  uint64_t nonce;

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  LOG_GENERAL(INFO, "Owner Address: " << ownerAddr);
  AccountStore::GetInstance().AddAccountTemp(ownerAddr,
                                             {200000000000000000, nonce});

  contrAddr0 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "contrAddr0: " << contrAddr0);
  contrAddr1 = Account::GetAddressForContract(ownerAddr, nonce + 1);
  LOG_GENERAL(INFO, "contrAddr1: " << contrAddr1);
  contrAddr2 = Account::GetAddressForContract(ownerAddr, nonce + 2);
  LOG_GENERAL(INFO, "contrAddr2: " << contrAddr2);
  contrAddr3 = Account::GetAddressForContract(ownerAddr, nonce + 3);
  LOG_GENERAL(INFO, "contrAddr3: " << contrAddr3);
  contrAddr4 = Account::GetAddressForContract(ownerAddr, nonce + 4);
  LOG_GENERAL(INFO, "contrAddr4: " << contrAddr4);

  ScillaTestUtil::ScillaTest test;
  BOOST_CHECK_MESSAGE(ScillaTestUtil::GetScillaTest(test, "loopy-tree-call", 1),
                      "Unable to fetch test loopy-tree-call_" << 1 << ".");

  test.message["_sender"] = "0x" + ownerAddr.hex();

  Json::Value other_instances;
  other_instances.append("0x" + contrAddr1.hex());
  other_instances.append("0x" + contrAddr2.hex());
  other_instances.append("0x" + contrAddr3.hex());
  other_instances.append("0x" + contrAddr4.hex());
  test.message["params"][1]["value"] = other_instances;

  LOG_GENERAL(INFO, "message: " << JSONUtils::GetInstance().convertJsontoStr(
                        test.message));

  // Replace owner address in init.json
  for (auto& it : test.init) {
    if (it["vname"] == "owner") {
      it["value"] = "0x" + ownerAddr.hex();
    }
  }

  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(test.init);
  ScillaTestUtil::RemoveThisAddressFromInit(test.init);

  // deploy contracts
  std::string initStr = JSONUtils::GetInstance().convertJsontoStr(test.init);
  bytes data = bytes(initStr.begin(), initStr.end());

  for (unsigned int i = 0; i < 5; i++) {
    Transaction tx(DataConversion::Pack(CHAIN_ID, 1), nonce, Address(), owner,
                   0, PRECISION_MIN_VALUE, 20000, test.code, data);
    TransactionReceipt tr;
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(
        ScillaTestUtil::GetBlockNumberFromJson(test.blockchain), 1, true, tx,
        tr, error_code);
    nonce++;
  }

  // call contract 0
  {
    bytes data;
    uint64_t amount = ScillaTestUtil::PrepareMessageData(test.message, data);

    Transaction tx(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr0, owner,
                   amount, PRECISION_MIN_VALUE, 2000000, {}, data);
    TransactionReceipt tr;
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(
        ScillaTestUtil::GetBlockNumberFromJson(test.blockchain), 1, true, tx,
        tr, error_code);

    LOG_GENERAL(INFO, "tr: " << tr.GetString());

    nonce++;
  }

  LOG_GENERAL(INFO, "loopy-tree-call ended");
}

BOOST_AUTO_TEST_CASE(salarybot) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey owner = Schnorr::GenKeyPair();
  PairOfKey employee1 = Schnorr::GenKeyPair();
  PairOfKey employee2 = Schnorr::GenKeyPair();
  PairOfKey employee3 = Schnorr::GenKeyPair();

  Address ownerAddr, employee1Addr, employee2Addr, employee3Addr, contrAddr;
  uint64_t nonce = 0;

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  employee1Addr = Account::GetAddressFromPublicKey(employee1.second);
  employee2Addr = Account::GetAddressFromPublicKey(employee2.second);
  employee3Addr = Account::GetAddressFromPublicKey(employee3.second);

  AccountStore::GetInstance().AddAccountTemp(ownerAddr, {2000000000000, nonce});

  contrAddr = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "Salarybot Address: " << contrAddr);

  std::vector<ScillaTestUtil::ScillaTest> tests;

  for (unsigned int i = 0; i <= 5; i++) {
    ScillaTestUtil::ScillaTest test;
    BOOST_CHECK_MESSAGE(ScillaTestUtil::GetScillaTest(test, "salarybot", i),
                        "Unable to fetch test salarybot_" << i << ".");

    test.message["_sender"] = "0x" + ownerAddr.hex();

    tests.emplace_back(test);
  }

  tests[1].message["params"][0]["value"] = "0x" + employee1Addr.hex();
  tests[2].message["params"][0]["value"] = "0x" + employee2Addr.hex();
  tests[3].message["params"][0]["value"] = "0x" + employee3Addr.hex();
  tests[4].message["params"][0]["value"] = "0x" + employee1Addr.hex();

  for (const auto& test : tests) {
    LOG_GENERAL(INFO, "message: " << JSONUtils::GetInstance().convertJsontoStr(
                          test.message));
  }

  // Replace owner address in init.json
  for (auto& it : tests[0].init) {
    if (it["vname"] == "owner") {
      it["value"] = "0x" + ownerAddr.hex();
    }
  }

  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(tests[0].init);
  ScillaTestUtil::RemoveThisAddressFromInit(tests[0].init);

  bool deployed = false;

  for (unsigned int i = 0; i < tests.size();) {
    bool deploy = i == 0 && !deployed;

    uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(tests[i].blockchain);
    std::string initStr =
        JSONUtils::GetInstance().convertJsontoStr(tests[i].init);
    bytes data;
    uint64_t amount = 0;
    Address recipient;
    bytes code;
    if (deploy) {
      data = bytes(initStr.begin(), initStr.end());
      recipient = Address();
      code = tests[i].code;
      deployed = true;
    } else {
      amount = ScillaTestUtil::PrepareMessageData(tests[i].message, data);
      recipient = contrAddr;
      i++;
    }

    Transaction tx(DataConversion::Pack(CHAIN_ID, 1), nonce, recipient, owner,
                   amount, PRECISION_MIN_VALUE, 20000, code, data);
    TransactionReceipt tr;
    TxnStatus ets;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx, tr, ets);
    nonce++;
  }

  Account* e2 = AccountStore::GetInstance().GetAccountTemp(employee2Addr);
  Account* e3 = AccountStore::GetInstance().GetAccountTemp(employee3Addr);

  BOOST_CHECK_MESSAGE(e2 != nullptr && e3 != nullptr,
                      "employee2 or 3 are not existing");

  BOOST_CHECK_MESSAGE(e2->GetBalance() == 11000 && e3->GetBalance() == 12000,
                      "multi message failed");
}

// Scilla Library
BOOST_AUTO_TEST_CASE(testScillaLibrary) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey owner = Schnorr::GenKeyPair();
  Address ownerAddr, libAddr1, libAddr2, contrAddr1;
  uint64_t nonce = 0;

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);

  AccountStore::GetInstance().AddAccountTemp(ownerAddr,
                                             {2000000000000000, nonce});

  /* ------------------------------------------------------------------- */
  // Deploying the library 1
  libAddr1 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "Library 1 address: " << libAddr1);

  ScillaTestUtil::ScillaTest t1;
  string t1_name = "0x986556789012345678901234567890123456abcd";
  if (!ScillaTestUtil::GetScillaTest(t1, t1_name, 1, "0", true)) {
    LOG_GENERAL(
        WARNING,
        "Unable to fetch test 0x986556789012345678901234567890123456abcd.");
    return;
  }

  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(t1.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t1.init);

  uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t1.blockchain);

  // Transaction to deploy library contract
  std::string initStr1 = JSONUtils::GetInstance().convertJsontoStr(t1.init);
  bytes data1(initStr1.begin(), initStr1.end());
  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t1.code, data1);
  TransactionReceipt tr1;
  TxnStatus error_code;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                 error_code);
  Account* account1 = AccountStore::GetInstance().GetAccountTemp(libAddr1);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account1 != nullptr,
                      "Error with creation of contract account");
  nonce++;

  /* ------------------------------------------------------------------- */
  // Deploying the library 2
  libAddr2 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "Library 2 address: " << libAddr2);

  ScillaTestUtil::ScillaTest t2;
  std::string t2_name = "0x111256789012345678901234567890123456abef";
  if (!ScillaTestUtil::GetScillaTest(t2, t2_name, 1, "0", true)) {
    LOG_GENERAL(
        WARNING,
        "Unable to fetch test 0x111256789012345678901234567890123456abef.");
    return;
  }

  // Modify _extlibs
  for (auto& it : t2.init) {
    if (it["vname"] == "_extlibs") {
      for (auto& v : it["value"]) {
        for (auto& a : v["arguments"]) {
          if (a.asString() == t1_name) {
            a = "0x" + libAddr1.hex();
          }
        }
      }
    }
  }

  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(t2.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t2.init);

  uint64_t bnum2 = ScillaTestUtil::GetBlockNumberFromJson(t2.blockchain);

  // Transaction to deploy library contract
  std::string initStr2 = JSONUtils::GetInstance().convertJsontoStr(t2.init);
  LOG_GENERAL(INFO, "initStr2: " << initStr2);
  bytes data2(initStr2.begin(), initStr2.end());
  Transaction tx2(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t2.code, data2);
  TransactionReceipt tr2;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum2, 1, true, tx2, tr2,
                                                 error_code);
  Account* account2 = AccountStore::GetInstance().GetAccountTemp(libAddr2);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account2 != nullptr,
                      "Error with creation of contract account");
  nonce++;

  // Check whether cache of library 1 exists
  BOOST_CHECK_MESSAGE(
      boost::filesystem::exists(EXTLIB_FOLDER + '/' + "0x" + libAddr1.hex() +
                                LIBRARY_CODE_EXTENSION),
      "libAddr1 cache not exists for libAddr2 deployment");
  AccountStore::GetInstance().CleanNewLibrariesCacheTemp();
  // Check whether cache of library 1 exists
  BOOST_CHECK_MESSAGE(
      boost::filesystem::exists(EXTLIB_FOLDER + '/' + "0x" + libAddr1.hex() +
                                LIBRARY_CODE_EXTENSION),
      "libAddr1 cache still exists for libAddr2 deployment after cache clean");

  /* ------------------------------------------------------------------- */
  // deploying contract
  contrAddr1 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "Contract address: " << contrAddr1);

  ScillaTestUtil::ScillaTest t3;
  if (!ScillaTestUtil::GetScillaTest(t3, "import-test-lib", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test import-test-lib");
    return;
  }

  // Modify _extlibs
  for (auto& it : t3.init) {
    if (it["vname"] == "_extlibs") {
      for (auto& v : it["value"]) {
        for (auto& a : v["arguments"]) {
          if (a.asString() == t2_name) {
            a = "0x" + libAddr2.hex();
          }
        }
      }
    }
  }

  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(t3.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t3.init);

  uint64_t bnum3 = ScillaTestUtil::GetBlockNumberFromJson(t3.blockchain);

  // Transaction to deploy contract
  std::string initStr3 = JSONUtils::GetInstance().convertJsontoStr(t3.init);
  bytes data3(initStr3.begin(), initStr3.end());
  Transaction tx3(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t3.code, data3);
  TransactionReceipt tr3;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum3, 1, true, tx3, tr3,
                                                 error_code);
  Account* account3 = AccountStore::GetInstance().GetAccountTemp(contrAddr1);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account3 != nullptr,
                      "Error with creation of contract account");
  nonce++;

  // Check whether cache of library 1/2 exists
  BOOST_CHECK_MESSAGE(
      boost::filesystem::exists(EXTLIB_FOLDER + '/' + "0x" + libAddr1.hex() +
                                LIBRARY_CODE_EXTENSION) &&
          boost::filesystem::exists(EXTLIB_FOLDER + '/' + "0x" +
                                    libAddr2.hex() + LIBRARY_CODE_EXTENSION),
      "libAddr1/2 cache not exists for contAddr1 deployment");

  /* ------------------------------------------------------------------- */
  // Execute message_1.
  bytes dataHi;
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t3.message, dataHi);

  Transaction tx4(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr1, owner,
                  amount, PRECISION_MIN_VALUE, 50000, {}, dataHi);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx4, tr4,
                                                     error_code)) {
    nonce++;
  }

  // check receipt for events
  LOG_GENERAL(INFO, "receipt after processing: " << tr4.GetString());
}

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testCrowdfunding) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey owner(priv1, {priv1}), donor1(priv2, {priv2}),
      donor2(priv3, {priv3});
  Address ownerAddr, donor1Addr, donor2Addr, contrAddr;
  uint64_t nonce = 0;

  setup();

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  donor1Addr = Account::GetAddressFromPublicKey(donor1.second);
  donor2Addr = Account::GetAddressFromPublicKey(donor2.second);

  AccountStore::GetInstance().AddAccountTemp(ownerAddr,
                                             {2000000000000000, nonce});
  AccountStore::GetInstance().AddAccountTemp(donor1Addr,
                                             {2000000000000000, nonce});
  AccountStore::GetInstance().AddAccountTemp(donor2Addr,
                                             {2000000000000000, nonce});

  contrAddr = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "CrowdFunding Address: " << contrAddr);

  // Deploying the contract can use data from the 1st Scilla test.
  ScillaTestUtil::ScillaTest t1;
  if (!ScillaTestUtil::GetScillaTest(t1, "crowdfunding", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_1.");
    return;
  }

  // Replace owner address in init.json.
  for (auto& it : t1.init) {
    if (it["vname"] == "owner") {
      it["value"] = "0x" + ownerAddr.hex();
    }
  }
  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(t1.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t1.init);

  uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t1.blockchain);

  // Transaction to deploy contract.
  std::string initStr = JSONUtils::GetInstance().convertJsontoStr(t1.init);
  bytes data(initStr.begin(), initStr.end());
  Transaction tx0(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t1.code, data);
  TransactionReceipt tr0;
  TxnStatus error_code;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx0, tr0,
                                                 error_code);
  Account* account = AccountStore::GetInstance().GetAccountTemp(contrAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account != nullptr,
                      "Error with creation of contract account");
  nonce++;

  /* ------------------------------------------------------------------- */

  // Execute message_1, the Donate transaction.
  bytes dataDonate;
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t1.message, dataDonate);

  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor1,
                  amount, PRECISION_MIN_VALUE, 50000, {}, dataDonate);
  TransactionReceipt tr1;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                     error_code)) {
    nonce++;
  }

  uint128_t contrBal =
      AccountStore::GetInstance().GetAccountTemp(contrAddr)->GetBalance();

  LOG_GENERAL(INFO, "[Call1] Owner balance: " << AccountStore::GetInstance()
                                                     .GetAccountTemp(ownerAddr)
                                                     ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call1] Donor1 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor1Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call1] Donor2 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor2Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO, "[Call1] Contract balance (scilla): " << contrBal);
  BOOST_CHECK_MESSAGE(contrBal == amount, "Balance mis-match after Donate");

  /* ------------------------------------------------------------------- */

  // Do another donation from donor2
  ScillaTestUtil::ScillaTest t2;
  if (!ScillaTestUtil::GetScillaTest(t2, "crowdfunding", 2)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_2.");
    return;
  }

  uint64_t bnum2 = ScillaTestUtil::GetBlockNumberFromJson(t2.blockchain);
  // Execute message_2, the Donate transaction.
  bytes dataDonate2;
  uint64_t amount2 =
      ScillaTestUtil::PrepareMessageData(t2.message, dataDonate2);

  Transaction tx2(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor2,
                  amount2, PRECISION_MIN_VALUE, 50000, {}, dataDonate2);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum2, 1, true, tx2, tr2,
                                                     error_code)) {
    nonce++;
  }

  uint128_t contrBal2 =
      AccountStore::GetInstance().GetAccountTemp(contrAddr)->GetBalance();

  LOG_GENERAL(INFO, "[Call2] Owner balance: " << AccountStore::GetInstance()
                                                     .GetAccountTemp(ownerAddr)
                                                     ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call2] Donor1 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor1Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call2] Donor2 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor2Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO, "[Call2] Contract balance (scilla): " << contrBal2);
  BOOST_CHECK_MESSAGE(contrBal2 == amount + amount2,
                      "Balance mis-match after Donate2");

  /* ------------------------------------------------------------------- */

  // Let's try donor1 donating again, it shouldn't have an impact.
  // Execute message_3, the unsuccessful Donate transaction.
  Transaction tx3(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor1,
                  amount, PRECISION_MIN_VALUE, 50000, {}, dataDonate);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx3, tr3,
                                                     error_code)) {
    nonce++;
  }
  uint128_t contrBal3 =
      AccountStore::GetInstance().GetAccountTemp(contrAddr)->GetBalance();

  LOG_GENERAL(INFO, "[Call3] Owner balance: " << AccountStore::GetInstance()
                                                     .GetAccountTemp(ownerAddr)
                                                     ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call3] Donor1 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor1Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call3] Donor2 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor2Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO, "[Call3] Contract balance (scilla): " << contrBal3);
  BOOST_CHECK_MESSAGE(contrBal3 == contrBal2,
                      "Balance mis-match after Donate3");

  /* ------------------------------------------------------------------- */

  // Owner tries to get fund, fails
  ScillaTestUtil::ScillaTest t4;
  if (!ScillaTestUtil::GetScillaTest(t4, "crowdfunding", 4)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_4.");
    return;
  }

  uint64_t bnum4 = ScillaTestUtil::GetBlockNumberFromJson(t4.blockchain);
  // Execute message_4, the Donate transaction.
  bytes data4;
  uint64_t amount4 = ScillaTestUtil::PrepareMessageData(t4.message, data4);

  Transaction tx4(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, owner,
                  amount4, PRECISION_MIN_VALUE, 50000, {}, data4);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum4, 1, true, tx4, tr4,
                                                     error_code)) {
    nonce++;
  }

  uint128_t contrBal4 =
      AccountStore::GetInstance().GetAccountTemp(contrAddr)->GetBalance();

  LOG_GENERAL(INFO, "[Call4] Owner balance: " << AccountStore::GetInstance()
                                                     .GetAccountTemp(ownerAddr)
                                                     ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call4] Donor1 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor1Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call4] Donor2 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor2Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO, "[Call4] Contract balance (scilla): " << contrBal4);
  BOOST_CHECK_MESSAGE(contrBal4 == contrBal3,
                      "Balance mis-match after GetFunds");

  /* ------------------------------------------------------------------- */

  // Donor1 ClaimsBack his funds. Succeeds.
  ScillaTestUtil::ScillaTest t5;
  if (!ScillaTestUtil::GetScillaTest(t5, "crowdfunding", 5)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_5.");
    return;
  }

  uint64_t bnum5 = ScillaTestUtil::GetBlockNumberFromJson(t5.blockchain);
  // Execute message_5, the Donate transaction.
  bytes data5;
  uint64_t amount5 = ScillaTestUtil::PrepareMessageData(t5.message, data5);

  Transaction tx5(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor1,
                  amount5, PRECISION_MIN_VALUE, 50000, {}, data5);
  TransactionReceipt tr5;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum5, 1, true, tx5, tr5,
                                                     error_code)) {
    nonce++;
  }

  uint128_t contrBal5 =
      AccountStore::GetInstance().GetAccountTemp(contrAddr)->GetBalance();

  LOG_GENERAL(INFO, "[Call5] Owner balance: " << AccountStore::GetInstance()
                                                     .GetAccountTemp(ownerAddr)
                                                     ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call5] Donor1 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor1Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO,
              "[Call5] Donor2 balance: " << AccountStore::GetInstance()
                                                .GetAccountTemp(donor2Addr)
                                                ->GetBalance());
  LOG_GENERAL(INFO, "[Call5] Contract balance (scilla): " << contrBal4);
  BOOST_CHECK_MESSAGE(contrBal5 == contrBal4 - amount,
                      "Balance mis-match after GetFunds");

  /* ------------------------------------------------------------------- */
}

BOOST_AUTO_TEST_CASE(testPingPong) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey owner(priv1, {priv1}), ping(priv2, {priv2}), pong(priv3, {priv3});
  Address ownerAddr, pingAddr, pongAddr;
  uint64_t nonce = 0;

  setup();

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  AccountStore::GetInstance().AddAccountTemp(ownerAddr,
                                             {2000000000000000, nonce});

  pingAddr = Account::GetAddressForContract(ownerAddr, nonce);
  pongAddr = Account::GetAddressForContract(ownerAddr, nonce + 1);

  LOG_GENERAL(INFO,
              "Ping Address: " << pingAddr << " ; PongAddress: " << pongAddr);

  /* ------------------------------------------------------------------- */

  // Deploying the contract can use data from the 0th Scilla test.
  ScillaTestUtil::ScillaTest t0ping;
  if (!ScillaTestUtil::GetScillaTest(t0ping, "ping", 0)) {
    LOG_GENERAL(WARNING, "Unable to fetch test ping_0.");
    return;
  }

  uint64_t bnumPing = ScillaTestUtil::GetBlockNumberFromJson(t0ping.blockchain);
  ScillaTestUtil::RemoveCreationBlockFromInit(t0ping.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t0ping.init);

  // Transaction to deploy ping.
  std::string initStrPing =
      JSONUtils::GetInstance().convertJsontoStr(t0ping.init);
  bytes dataPing(initStrPing.begin(), initStrPing.end());
  Transaction tx0(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t0ping.code, dataPing);
  TransactionReceipt tr0;
  TxnStatus error_code;
  AccountStore::GetInstance().UpdateAccountsTemp(bnumPing, 1, true, tx0, tr0,
                                                 error_code);
  Account* accountPing = AccountStore::GetInstance().GetAccountTemp(pingAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountPing != nullptr,
                      "Error with creation of ping account");
  nonce++;

  // Deploying the contract can use data from the 0th Scilla test.
  ScillaTestUtil::ScillaTest t0pong;
  if (!ScillaTestUtil::GetScillaTest(t0pong, "pong", 0)) {
    LOG_GENERAL(WARNING, "Unable to fetch test pong_0.");
    return;
  }

  uint64_t bnumPong = ScillaTestUtil::GetBlockNumberFromJson(t0pong.blockchain);
  ScillaTestUtil::RemoveCreationBlockFromInit(t0pong.init);
  ScillaTestUtil::RemoveThisAddressFromInit(t0pong.init);

  // Transaction to deploy pong.
  std::string initStrPong =
      JSONUtils::GetInstance().convertJsontoStr(t0pong.init);
  bytes dataPong(initStrPong.begin(), initStrPong.end());
  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, t0pong.code, dataPong);
  TransactionReceipt tr1;
  AccountStore::GetInstance().UpdateAccountsTemp(bnumPong, 1, true, tx1, tr1,
                                                 error_code);
  Account* accountPong = AccountStore::GetInstance().GetAccountTemp(pongAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountPong != nullptr,
                      "Error with creation of pong account");
  nonce++;

  LOG_GENERAL(INFO, "Deployed ping and pong contracts.");

  /* ------------------------------------------------------------------- */

  // Set addresses of ping and pong in pong and ping respectively.
  bytes data;
  // Replace pong address in parameter of message.
  for (auto it = t0ping.message["params"].begin();
       it != t0ping.message["params"].end(); it++) {
    if ((*it)["vname"] == "pongAddr") {
      (*it)["value"] = "0x" + pongAddr.hex();
    }
  }
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t0ping.message, data);
  Transaction tx2(DataConversion::Pack(CHAIN_ID, 1), nonce, pingAddr, owner,
                  amount, PRECISION_MIN_VALUE, 50000, {}, data);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnumPing, 1, true, tx2,
                                                     tr2, error_code)) {
    nonce++;
  }

  // Replace ping address in paramter of message.
  for (auto it = t0pong.message["params"].begin();
       it != t0pong.message["params"].end(); it++) {
    if ((*it)["vname"] == "pingAddr") {
      (*it)["value"] = "0x" + pingAddr.hex();
    }
  }
  amount = ScillaTestUtil::PrepareMessageData(t0pong.message, data);
  Transaction tx3(DataConversion::Pack(CHAIN_ID, 1), nonce, pongAddr, owner,
                  amount, PRECISION_MIN_VALUE, 50000, {}, data);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnumPong, 1, true, tx3,
                                                     tr3, error_code)) {
    nonce++;
  }

  LOG_GENERAL(INFO, "Finished setting ping-pong addresses in both contracts.");

  /* ------------------------------------------------------------------- */

  // Let's just ping now and see the ping-pong bounces.
  ScillaTestUtil::ScillaTest t1ping;
  if (!ScillaTestUtil::GetScillaTest(t1ping, "ping", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test ping_1.");
    return;
  }

  ScillaTestUtil::PrepareMessageData(t1ping.message, data);
  Transaction tx4(DataConversion::Pack(CHAIN_ID, 1), nonce, pingAddr, owner,
                  amount, PRECISION_MIN_VALUE, 50000, {}, data);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnumPing, 1, true, tx4,
                                                     tr4, error_code)) {
    nonce++;
  }

  // Fetch the states of both ping and pong and verify "count" is 0.
  Json::Value pingState;
  BOOST_CHECK_MESSAGE(accountPing->FetchStateJson(pingState, "", {}, true),
                      "Fetch pingState failed");
  int pingCount = -1;
  if (pingState.isMember("count")) {
    pingCount = atoi(pingState["count"].asCString());
  }

  Json::Value pongState;
  BOOST_CHECK_MESSAGE(accountPong->FetchStateJson(pongState, "", {}, true),
                      "Fetch pongState failed");
  int pongCount = -1;
  if (pongState.isMember("count")) {
    pongCount = atoi(pongState["count"].asCString());
  }
  BOOST_CHECK_MESSAGE(pingCount == 0 && pongCount == 0,
                      "Ping / Pong did not reach count 0.");

  LOG_GENERAL(INFO, "Ping and pong bounced back to reach 0. Successful.");

  /* ------------------------------------------------------------------- */
}

BOOST_AUTO_TEST_CASE(testChainCalls) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey owner(priv1, {priv1}), contrA(priv2, {priv2}),
      contrB(priv3, {priv3}), contractaddress(priv4, {priv4});
  Address ownerAddr, aAddr, bAddr, cAddr;
  uint64_t nonce = 0;

  setup();

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  AccountStore::GetInstance().AddAccountTemp(ownerAddr,
                                             {2000000000000000, nonce});

  aAddr = Account::GetAddressForContract(ownerAddr, nonce);
  bAddr = Account::GetAddressForContract(ownerAddr, nonce + 1);
  cAddr = Account::GetAddressForContract(ownerAddr, nonce + 2);

  LOG_GENERAL(INFO, "aAddr: " << aAddr << " ; bAddr: " << bAddr
                              << " ; cAddr: " << cAddr);

  /* ------------------------------------------------------------------- */

  // Deploying the contract can use data from the 1st Scilla test.
  ScillaTestUtil::ScillaTest tContrA;
  if (!ScillaTestUtil::GetScillaTest(tContrA, "chain-call-balance-1", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test chain-call-balance-1.");
    return;
  }

  uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(tContrA.blockchain);
  ScillaTestUtil::RemoveCreationBlockFromInit(tContrA.init);
  ScillaTestUtil::RemoveThisAddressFromInit(tContrA.init);

  // Transaction to deploy contrA
  std::string initStrA =
      JSONUtils::GetInstance().convertJsontoStr(tContrA.init);
  bytes dataA(initStrA.begin(), initStrA.end());
  Transaction tx0(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, tContrA.code, dataA);
  TransactionReceipt tr0;
  TxnStatus error_code;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx0, tr0,
                                                 error_code);
  Account* accountA = AccountStore::GetInstance().GetAccountTemp(aAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountA != nullptr, "Error with creation of contract A");
  nonce++;

  ScillaTestUtil::ScillaTest tContrB;
  if (!ScillaTestUtil::GetScillaTest(tContrB, "chain-call-balance-2", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test chain-call-balance-2.");
    return;
  }

  ScillaTestUtil::RemoveCreationBlockFromInit(tContrB.init);
  ScillaTestUtil::RemoveThisAddressFromInit(tContrB.init);

  // Transaction to deploy contrB
  std::string initStrB =
      JSONUtils::GetInstance().convertJsontoStr(tContrB.init);
  bytes dataB(initStrB.begin(), initStrB.end());
  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, tContrB.code, dataB);
  TransactionReceipt tr1;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                 error_code);
  Account* accountB = AccountStore::GetInstance().GetAccountTemp(bAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountB != nullptr, "Error with creation of contract B");
  nonce++;

  ScillaTestUtil::ScillaTest tContrC;
  if (!ScillaTestUtil::GetScillaTest(tContrC, "chain-call-balance-3", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test chain-call-balance-3.");
    return;
  }

  ScillaTestUtil::RemoveCreationBlockFromInit(tContrC.init);
  ScillaTestUtil::RemoveThisAddressFromInit(tContrC.init);

  // Transaction to deploy contrC
  std::string initStrC =
      JSONUtils::GetInstance().convertJsontoStr(tContrC.init);
  bytes dataC(initStrC.begin(), initStrC.end());
  Transaction tx2(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 50000, tContrC.code, dataC);
  TransactionReceipt tr2;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx2, tr2,
                                                 error_code);
  Account* accountC = AccountStore::GetInstance().GetAccountTemp(cAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountC != nullptr, "Error with creation of contract C");
  nonce++;

  LOG_GENERAL(INFO, "Deployed contracts A, B, and C.");

  /* ------------------------------------------------------------------- */
  // Transfer 100 each to contracts A, B, and C.
  /* ------------------------------------------------------------------- */

  {
    Json::Value m;
    m["_tag"] = "simply_accept";
    m["_amount"] = "100";
    m["params"].resize(0);

    bytes m_data;
    ScillaTestUtil::PrepareMessageData(m, m_data);

    // Fund contrA
    Transaction txFundA(DataConversion::Pack(CHAIN_ID, 1), nonce, aAddr, owner,
                        100, PRECISION_MIN_VALUE, 50000, {}, m_data);
    TransactionReceipt trFundA;
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, txFundA,
                                                   trFundA, error_code);
    nonce++;
    // Fund contrB
    Transaction txFundB(DataConversion::Pack(CHAIN_ID, 1), nonce, bAddr, owner,
                        100, PRECISION_MIN_VALUE, 50000, {}, m_data);
    TransactionReceipt trFundB;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, txFundB,
                                                   trFundB, error_code);
    nonce++;
    // Fund contrC
    Transaction txFundC(DataConversion::Pack(CHAIN_ID, 1), nonce, cAddr, owner,
                        100, PRECISION_MIN_VALUE, 50000, {}, m_data);
    TransactionReceipt trFundC;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, txFundC,
                                                   trFundC, error_code);
    nonce++;
  }

  bytes data;
  // Replace addrB and addrC in parameter of message to contrA.
  for (auto it = tContrA.message["params"].begin();
       it != tContrA.message["params"].end(); it++) {
    if ((*it)["vname"] == "addrB") {
      (*it)["value"] = "0x" + bAddr.hex();
    } else if ((*it)["vname"] == "addrC") {
      (*it)["value"] = "0x" + cAddr.hex();
    }
  }
  uint64_t amount = ScillaTestUtil::PrepareMessageData(tContrA.message, data);
  Transaction tx3(DataConversion::Pack(CHAIN_ID, 1), nonce, aAddr, owner,
                  amount, PRECISION_MIN_VALUE, 50000, {}, data);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx3, tr3,
                                                     error_code)) {
    nonce++;
  }

  uint128_t aBal =
      AccountStore::GetInstance().GetAccountTemp(aAddr)->GetBalance();
  uint128_t bBal =
      AccountStore::GetInstance().GetAccountTemp(bAddr)->GetBalance();
  uint128_t cBal =
      AccountStore::GetInstance().GetAccountTemp(cAddr)->GetBalance();

  LOG_GENERAL(INFO, "Call chain balances obtained: A: "
                        << aBal << ". B: " << bBal << ". C: " << cBal);
  LOG_GENERAL(INFO, "Call chain balances expected: A: " << 100 << ". B: " << 150
                                                        << ". C: " << 100);

  BOOST_CHECK_MESSAGE(aBal == 100 && bBal == 150 && cBal == 100,
                      "Call chain balance test failed.");

  /* ------------------------------------------------------------------- */
}

bool mapHandler([[gnu::unused]] const std::string& index, const Json::Value& s,
                [[gnu::unused]] std::map<std::string, bytes> state_entries) {
  LOG_MARKER();

  LOG_GENERAL(INFO, "s: " << JSONUtils::GetInstance().convertJsontoStr(s));

  for (const auto& v : s) {
    if (!v.isMember("key") || v.isMember("val")) {
      return false;
    }

    string t_index = index + "." + v["key"].asString();
    if (v["val"] == Json::arrayValue) {
      for (const auto& u : v["val"]) {
        mapHandler(t_index, u, state_entries);
      }
    } else {
      state_entries.emplace(
          t_index, DataConversion::StringToCharArray(
                       JSONUtils::GetInstance().convertJsontoStr(v["val"])));
    }
  }

  return true;
}

// Comment due to deprecated function used
BOOST_AUTO_TEST_CASE(testStoragePerf) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey ownerKeyPair(priv1, {priv1});
  Address ownerAddr = Account::GetAddressFromPublicKey(ownerKeyPair.second);
  const uint128_t bal{std::numeric_limits<uint128_t>::max()};
  uint64_t nonce = 0;
  const unsigned int numDeployments = 1;
  const unsigned int numMapEntries = 10;

  ofstream report;
  report.open("perf_report.csv");
  report << "deployment_microsec,deployment_gas,invoke_microsec,invoke_gas\n";

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();
  AccountStore::GetInstance().AddAccountTemp(ownerAddr, {bal, nonce});

  for (unsigned int i = 0; i < numDeployments; i++) {
    Address contractAddr = Account::GetAddressForContract(ownerAddr, nonce);

    // Deploy the contract using data from the 2nd Scilla test.
    ScillaTestUtil::ScillaTest t2;
    if (!ScillaTestUtil::GetScillaTest(t2, "fungible-token", 2)) {
      LOG_GENERAL(WARNING, "Unable to fetch test fungible-token_2.");
      return;
    }

    // Replace owner address in init.json.
    for (auto& it : t2.init) {
      if (it["vname"] == "owner") {
        it["value"] = "0x" + ownerAddr.hex();
      }
    }

    ScillaTestUtil::RemoveThisAddressFromInit(t2.init);
    ScillaTestUtil::RemoveCreationBlockFromInit(t2.init);

    uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t2.blockchain);

    // Transaction to deploy contract.
    std::string initStr = JSONUtils::GetInstance().convertJsontoStr(t2.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, nonce, NullAddress, ownerKeyPair, 0, PRECISION_MIN_VALUE,
                    500000, t2.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx0, tr0,
                                                   error_code);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    nonce++;

    Account* account = AccountStore::GetInstance().GetAccountTemp(contractAddr);

    // We should now have a new account.
    BOOST_CHECK_MESSAGE(account != nullptr,
                        "Error with creation of contract account");

    report << timeElapsedDeployment << "," << tr0.GetCumGas() << ",";

    for (unsigned int i = 0; i < numMapEntries; i++) {
      bytes hodler(ACC_ADDR_SIZE);
      std::string hodler_str;
      RAND_bytes(hodler.data(), ACC_ADDR_SIZE);
      DataConversion::Uint8VecToHexStr(hodler, hodler_str);
      std::string hodlerNumTokens = "168";

      Json::Value kvPair;
      kvPair["key"] = "0x" + hodler_str;
      kvPair["val"] = hodlerNumTokens;

      for (auto& it : t2.state) {
        if (it["vname"] == "balances") {
          // we have to artifically insert the owner here
          if (i == 0) {
            Json::Value ownerBal;
            ownerBal["key"] = "0x" + ownerAddr.hex();
            ownerBal["val"] = "88888888";
            it["value"][i] = ownerBal;
            continue;
          }

          it["value"][i] = kvPair;
        }
      }
    }

    LOG_GENERAL(INFO, "marker1");

    std::map<std::string, bytes> state_entries;
    // save the state
    for (auto& s : t2.state) {
      std::string index = contractAddr.hex();
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      index += "." + s["vname"].asString();
      if (s["value"] == Json::arrayValue) {
        if (!mapHandler(index, s["value"], state_entries)) {
          LOG_GENERAL(WARNING, "state format is invalid");
          break;
        }
      } else {
        state_entries.emplace(
            index, DataConversion::StringToCharArray(
                       JSONUtils::GetInstance().convertJsontoStr(s["value"])));
      }
    }

    LOG_GENERAL(INFO, "marker2");

    account->UpdateStates(contractAddr, state_entries, {}, true);

    bytes dataTransfer;
    uint64_t amount =
        ScillaTestUtil::PrepareMessageData(t2.message, dataTransfer);

    Transaction tx1(1, nonce, contractAddr, ownerKeyPair, amount,
                    PRECISION_MIN_VALUE, 500000, {}, dataTransfer);
    TransactionReceipt tr1;

    auto startTimeCall = r_timer_start();
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                   error_code);
    auto timeElapsedCall = r_timer_end(startTimeCall);
    nonce++;

    report << timeElapsedCall << "," << tr1.GetCumGas() << "\n";
  }

  report.close();
}

BOOST_AUTO_TEST_CASE(testFungibleToken) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  // 1. Bootstrap our test case.
  PairOfKey owner(priv1, {priv1});
  Address ownerAddr, contrAddr;
  uint64_t nonce = 0;

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  const unsigned int numHodlers[] = {10, 20, 30, 40, 50};

  for (auto hodlers : numHodlers) {
    AccountStore::GetInstance().Init();

    const uint128_t bal{std::numeric_limits<uint128_t>::max()};

    ownerAddr = Account::GetAddressFromPublicKey(owner.second);
    AccountStore::GetInstance().AddAccountTemp(ownerAddr, {bal, nonce});

    contrAddr = Account::GetAddressForContract(ownerAddr, nonce);
    LOG_GENERAL(INFO, "FungibleToken Address: " << contrAddr.hex());

    // Deploy the contract using data from the 2nd Scilla test.
    ScillaTestUtil::ScillaTest t2;
    if (!ScillaTestUtil::GetScillaTest(t2, "fungible-token", 2)) {
      LOG_GENERAL(WARNING, "Unable to fetch test fungible-token_2.");
      return;
    }

    // Replace owner address in init.json.
    for (auto& it : t2.init) {
      if (it["vname"] == "owner") {
        it["value"] = "0x" + ownerAddr.hex();
      }
    }

    ScillaTestUtil::RemoveThisAddressFromInit(t2.init);
    // and remove _creation_block (automatic insertion later).
    ScillaTestUtil::RemoveCreationBlockFromInit(t2.init);

    uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t2.blockchain);

    // Transaction to deploy contract.
    std::string initStr = JSONUtils::GetInstance().convertJsontoStr(t2.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE,
                    500000, t2.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx0, tr0,
                                                   error_code);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    Account* account = AccountStore::GetInstance().GetAccountTemp(contrAddr);

    // We should now have a new account.
    BOOST_CHECK_MESSAGE(account != nullptr,
                        "Error with creation of contract account");

    LOG_GENERAL(INFO, "Contract size = "
                          << ScillaTestUtil::GetFileSize("input.scilla"));
    LOG_GENERAL(INFO, "Gas used (deployment) = " << tr0.GetCumGas());
    LOG_GENERAL(INFO, "UpdateAccounts (deployment) (micro) = "
                          << timeElapsedDeployment);
    nonce++;

    // 2. Pre-generate and save a large map and save it to LDB
    for (unsigned int i = 0; i < hodlers; i++) {
      bytes hodler(ACC_ADDR_SIZE);
      std::string hodler_str;
      RAND_bytes(hodler.data(), ACC_ADDR_SIZE);
      DataConversion::Uint8VecToHexStr(hodler, hodler_str);
      std::string hodlerNumTokens = "1";

      Json::Value kvPair;
      kvPair["key"] = "0x" + hodler_str;
      kvPair["val"] = hodlerNumTokens;

      for (auto& it : t2.state) {
        if (it["vname"] == "balances") {
          // we have to artifically insert the owner here
          if (i == 0) {
            Json::Value ownerBal;
            ownerBal["key"] = "0x" + ownerAddr.hex();
            ownerBal["val"] = "88888888";
            it["value"][i] = ownerBal;
            continue;
          }

          it["value"][i] = kvPair;
        }
      }
    }

    std::map<std::string, bytes> state_entries;
    // save the state
    for (auto& s : t2.state) {
      std::string index = contrAddr.hex();
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      index += "." + s["vname"].asString();
      if (s["value"] == Json::arrayValue) {
        if (!mapHandler(index, s["value"], state_entries)) {
          LOG_GENERAL(WARNING, "state format is invalid");
          break;
        }
      } else {
        state_entries.emplace(
            index, DataConversion::StringToCharArray(
                       JSONUtils::GetInstance().convertJsontoStr(s["value"])));
      }
    }
    account->UpdateStates(contrAddr, state_entries, {}, true);

    // 3. Create a call to Transfer from one account to another
    bytes dataTransfer;
    uint64_t amount =
        ScillaTestUtil::PrepareMessageData(t2.message, dataTransfer);

    Transaction tx1(1, nonce, contrAddr, owner, amount, PRECISION_MIN_VALUE,
                    88888888, {}, dataTransfer);
    TransactionReceipt tr1;

    auto startTimeCall = r_timer_start();
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                   error_code);
    auto timeElapsedCall = r_timer_end(startTimeCall);

    LOG_GENERAL(
        INFO, "Size of output = " << ScillaTestUtil::GetFileSize("output.json"))
    LOG_GENERAL(INFO, "Size of map (balances) = " << hodlers);
    LOG_GENERAL(INFO, "Gas used (invocation) = " << tr1.GetCumGas());
    LOG_GENERAL(INFO, "UpdateAccounts (micro) = " << timeElapsedCall);
    nonce++;
  }
}

BOOST_AUTO_TEST_CASE(testNonFungibleToken) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  // 1. Bootstrap test case
  const unsigned int numOperators = 5;
  const unsigned int numHodlers[] = {10, 20, 30, 40, 50};
  std::string numTokensOwned = "1";

  PairOfKey owner(priv1, {priv1});
  PairOfKey sender;  // also an operator, assigned later.
  Address ownerAddr, senderAddr, contrAddr;

  vector<PairOfKey> operators;
  vector<Address> operatorAddrs;

  uint64_t ownerNonce = 0;
  uint64_t senderNonce = 0;

  // generate operator PairOfKeys
  for (unsigned int i = 0; i < numOperators; i++) {
    PairOfKey oprtr = Schnorr::GenKeyPair();
    Address operatorAddr = Account::GetAddressFromPublicKey(oprtr.second);
    operators.emplace_back(oprtr);
    operatorAddrs.emplace_back(operatorAddr);

    if (i == 0) {
      sender = oprtr;
    }
  }

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  const uint128_t bal{std::numeric_limits<uint128_t>::max()};

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  AccountStore::GetInstance().AddAccountTemp(ownerAddr, {bal, ownerNonce});

  senderAddr = Account::GetAddressFromPublicKey(sender.second);
  AccountStore::GetInstance().AddAccountTemp(senderAddr, {bal, senderNonce});

  for (auto hodlers : numHodlers) {
    contrAddr = Account::GetAddressForContract(ownerAddr, ownerNonce);
    LOG_GENERAL(INFO, "NonFungibleToken Address: " << contrAddr.hex());

    // Deploy the contract using data from the 10th Scilla test.
    ScillaTestUtil::ScillaTest t10;
    if (!ScillaTestUtil::GetScillaTest(t10, "nonfungible-token", 10)) {
      LOG_GENERAL(WARNING, "Unable to fetch test nonfungible-token_10;.");
      return;
    }

    // Replace owner address in init.json.
    for (auto& it : t10.init) {
      if (it["vname"] == "owner") {
        it["value"] = "0x" + ownerAddr.hex();
      }
    }
    // and remove _creation_block (automatic insertion later).
    ScillaTestUtil::RemoveCreationBlockFromInit(t10.init);
    ScillaTestUtil::RemoveThisAddressFromInit(t10.init);

    uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t10.blockchain);

    // Transaction to deploy contract.
    std::string initStr = JSONUtils::GetInstance().convertJsontoStr(t10.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, ownerNonce, NullAddress, owner, 0, PRECISION_MIN_VALUE,
                    500000, t10.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    TxnStatus error_code;
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx0, tr0,
                                                   error_code);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    Account* account = AccountStore::GetInstance().GetAccountTemp(contrAddr);

    // We should now have a new account.
    BOOST_CHECK_MESSAGE(account != nullptr,
                        "Error with creation of contract account");
    LOG_GENERAL(INFO, "Contract size = "
                          << ScillaTestUtil::GetFileSize("input.scilla"));
    LOG_GENERAL(INFO, "Gas used (deployment) = " << tr0.GetCumGas());
    LOG_GENERAL(INFO, "UpdateAccounts (micro) = " << timeElapsedDeployment);
    ownerNonce++;

    // 2. Insert n owners of 1 token each, with 5 operator approvals.
    //  Map Uint256 ByStr20
    Json::Value tokenOwnerMap(Json::arrayValue);
    // Map ByStr20 Uint256
    Json::Value ownedTokenCount(Json::arrayValue);
    // Map ByStr20 (Map ByStr20 Bool)
    Json::Value operatorApprovals(Json::arrayValue);

    Json::Value adtBoolTrue;
    adtBoolTrue["constructor"] = "True",
    adtBoolTrue["argtypes"] = Json::arrayValue;
    adtBoolTrue["arguments"] = Json::arrayValue;

    Json::Value approvedOperators(Json::arrayValue);
    for (auto& operatorAddr : operatorAddrs) {
      Json::Value operatorApprovalEntry;
      operatorApprovalEntry["key"] = "0x" + operatorAddr.hex();
      operatorApprovalEntry["val"] = adtBoolTrue;
      approvedOperators.append(operatorApprovalEntry);
    }

    for (unsigned int i = 0; i < hodlers; i++) {
      Address hodler;
      RAND_bytes(hodler.data(), ACC_ADDR_SIZE);

      // contract owner gets the first token
      if (i == 0) {
        hodler = Account::GetAddressFromPublicKey(owner.second);
      }

      // set ownership
      Json::Value tokenOwnerEntry;
      tokenOwnerEntry["key"] = to_string(i + 1);
      tokenOwnerEntry["val"] = "0x" + hodler.hex();
      tokenOwnerMap[i] = tokenOwnerEntry;

      // set token count
      Json::Value tokenCountEntry;
      tokenCountEntry["key"] = "0x" + hodler.hex();
      tokenCountEntry["val"] = numTokensOwned;
      ownedTokenCount[i] = tokenCountEntry;

      // set operator approval
      Json::Value ownerApprovalEntry;
      ownerApprovalEntry["key"] = "0x" + hodler.hex();
      ownerApprovalEntry["val"] = approvedOperators;
      operatorApprovals[i] = ownerApprovalEntry;
    }

    for (auto& it : t10.state) {
      std::string vname(it["vname"].asString());

      if (vname == "tokenOwnerMap") {
        it["value"] = tokenOwnerMap;
        continue;
      }

      if (vname == "ownedTokenCount") {
        it["value"] = ownedTokenCount;
      }

      if (vname == "operatorApprovals") {
        it["value"] = operatorApprovals;
        continue;
      }
    }

    std::map<std::string, bytes> state_entries;
    // save the state
    for (auto& s : t10.state) {
      std::string index = contrAddr.hex();
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      index += "." + s["vname"].asString();
      if (s["value"] == Json::arrayValue) {
        if (!mapHandler(index, s["value"], state_entries)) {
          LOG_GENERAL(WARNING, "state format is invalid");
          break;
        }
      } else {
        state_entries.emplace(
            index, DataConversion::StringToCharArray(
                       JSONUtils::GetInstance().convertJsontoStr(s["value"])));
      }
    }
    account->UpdateStates(contrAddr, state_entries, {}, true);

    // 3. Execute transferFrom as an operator
    boost::random::mt19937 rng;
    boost::random::uniform_int_distribution<> ownerDist(0, int(hodlers - 1));
    Json::Value randomReceiver = tokenOwnerMap[ownerDist(rng)];

    // modify t3.message
    for (auto& p : t10.message["params"]) {
      if (p["vname"] == "tokenId") {
        p["value"] = "1";
      }

      if (p["vname"] == "from") {
        p["value"] = "0x" + ownerAddr.hex();
      }

      if (p["vname"] == "to") {
        p["value"] = randomReceiver["val"];
      }
    }

    bytes dataTransfer;
    uint64_t amount =
        ScillaTestUtil::PrepareMessageData(t10.message, dataTransfer);

    Transaction tx1(1, senderNonce, contrAddr, sender, amount,
                    PRECISION_MIN_VALUE, 88888888, {}, dataTransfer);
    TransactionReceipt tr1;

    auto startTimeCall = r_timer_start();
    AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx1, tr1,
                                                   error_code);
    auto timeElapsedCall = r_timer_end(startTimeCall);

    LOG_GENERAL(
        INFO, "Size of output = " << ScillaTestUtil::GetFileSize("output.json"))
    LOG_GENERAL(INFO, "Size of map (inner) = " << numOperators);
    LOG_GENERAL(INFO, "Size of map (outer) = " << hodlers);
    LOG_GENERAL(INFO, "Gas used (transferFrom) = " << tr1.GetCumGas());
    LOG_GENERAL(INFO, "UpdateAccounts (micro) = " << timeElapsedCall);
    senderNonce++;
  }
}

// BOOST_AUTO_TEST_CASE(testDEX) {
//   INIT_STDOUT_LOGGER();
//   LOG_MARKER();

//   // 1. Bootstrap test case
//   const unsigned int numHodlers[] = {10, 20, 30, 40, 50};
//   const unsigned int numOrders = 10;
//   std::string numTokensOwned = "1";

//   PairOfKey ownerToken1(priv1, {priv1});
//   PairOfKey ownerToken2(priv2, {priv2});
//   PairOfKey ownerDex(priv3, {priv3});

//   Address ownerToken1Addr, ownerToken2Addr, ownerDexAddr, token1Addr,
//       token2Addr, dexAddr;

//   uint64_t ownerToken1Nonce = 0;
//   uint64_t ownerToken2Nonce = 0;
//   uint64_t ownerDexNonce = 0;

//   if (SCILLA_ROOT.empty()) {
//     LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
//     return;
//   }

//   AccountStore::GetInstance().Init();

//   const uint128_t bal{std::numeric_limits<uint128_t>::max()};

//   ownerToken1Addr = Account::GetAddressFromPublicKey(ownerToken1.second);
//   ownerToken2Addr = Account::GetAddressFromPublicKey(ownerToken2.second);
//   ownerDexAddr = Account::GetAddressFromPublicKey(ownerDex.second);
//   AccountStore::GetInstance().AddAccountTemp(ownerToken1Addr,
//                                              {bal, ownerToken1Nonce});
//   AccountStore::GetInstance().AddAccountTemp(ownerToken2Addr,
//                                              {bal, ownerToken2Nonce});
//   AccountStore::GetInstance().AddAccountTemp(ownerDexAddr,
//                                              {bal, ownerDexNonce});

//   for (auto hodlers : numHodlers) {
//     LOG_GENERAL(INFO, "\n\n===START TEST ITERATION===\n\n");
//     // Seller sells Token A for Token B. Buyer buys Token A with Token B.
//     // Execute makeOrder with Seller's private key
//     // Execute fillOrder with Buyer's private key

//     // Deploy the token contracts using the 5th Scilla test case for
//     // fungible-token.
//     ScillaTestUtil::ScillaTest fungibleTokenT5;
//     if (!ScillaTestUtil::GetScillaTest(fungibleTokenT5, "fungible-token", 5))
//     {
//       LOG_GENERAL(WARNING, "Unable to fetch test fungible-token_5;.");
//       return;
//     }

//     ScillaTestUtil::RemoveThisAddressFromInit(fungibleTokenT5.init);
//     ScillaTestUtil::RemoveCreationBlockFromInit(fungibleTokenT5.init);
//     uint64_t bnum =
//         ScillaTestUtil::GetBlockNumberFromJson(fungibleTokenT5.blockchain);
//     std::string initStr =
//         JSONUtils::GetInstance().convertJsontoStr(fungibleTokenT5.init);

//     bytes deployTokenData(initStr.begin(), initStr.end());

//     // Deploy TOKEN 1
//     token1Addr =
//         Account::GetAddressForContract(ownerToken1Addr, ownerToken1Nonce);
//     Transaction txDeployToken1(1, ownerToken1Nonce, NullAddress, ownerToken1,
//     0,
//                                PRECISION_MIN_VALUE, 500000,
//                                fungibleTokenT5.code, deployTokenData);
//     TransactionReceipt trDeplyoToken1;
//     AccountStore::GetInstance().UpdateAccountsTemp(
//         bnum, 1, true, txDeployToken1, trDeplyoToken1);
//     Account* token1Account =
//         AccountStore::GetInstance().GetAccountTemp(token1Addr);
//     ownerToken1Nonce++;
//     BOOST_CHECK_MESSAGE(token1Account != nullptr,
//                         "Error with creation of token 1 account");

//     // Deploy TOKEN 2
//     token2Addr =
//         Account::GetAddressForContract(ownerToken2Addr, ownerToken2Nonce);
//     Transaction txDeployToken2(1, ownerToken2Nonce, NullAddress, ownerToken2,
//     0,
//                                PRECISION_MIN_VALUE, 500000,
//                                fungibleTokenT5.code, deployTokenData);
//     TransactionReceipt trDeployToken2;
//     AccountStore::GetInstance().UpdateAccountsTemp(
//         bnum, 1, true, txDeployToken2, trDeployToken2);
//     Account* token2Account =
//         AccountStore::GetInstance().GetAccountTemp(token2Addr);
//     ownerToken2Nonce++;
//     BOOST_CHECK_MESSAGE(token2Account != nullptr,
//                         "Error with creation of token 2 account");

//     // Insert hodlers artifically
//     for (unsigned int i = 0; i < hodlers; i++) {
//       bytes hodler(ACC_ADDR_SIZE);
//       std::string hodlerAddr;
//       RAND_bytes(hodler.data(), ACC_ADDR_SIZE);
//       DataConversion::Uint8VecToHexStr(hodler, hodlerAddr);
//       std::string hodlerNumTokens = "1";

//       Json::Value kvPair;
//       kvPair["key"] = "0x" + hodlerAddr;
//       kvPair["val"] = hodlerNumTokens;

//       for (auto& it : fungibleTokenT5.state) {
//         if (it["vname"] == "balances") {
//           // we have to artifically insert the owner here
//           if (i == 0) {
//             Json::Value ownerBal;
//             ownerBal["key"] = "0x" + ownerToken1Addr.hex();
//             ownerBal["val"] = "88888888";
//             it["value"][i] = ownerBal;
//             continue;
//           }

//           if (i == 1) {
//             Json::Value ownerBal;
//             ownerBal["key"] = "0x" + ownerToken2Addr.hex();
//             ownerBal["val"] = "88888888";
//             it["value"][i] = ownerBal;
//             continue;
//           }

//           it["value"][i] = kvPair;
//         }
//       }
//     }

//     std::map<std::string, bytes> token_state_entries_1;
//     std::map<std::string, bytes> token_state_entries_2;
//     // save the state
//     for (auto& s : fungibleTokenT5.state) {
//       std::string index = token1Addr.hex();
//       if (s["vname"].asString() == "_balance") {
//         continue;
//       }

//       index += "." + s["vname"].asString();
//       if (s["value"] == Json::arrayValue) {
//         if (!mapHandler(index, s["value"], token_state_entries_1)) {
//           LOG_GENERAL(WARNING, "state format is invalid");
//           break;
//         }
//       } else {
//         token_state_entries_1.emplace(
//             index, DataConversion::StringToCharArray(
//                        JSONUtils::GetInstance().convertJsontoStr(s["value"])));
//       }
//     }
//     // save the state
//     for (auto& s : fungibleTokenT5.state) {
//       std::string index = token2Addr.hex();
//       if (s["vname"].asString() == "_balance") {
//         continue;
//       }

//       index += "." + s["vname"].asString();
//       if (s["value"] == Json::arrayValue) {
//         if (!mapHandler(index, s["value"], token_state_entries_2)) {
//           LOG_GENERAL(WARNING, "state format is invalid");
//           break;
//         }
//       } else {
//         token_state_entries_2.emplace(
//             index, DataConversion::StringToCharArray(
//                        JSONUtils::GetInstance().convertJsontoStr(s["value"])));
//       }
//     }
//     token1Account->UpdateStates(token1Addr, token_state_entries_1, {}, true);
//     token2Account->UpdateStates(token2Addr, token_state_entries_2, {}, true);

//     // Deploy DEX
//     // Deploy the DEX contract with the 0th test case, but use custom
//     // messages for makeOrder/fillOrder.
//     ScillaTestUtil::ScillaTest dexT1;
//     if (!ScillaTestUtil::GetScillaTest(dexT1, "simple-dex", 1)) {
//       LOG_GENERAL(WARNING, "Unable to fetch test simple-dex_1.");
//       return;
//     }

//     // remove _creation_block (automatic insertion later).
//     ScillaTestUtil::RemoveThisAddressFromInit(dexT1.init);
//     ScillaTestUtil::RemoveCreationBlockFromInit(dexT1.init);
//     for (auto& p : dexT1.init) {
//       if (p["vname"].asString() == "contractOwner") {
//         p["value"] = "0x" + ownerDexAddr.hex();
//         break;
//       }
//     }

//     uint64_t dexBnum =
//     ScillaTestUtil::GetBlockNumberFromJson(dexT1.blockchain); std::string
//     dexInitStr =
//         JSONUtils::GetInstance().convertJsontoStr(dexT1.init);
//     bytes deployDexData(dexInitStr.begin(), dexInitStr.end());

//     dexAddr = Account::GetAddressForContract(ownerDexAddr, ownerDexNonce);
//     Transaction txDeployDex(1, ownerDexNonce, NullAddress, ownerDex, 0,
//                             PRECISION_MIN_VALUE, 500000, dexT1.code,
//                             deployDexData);
//     TransactionReceipt trDeployDex;
//     auto startTimeDeployment = r_timer_start();
//     AccountStore::GetInstance().UpdateAccountsTemp(dexBnum, 1, true,
//                                                    txDeployDex, trDeployDex);
//     auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
//     Account* dexAccount =
//     AccountStore::GetInstance().GetAccountTemp(dexAddr);
//     BOOST_CHECK_MESSAGE(dexAccount != nullptr,
//                         "Error with creation of dex account");
//     LOG_GENERAL(INFO, "\n\n=== Deployed DEX ===\n\n");
//     LOG_GENERAL(INFO, "Contract size = "
//                           << ScillaTestUtil::GetFileSize("input.scilla"));
//     LOG_GENERAL(INFO, "Gas used (deployment) = " << trDeployDex.GetCumGas());
//     LOG_GENERAL(INFO, "UpdateAccounts (deployment) (micro) = "
//                           << timeElapsedDeployment);
//     ownerDexNonce++;

//     // Artificially populate the order book
//     Json::Value orderBook;
//     Json::Value orderInfo;
//     std::vector<Contract::StateEntry> dex_state_entries;
//     for (unsigned int i = 0; i < numOrders; i++) {
//       Json::Value info;

//       bytes sender(ACC_ADDR_SIZE);
//       std::string sender_str;
//       DataConversion::Uint8VecToHexStr(sender, sender_str);
//       RAND_bytes(sender.data(), ACC_ADDR_SIZE);

//       bytes orderId(COMMON_HASH_SIZE);
//       std::string orderId_str;
//       DataConversion::Uint8VecToHexStr(orderId, orderId_str);
//       RAND_bytes(orderId.data(), COMMON_HASH_SIZE);
//       std::string orderIdHex = "0x" + orderId_str;

//       info["key"] = orderIdHex;
//       info["val"]["constructor"] = "Pair";
//       info["val"]["argtypes"][0] = "ByStr20";
//       info["val"]["argtypes"][1] = "BNum";
//       info["val"]["arguments"][0] = "0x" + sender_str;
//       info["val"]["arguments"][1] = "168";
//       orderInfo[i] = info;

//       // Token1
//       Json::Value sell;
//       sell["constructor"] = "Pair";
//       sell["argtypes"][0] = "ByStr20";
//       sell["argtypes"][1] = "Uint128";
//       sell["arguments"][0] = "0x" + token1Addr.hex();
//       sell["arguments"][1] = "1";

//       // Token 2
//       Json::Value buy;
//       buy["constructor"] = "Pair";
//       buy["argtypes"][0] = "ByStr20";
//       buy["argtypes"][1] = "Uint128";
//       buy["arguments"][0] = "0x" + token2Addr.hex();
//       buy["arguments"][1] = "1";

//       Json::Value order;
//       order["key"] = orderIdHex;
//       order["val"]["constructor"] = "Pair";
//       order["val"]["argtypes"][0] = "Pair (ByStr20) (Uint128)";
//       order["val"]["argtypes"][1] = "Pair (ByStr20) (Uint128)";
//       order["val"]["arguments"][0] = sell;
//       order["val"]["arguments"][1] = buy;

//       orderBook[i] = order;
//     }

//     std::map<std::string, bytes> state_entries;
//     std::string index = dexAddr.hex();
//     std::string orderbook_index = index + "." + "orderbook";
//     mapHandler(orderbook_index, orderBook, state_entries);
//     std::string orderinfo_index = index + "." + "orderInfo";
//     mapHandler(orderinfo_index, orderInfo, state_entries);

//     dexAccount->UpdateStates(dexAddr, state_entries, {}, true);

//     // Approve DEX on Token A and Token B respectively
//     Json::Value dataApprove = fungibleTokenT5.message;
//     dataApprove["params"][0]["value"] = "0x" + dexAddr.hex();
//     bytes dataApproveBytes;
//     ScillaTestUtil::PrepareMessageData(dataApprove, dataApproveBytes);

//     // Execute Approve on Token A in favour of DEX
//     Transaction txApproveToken1(1, ownerToken1Nonce, token1Addr, ownerToken1,
//     0,
//                                 PRECISION_MIN_VALUE, 88888888, {},
//                                 dataApproveBytes);
//     TransactionReceipt trApproveToken1;

//     AccountStore::GetInstance().UpdateAccountsTemp(
//         bnum, 1, true, txApproveToken1, trApproveToken1);
//     ownerToken1Nonce++;

//     // Execute Approve on Token B in favour of DEX
//     Transaction txApproveToken2(1, ownerToken2Nonce, token2Addr, ownerToken2,
//     0,
//                                 PRECISION_MIN_VALUE, 88888888, {},
//                                 dataApproveBytes);
//     TransactionReceipt trApproveToken2;

//     AccountStore::GetInstance().UpdateAccountsTemp(
//         bnum, 1, true, txApproveToken2, trApproveToken2);
//     ownerToken2Nonce++;

//     // Execute updateAddress as dexOwner
//     Json::Value dataUpdateAddress = dexT1.message;
//     dataUpdateAddress["params"][0]["value"] = "0x" + ownerDexAddr.hex();

//     bytes dataUpdateAddressBytes;
//     ScillaTestUtil::PrepareMessageData(dataUpdateAddress,
//                                        dataUpdateAddressBytes);

//     Transaction txUpdateAddress(1, ownerDexNonce, dexAddr, ownerDex, 0,
//                                 PRECISION_MIN_VALUE, 88888888, {},
//                                 dataUpdateAddressBytes);
//     TransactionReceipt trUpdateAddress;

//     AccountStore::GetInstance().UpdateAccountsTemp(
//         bnum, 1, true, txUpdateAddress, trUpdateAddress);
//     ownerDexNonce++;

//     // Execute makeOrder as ownerToken1
//     Json::Value dataMakeOrder = dexT1.message;
//     Json::Value dataMakeOrderParams;

//     dataMakeOrder["_tag"] = "makeOrder";

//     dataMakeOrderParams[0]["vname"] = "tokenA";
//     dataMakeOrderParams[0]["type"] = "ByStr20";
//     dataMakeOrderParams[0]["value"] = "0x" + token1Addr.hex();

//     dataMakeOrderParams[1]["vname"] = "tokenB";
//     dataMakeOrderParams[1]["type"] = "ByStr20";
//     dataMakeOrderParams[1]["value"] = "0x" + token2Addr.hex();

//     dataMakeOrderParams[2]["vname"] = "valueA";
//     dataMakeOrderParams[2]["type"] = "Uint128";
//     dataMakeOrderParams[2]["value"] = "168";

//     dataMakeOrderParams[3]["vname"] = "valueB";
//     dataMakeOrderParams[3]["type"] = "Uint128";
//     dataMakeOrderParams[3]["value"] = "168";

//     dataMakeOrderParams[4]["vname"] = "expirationBlock";
//     dataMakeOrderParams[4]["type"] = "BNum";
//     dataMakeOrderParams[4]["value"] = "200";

//     dataMakeOrder["params"] = dataMakeOrderParams;

//     bytes dataMakeOrderBytes;
//     ScillaTestUtil::PrepareMessageData(dataMakeOrder, dataMakeOrderBytes);

//     Transaction txMakeOrder(1, ownerToken1Nonce, dexAddr, ownerToken1, 0,
//                             PRECISION_MIN_VALUE, 88888888, {},
//                             dataMakeOrderBytes);
//     TransactionReceipt trMakeOrder;

//     LOG_GENERAL(INFO, "\n\n=== EXECUTING makeOrder ===\n\n");
//     auto startMakeOrder = r_timer_start();
//     AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true,
//     txMakeOrder,
//                                                    trMakeOrder);
//     auto timeMakeOrder = r_timer_end(startMakeOrder);
//     ownerToken1Nonce++;

//     // At this point:
//     // - sender's balance should have decreased, because the DEX contract
//     // will have taken custody of the token.
//     // - there should be an additional order in simple-dex.
//     Json::Value token1State;
//     BOOST_CHECK_MESSAGE(
//         token1Account->FetchStateJson(token1State, "", {}, true),
//         "Fetch token1State failed");
//     for (auto& s : token1State) {
//       if (s["vname"] == "balances") {
//         for (auto& hodl : s["value"]) {
//           if (hodl["key"] == "0x" + ownerToken1Addr.hex()) {
//             BOOST_CHECK_MESSAGE(hodl["val"] == "88888720",
//                                 "Owner 1's balance did not decrease!");
//             LOG_GENERAL(INFO, "Owner 1 balance = " << hodl["val"]);
//           }
//         }
//       }
//     }

//     Json::Value logs = trMakeOrder.GetJsonValue();
//     std::string id = logs["event_logs"][0]["params"][0]["value"].asString();
//     LOG_GENERAL(INFO, "New order ID = " << id);

//     Json::Value simpleDexState;
//     BOOST_CHECK_MESSAGE(
//         dexAccount->FetchStateJson(simpleDexState, "", {}, true),
//         "Fetch token1State failed");
//     bool hasNewOrder = false;

//     for (auto& s : simpleDexState) {
//       LOG_GENERAL(INFO, "s: " <<
//       JSONUtils::GetInstance().convertJsontoStr(s));
//       // if (s["vname"] == "orderbook") {
//       //   for (auto& ord : s["value"]) {
//       //     if (ord["key"] == id) {
//       //       hasNewOrder = true;
//       //       LOG_GENERAL(INFO, "New order = " << ord["val"]);
//       //     }
//       //   }
//       // }
//     }

//     BOOST_CHECK_MESSAGE(hasNewOrder == true,
//                         "Did not receive a new order in simple-dex!");

//     LOG_GENERAL(
//         INFO, "Size of output = " <<
//         ScillaTestUtil::GetFileSize("output.json"))
//     LOG_GENERAL(INFO, "Size of map (Token A) = " << hodlers);
//     LOG_GENERAL(INFO, "Size of map (Token B) = " << hodlers);
//     LOG_GENERAL(INFO, "Receipt makeOrder = " << trMakeOrder.GetString());
//     LOG_GENERAL(INFO, "Gas used (makeOrder) = " << trMakeOrder.GetCumGas());
//     LOG_GENERAL(INFO, "Time elapsed (updateAccount) = " << timeMakeOrder);
//     LOG_GENERAL(INFO, "\n\n=== END TEST ITERATION ===\n\n");
//   }
// }

BOOST_AUTO_TEST_CASE(testCreateContractJsonOutput) {
  std::string scillaOutput =
      "{ \
    \"scilla_major_version\": \"0\", \
    \"gas_remaining\": \"7290\", \
    \"_accepted\": \"false\", \
    \"message\": null, \
    \"states\": [ \
      { \"vname\": \"_balance\", \"type\": \"Uint128\", \"value\": \"0\" }, \
      { \"vname\": \"touches\", \"type\": \"Map (String) (Bool)\", \"value\": [] } \
    ], \
    \"events\": [] \
  }";

  try {
    Json::Value jsonValue;
    std::string errors;
    Json::CharReaderBuilder builder;
    auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
    if (!reader->parse(scillaOutput.c_str(),
                       scillaOutput.c_str() + scillaOutput.size(), &jsonValue,
                       &errors)) {
      LOG_GENERAL(WARNING,
                  "Failed to parse return result to json: " << scillaOutput);
      LOG_GENERAL(WARNING, "Error: " << errors);
      return;
    }

    bool passed = jsonValue["message"].type() == Json::nullValue &&
                  jsonValue["states"].type() == Json::arrayValue &&
                  jsonValue["events"].type() == Json::arrayValue;
    BOOST_REQUIRE(passed);

  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Failed to parse tag information, exception: " << e.what());
    return;
  }
}

BOOST_AUTO_TEST_SUITE_END()
