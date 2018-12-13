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

#include "ScillaTestUtil.h"

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

PrivKey priv1(
    DataConversion::HexStrToUint8Vec(
        "1658F915F3F9AE35E6B471B7670F53AD1A5BE15D7331EC7FD5E503F21D3450C8"),
    0),
    priv2(
        DataConversion::HexStrToUint8Vec(
            "0FC87BC5ACF5D1243DE7301972B9649EE31688F291F781396B0F67AD98A88147"),
        0),
    priv3(
        DataConversion::HexStrToUint8Vec(
            "0AB52CF5D3F9A1E730243DB96419729EE31688F29B0F67AD98A881471F781396"),
        0);

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testCrowdfunding) {

  KeyPair owner(priv1, {priv1}), donor1(priv2, {priv2}), donor2(priv3, {priv3});
  Address ownerAddr, donor1Addr, donor2Addr, contrAddr;
  uint64_t nonce = 0;

  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  donor1Addr = Account::GetAddressFromPublicKey(donor1.second);
  donor2Addr = Account::GetAddressFromPublicKey(donor2.second);

  AccountStore::GetInstance().AddAccount(ownerAddr, {2000000, nonce});
  AccountStore::GetInstance().AddAccount(donor1Addr, {2000000, nonce});
  AccountStore::GetInstance().AddAccount(donor2Addr, {2000000, nonce});

  contrAddr = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "CrowdFunding Address: " << contrAddr);

  // Deploying the contract can use data from the 1st Scilla test.
  ScillaTestUtil::ScillaTest t1;
  if (!ScillaTestUtil::GetScillaTest(t1, "crowdfunding", 1)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_1.");
    return;
  }

  // Replace owner address in init.json.
  for (auto it = t1.init.begin(); it != t1.init.end(); it++) {
    if ((*it)["vname"] == "owner") (*it)["value"] = "0x" + ownerAddr.hex();
  }
  // and remove _creation_block (automatic insertion later).
  ScillaTestUtil::RemoveCreationBlockFromInit(t1.init);

  uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t1.blockchain);

  // Transaction to deploy contract.
  std::string initStr = JSONUtils::convertJsontoStr(t1.init);
  std::vector<unsigned char> data(initStr.begin(), initStr.end());
  Transaction tx0(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE, 5000,
                  t1.code, data);
  TransactionReceipt tr0;
  AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
  Account* account = AccountStore::GetInstance().GetAccount(contrAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account != nullptr,
                      "Error with creation of contract account");
  nonce++;

  /* ------------------------------------------------------------------- */

  // Execute message_1, the Donate transaction.
  std::vector<unsigned char> dataDonate;
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t1.message, dataDonate);

  Transaction tx1(1, nonce, contrAddr, donor1, amount, PRECISION_MIN_VALUE,
                  5000, {}, dataDonate);
  TransactionReceipt tr1;
  if (AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1)) {
    nonce++;
  }

  uint128_t contrBal = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call1] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO,
              "[Call1] Donor1 balance: "
                  << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO,
              "[Call1] Donor2 balance: "
                  << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call1] Contract balance (scilla): " << contrBal);
  LOG_GENERAL(INFO, "[Call1] Contract balance (blockchain): " << oBal);
  BOOST_CHECK_MESSAGE(contrBal == oBal && contrBal == amount, "Balance mis-match after Donate");

  /* ------------------------------------------------------------------- */

  // Do another donation from donor2
  ScillaTestUtil::ScillaTest t2;
  if (!ScillaTestUtil::GetScillaTest(t2, "crowdfunding", 2)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_2.");
    return;
  }

  uint64_t bnum2 = ScillaTestUtil::GetBlockNumberFromJson(t2.blockchain);
  // Execute message_2, the Donate transaction.
  std::vector<unsigned char> dataDonate2;
  uint64_t amount2= ScillaTestUtil::PrepareMessageData(t2.message, dataDonate2);

  Transaction tx2(1, nonce, contrAddr, donor2, amount2, PRECISION_MIN_VALUE,
                  5000, {}, dataDonate2);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccounts(bnum2, 1, true, tx2, tr2)) {
    nonce++;
  }

  uint128_t contrBal2 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal2 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call2] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO,
              "[Call2] Donor1 balance: "
                  << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO,
              "[Call2] Donor2 balance: "
                  << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call2] Contract balance (scilla): " << contrBal2);
  LOG_GENERAL(INFO, "[Call2] Contract balance (blockchain): " << oBal2);
  BOOST_CHECK_MESSAGE(contrBal2 == oBal2 && contrBal2 == amount + amount2, "Balance mis-match after Donate2");

  /* ------------------------------------------------------------------- */

  // Let's try donor1 donating again, it shouldn't have an impact.
  // Execute message_3, the unsuccessful Donate transaction.
  Transaction tx3(1, nonce, contrAddr, donor1, amount, PRECISION_MIN_VALUE,
                  5000, {}, dataDonate);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx3, tr3)) {
    nonce++;
  }
  uint128_t contrBal3 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal3 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call3] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO,
              "[Call3] Donor1 balance: "
                  << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO,
              "[Call3] Donor2 balance: "
                  << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call3] Contract balance (scilla): " << contrBal3);
  LOG_GENERAL(INFO, "[Call3] Contract balance (blockchain): " << oBal3);
  BOOST_CHECK_MESSAGE(contrBal3 == contrBal2, "Balance mis-match after Donate3");

  /* ------------------------------------------------------------------- */

  // Owner tries to get fund, fails
  ScillaTestUtil::ScillaTest t4;
  if (!ScillaTestUtil::GetScillaTest(t4, "crowdfunding", 4)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_4.");
    return;
  }

  uint64_t bnum4 = ScillaTestUtil::GetBlockNumberFromJson(t4.blockchain);
  // Execute message_4, the Donate transaction.
  std::vector<unsigned char> data4;
  uint64_t amount4 = ScillaTestUtil::PrepareMessageData(t4.message, data4);

  Transaction tx4(1, nonce, contrAddr, owner, amount4, PRECISION_MIN_VALUE,
                  5000, {}, data4);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccounts(bnum4, 1, true, tx4, tr4)) {
    nonce++;
  }

  uint128_t contrBal4 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal4 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call4] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO,
              "[Call4] Donor1 balance: "
                  << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO,
              "[Call4] Donor2 balance: "
                  << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call4] Contract balance (scilla): " << contrBal4);
  LOG_GENERAL(INFO, "[Call4] Contract balance (blockchain): " << oBal4);
  BOOST_CHECK_MESSAGE(contrBal4 == contrBal3 && contrBal4 == oBal4, "Balance mis-match after GetFunds");

  /* ------------------------------------------------------------------- */
  
  // Donor1 ClaimsBack his funds. Succeeds.
  ScillaTestUtil::ScillaTest t5;
  if (!ScillaTestUtil::GetScillaTest(t5, "crowdfunding", 5)) {
    LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_5.");
    return;
  }

  uint64_t bnum5 = ScillaTestUtil::GetBlockNumberFromJson(t5.blockchain);
  // Execute message_5, the Donate transaction.
  std::vector<unsigned char> data5;
  uint64_t amount5 = ScillaTestUtil::PrepareMessageData(t5.message, data5);

  Transaction tx5(1, nonce, contrAddr, donor1, amount5, PRECISION_MIN_VALUE,
                  5000, {}, data5);
  TransactionReceipt tr5;
  if (AccountStore::GetInstance().UpdateAccounts(bnum5, 1, true, tx5, tr5)) {
    nonce++;
  }

  uint128_t contrBal5 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal5 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call5] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO,
              "[Call5] Donor1 balance: "
                  << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO,
              "[Call5] Donor2 balance: "
                  << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call5] Contract balance (scilla): " << contrBal4);
  LOG_GENERAL(INFO, "[Call5] Contract balance (blockchain): " << oBal4);
  BOOST_CHECK_MESSAGE(contrBal5 == oBal5 &&  contrBal5 == contrBal4 - amount, "Balance mis-match after GetFunds");

  /* ------------------------------------------------------------------- */

}

BOOST_AUTO_TEST_CASE(testPingPong) {

  KeyPair owner(priv1, {priv1}), ping(priv2, {priv2}), pong(priv3, {priv3});
  Address ownerAddr, pingAddr, pongAddr;
  uint64_t nonce = 0;

  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  ownerAddr = Account::GetAddressFromPublicKey(owner.second);
  AccountStore::GetInstance().AddAccount(ownerAddr, {2000000, nonce});

  pingAddr = Account::GetAddressForContract(ownerAddr, nonce);
  pongAddr = Account::GetAddressForContract(ownerAddr, nonce+1);

  LOG_GENERAL(INFO, "Ping Address: " << pingAddr << " ; PongAddress: " << pongAddr);

  /* ------------------------------------------------------------------- */

  // Deploying the contract can use data from the 0th Scilla test.
  ScillaTestUtil::ScillaTest t0ping;
  if (!ScillaTestUtil::GetScillaTest(t0ping, "ping", 0)) {
    LOG_GENERAL(WARNING, "Unable to fetch test ping_0.");
    return;
  }

  uint64_t bnumPing = ScillaTestUtil::GetBlockNumberFromJson(t0ping.blockchain);
  ScillaTestUtil::RemoveCreationBlockFromInit(t0ping.init);

  // Transaction to deploy ping.
  std::string initStrPing = JSONUtils::convertJsontoStr(t0ping.init);
  std::vector<unsigned char> dataPing(initStrPing.begin(), initStrPing.end());
  Transaction tx0(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE, 5000,
                  t0ping.code, dataPing);
  TransactionReceipt tr0;
  AccountStore::GetInstance().UpdateAccounts(bnumPing, 1, true, tx0, tr0);
  Account* accountPing = AccountStore::GetInstance().GetAccount(pingAddr);
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

  // Transaction to deploy pong.
  std::string initStrPong = JSONUtils::convertJsontoStr(t0pong.init);
  std::vector<unsigned char> dataPong(initStrPong.begin(), initStrPong.end());
  Transaction tx1(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE, 5000,
                  t0pong.code, dataPong);
  TransactionReceipt tr1;
  AccountStore::GetInstance().UpdateAccounts(bnumPong, 1, true, tx1, tr1);
  Account* accountPong = AccountStore::GetInstance().GetAccount(pongAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(accountPong != nullptr,
                      "Error with creation of pong account");
  nonce++;

  LOG_GENERAL(INFO, "Deployed ping and pong contracts.");

  /* ------------------------------------------------------------------- */

  // Set addresses of ping and pong in pong and ping respectively.
  std::vector<unsigned char> data;
  // Replace pong address in parameter of message.
  for (auto it = t0ping.message["params"].begin(); it != t0ping.message["params"].end(); it++) {
    if ((*it)["vname"] == "pongAddr")
      (*it)["value"] = "0x" + pongAddr.hex();
  }
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t0ping.message, data);
  Transaction tx2(1, nonce, pingAddr, owner, amount, PRECISION_MIN_VALUE,
                  5000, {}, data);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccounts(bnumPing, 1, true, tx2, tr2)) {
    nonce++;
  }

  // Replace ping address in paramter of message.
  for (auto it = t0pong.message["params"].begin(); it != t0pong.message["params"].end(); it++) {
    if ((*it)["vname"] == "pingAddr")
      (*it)["value"] = "0x" + pingAddr.hex();
  }
  amount = ScillaTestUtil::PrepareMessageData(t0pong.message, data);
  Transaction tx3(1, nonce, pongAddr, owner, amount, PRECISION_MIN_VALUE,
                  5000, {}, data);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccounts(bnumPong, 1, true, tx3, tr3)) {
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
  Transaction tx4(1, nonce, pingAddr, owner, amount, PRECISION_MIN_VALUE,
                  5000, {}, data);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccounts(bnumPing, 1, true, tx4, tr4)) {
    nonce++;
  }

  // Fetch the states of both ping and pong and verify "count" is 0.
  Json::Value pingState = accountPing->GetStorageJson();
  int pingCount = -1;
  for (auto it = pingState.begin(); it != pingState.end(); it++) {
    if ((*it)["vname"] == "count")
      pingCount = atoi ((*it)["value"].asCString());
  }
  Json::Value pongState = accountPing->GetStorageJson();
  int pongCount = -1;
  for (auto it = pongState.begin(); it != pongState.end(); it++) {
    if ((*it)["vname"] == "count")
      pongCount = atoi ((*it)["value"].asCString());
  }
  BOOST_CHECK_MESSAGE(pingCount == 0 && pongCount == 0,
                      "Ping / Pong did not reach count 0.");

  LOG_GENERAL(INFO, "Ping and pong bounced back to reach 0. Successful.");

  /* ------------------------------------------------------------------- */

}

BOOST_AUTO_TEST_SUITE_END()
