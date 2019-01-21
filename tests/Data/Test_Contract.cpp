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
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>

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
#include "libUtils/TimeUtils.h"

#include "ScillaTestUtil.h"

#define BOOST_TEST_MODULE contracttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(contracttest)

PrivKey priv1, priv2, priv3;

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
  std::string initStr = JSONUtils::convertJsontoStr(t1.init);
  bytes data(initStr.begin(), initStr.end());
  Transaction tx0(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 5000, t1.code, data);
  TransactionReceipt tr0;
  AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
  Account* account = AccountStore::GetInstance().GetAccount(contrAddr);
  // We should now have a new account.
  BOOST_CHECK_MESSAGE(account != nullptr,
                      "Error with creation of contract account");
  nonce++;

  /* ------------------------------------------------------------------- */

  // Execute message_1, the Donate transaction.
  bytes dataDonate;
  uint64_t amount = ScillaTestUtil::PrepareMessageData(t1.message, dataDonate);

  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor1,
                  amount, PRECISION_MIN_VALUE, 5000, {}, dataDonate);
  TransactionReceipt tr1;
  if (AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1)) {
    nonce++;
  }

  uint128_t contrBal = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call1] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO, "[Call1] Donor1 balance: "
                        << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO, "[Call1] Donor2 balance: "
                        << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call1] Contract balance (scilla): " << contrBal);
  LOG_GENERAL(INFO, "[Call1] Contract balance (blockchain): " << oBal);
  BOOST_CHECK_MESSAGE(contrBal == oBal && contrBal == amount,
                      "Balance mis-match after Donate");

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
                  amount2, PRECISION_MIN_VALUE, 5000, {}, dataDonate2);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccounts(bnum2, 1, true, tx2, tr2)) {
    nonce++;
  }

  uint128_t contrBal2 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal2 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call2] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO, "[Call2] Donor1 balance: "
                        << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO, "[Call2] Donor2 balance: "
                        << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call2] Contract balance (scilla): " << contrBal2);
  LOG_GENERAL(INFO, "[Call2] Contract balance (blockchain): " << oBal2);
  BOOST_CHECK_MESSAGE(contrBal2 == oBal2 && contrBal2 == amount + amount2,
                      "Balance mis-match after Donate2");

  /* ------------------------------------------------------------------- */

  // Let's try donor1 donating again, it shouldn't have an impact.
  // Execute message_3, the unsuccessful Donate transaction.
  Transaction tx3(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, donor1,
                  amount, PRECISION_MIN_VALUE, 5000, {}, dataDonate);
  TransactionReceipt tr3;
  if (AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx3, tr3)) {
    nonce++;
  }
  uint128_t contrBal3 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal3 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call3] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO, "[Call3] Donor1 balance: "
                        << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO, "[Call3] Donor2 balance: "
                        << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call3] Contract balance (scilla): " << contrBal3);
  LOG_GENERAL(INFO, "[Call3] Contract balance (blockchain): " << oBal3);
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
                  amount4, PRECISION_MIN_VALUE, 5000, {}, data4);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccounts(bnum4, 1, true, tx4, tr4)) {
    nonce++;
  }

  uint128_t contrBal4 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal4 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call4] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO, "[Call4] Donor1 balance: "
                        << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO, "[Call4] Donor2 balance: "
                        << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call4] Contract balance (scilla): " << contrBal4);
  LOG_GENERAL(INFO, "[Call4] Contract balance (blockchain): " << oBal4);
  BOOST_CHECK_MESSAGE(contrBal4 == contrBal3 && contrBal4 == oBal4,
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
                  amount5, PRECISION_MIN_VALUE, 5000, {}, data5);
  TransactionReceipt tr5;
  if (AccountStore::GetInstance().UpdateAccounts(bnum5, 1, true, tx5, tr5)) {
    nonce++;
  }

  uint128_t contrBal5 = AccountStore::GetInstance().GetBalance(contrAddr);
  uint128_t oBal5 = ScillaTestUtil::GetBalanceFromOutput();

  LOG_GENERAL(INFO, "[Call5] Owner balance: "
                        << AccountStore::GetInstance().GetBalance(ownerAddr));
  LOG_GENERAL(INFO, "[Call5] Donor1 balance: "
                        << AccountStore::GetInstance().GetBalance(donor1Addr));
  LOG_GENERAL(INFO, "[Call5] Donor2 balance: "
                        << AccountStore::GetInstance().GetBalance(donor2Addr));
  LOG_GENERAL(INFO, "[Call5] Contract balance (scilla): " << contrBal4);
  LOG_GENERAL(INFO, "[Call5] Contract balance (blockchain): " << oBal4);
  BOOST_CHECK_MESSAGE(contrBal5 == oBal5 && contrBal5 == contrBal4 - amount,
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
  AccountStore::GetInstance().AddAccount(ownerAddr, {2000000, nonce});

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
  std::string initStrPing = JSONUtils::convertJsontoStr(t0ping.init);
  bytes dataPing(initStrPing.begin(), initStrPing.end());
  Transaction tx0(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 5000, t0ping.code, dataPing);
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
  ScillaTestUtil::RemoveThisAddressFromInit(t0pong.init);

  // Transaction to deploy pong.
  std::string initStrPong = JSONUtils::convertJsontoStr(t0pong.init);
  bytes dataPong(initStrPong.begin(), initStrPong.end());
  Transaction tx1(DataConversion::Pack(CHAIN_ID, 1), nonce, NullAddress, owner,
                  0, PRECISION_MIN_VALUE, 5000, t0pong.code, dataPong);
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
                  amount, PRECISION_MIN_VALUE, 5000, {}, data);
  TransactionReceipt tr2;
  if (AccountStore::GetInstance().UpdateAccounts(bnumPing, 1, true, tx2, tr2)) {
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
                  amount, PRECISION_MIN_VALUE, 5000, {}, data);
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
  Transaction tx4(DataConversion::Pack(CHAIN_ID, 1), nonce, pingAddr, owner,
                  amount, PRECISION_MIN_VALUE, 5000, {}, data);
  TransactionReceipt tr4;
  if (AccountStore::GetInstance().UpdateAccounts(bnumPing, 1, true, tx4, tr4)) {
    nonce++;
  }

  // Fetch the states of both ping and pong and verify "count" is 0.
  Json::Value pingState = accountPing->GetStateJson(true);
  int pingCount = -1;
  for (auto& it : pingState) {
    if (it["vname"] == "count") {
      pingCount = atoi(it["value"].asCString());
    }
  }
  Json::Value pongState = accountPing->GetStateJson(true);
  int pongCount = -1;
  for (auto& it : pongState) {
    if (it["vname"] == "count") {
      pongCount = atoi(it["value"].asCString());
    }
  }
  BOOST_CHECK_MESSAGE(pingCount == 0 && pongCount == 0,
                      "Ping / Pong did not reach count 0.");

  LOG_GENERAL(INFO, "Ping and pong bounced back to reach 0. Successful.");

  /* ------------------------------------------------------------------- */
}

BOOST_AUTO_TEST_CASE(testStoragePerf) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  PairOfKey ownerKeyPair(priv1, {priv1});
  Address ownerAddr = Account::GetAddressFromPublicKey(ownerKeyPair.second);
  const uint128_t bal{std::numeric_limits<uint128_t>::max()};
  uint64_t nonce = 0;
  const unsigned int numDeployments = 10000;
  const unsigned int numMapEntries = 1000;

  ofstream report;
  report.open("perf_report.csv");
  report << "deployment_microsec,deployment_gas,invoke_microsec,invoke_gas\n";

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();
  AccountStore::GetInstance().AddAccount(ownerAddr, {bal, nonce});

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
    std::string initStr = JSONUtils::convertJsontoStr(t2.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, nonce, NullAddress, ownerKeyPair, 0, PRECISION_MIN_VALUE,
                    500000, t2.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    nonce++;

    Account* account = AccountStore::GetInstance().GetAccount(contractAddr);

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

    std::vector<Contract::StateEntry> state_entries;
    // save the state
    for (auto& s : t2.state) {
      // skip _balance
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      std::string vname = s["vname"].asString();
      std::string type = s["type"].asString();
      std::string value = s["value"].isString()
                              ? s["value"].asString()
                              : JSONUtils::convertJsontoStr(s["value"]);

      state_entries.push_back(std::make_tuple(vname, true, type, value));
    }

    account->SetStorage(state_entries, true);

    bytes dataTransfer;
    uint64_t amount =
        ScillaTestUtil::PrepareMessageData(t2.message, dataTransfer);

    Transaction tx1(1, nonce, contractAddr, ownerKeyPair, amount,
                    PRECISION_MIN_VALUE, 500000, {}, dataTransfer);
    TransactionReceipt tr1;

    auto startTimeCall = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1);
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

  const unsigned int numHodlers[] = {100000, 200000, 300000, 400000, 500000};

  for (auto hodlers : numHodlers) {
    AccountStore::GetInstance().Init();

    const uint128_t bal{std::numeric_limits<uint128_t>::max()};

    ownerAddr = Account::GetAddressFromPublicKey(owner.second);
    AccountStore::GetInstance().AddAccount(ownerAddr, {bal, nonce});

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
    std::string initStr = JSONUtils::convertJsontoStr(t2.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE,
                    500000, t2.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    Account* account = AccountStore::GetInstance().GetAccount(contrAddr);

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

    std::vector<Contract::StateEntry> state_entries;
    // save the state
    for (auto& s : t2.state) {
      // skip _balance
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      std::string vname = s["vname"].asString();
      std::string type = s["type"].asString();
      std::string value = s["value"].isString()
                              ? s["value"].asString()
                              : JSONUtils::convertJsontoStr(s["value"]);

      state_entries.push_back(std::make_tuple(vname, true, type, value));
    }

    account->SetStorage(state_entries, true);

    // 3. Create a call to Transfer from one account to another
    bytes dataTransfer;
    uint64_t amount =
        ScillaTestUtil::PrepareMessageData(t2.message, dataTransfer);

    Transaction tx1(1, nonce, contrAddr, owner, amount, PRECISION_MIN_VALUE,
                    88888888, {}, dataTransfer);
    TransactionReceipt tr1;

    auto startTimeCall = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1);
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
  const unsigned int numHodlers[] = {100000, 200000, 300000, 400000, 500000};
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
    PairOfKey oprtr = Schnorr::GetInstance().GenKeyPair();
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
  AccountStore::GetInstance().AddAccount(ownerAddr, {bal, ownerNonce});

  senderAddr = Account::GetAddressFromPublicKey(sender.second);
  AccountStore::GetInstance().AddAccount(senderAddr, {bal, senderNonce});

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
    std::string initStr = JSONUtils::convertJsontoStr(t10.init);
    bytes data(initStr.begin(), initStr.end());
    Transaction tx0(1, ownerNonce, NullAddress, owner, 0, PRECISION_MIN_VALUE,
                    500000, t10.code, data);
    TransactionReceipt tr0;
    auto startTimeDeployment = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    Account* account = AccountStore::GetInstance().GetAccount(contrAddr);

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

    std::vector<Contract::StateEntry> state_entries;
    // save the state
    for (auto& s : t10.state) {
      // skip _balance
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      std::string vname = s["vname"].asString();
      std::string type = s["type"].asString();
      std::string value = s["value"].isString()
                              ? s["value"].asString()
                              : JSONUtils::convertJsontoStr(s["value"]);

      state_entries.push_back(std::make_tuple(vname, true, type, value));
    }

    account->SetStorage(state_entries, true);

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
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1);
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

BOOST_AUTO_TEST_CASE(testDEX) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  // 1. Bootstrap test case
  const unsigned int numHodlers[] = {100000, 200000, 300000, 400000, 500000};
  const unsigned int numOrders = 1000;
  std::string numTokensOwned = "1";

  PairOfKey ownerToken1(priv1, {priv1});
  PairOfKey ownerToken2(priv2, {priv2});
  PairOfKey ownerDex(priv3, {priv3});

  Address ownerToken1Addr, ownerToken2Addr, ownerDexAddr, token1Addr,
      token2Addr, dexAddr;

  uint64_t ownerToken1Nonce = 0;
  uint64_t ownerToken2Nonce = 0;
  uint64_t ownerDexNonce = 0;

  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT not set to run Test_Contract");
    return;
  }

  AccountStore::GetInstance().Init();

  const uint128_t bal{std::numeric_limits<uint128_t>::max()};

  ownerToken1Addr = Account::GetAddressFromPublicKey(ownerToken1.second);
  ownerToken2Addr = Account::GetAddressFromPublicKey(ownerToken2.second);
  ownerDexAddr = Account::GetAddressFromPublicKey(ownerDex.second);
  AccountStore::GetInstance().AddAccount(ownerToken1.second,
                                         {bal, ownerToken1Nonce});
  AccountStore::GetInstance().AddAccount(ownerToken2.second,
                                         {bal, ownerToken2Nonce});
  AccountStore::GetInstance().AddAccount(ownerDex.second, {bal, ownerDexNonce});

  for (auto hodlers : numHodlers) {
    LOG_GENERAL(INFO, "\n\n===START TEST ITERATION===\n\n");
    // Seller sells Token A for Token B. Buyer buys Token A with Token B.
    // Execute makeOrder with Seller's private key
    // Execute fillOrder with Buyer's private key

    // Deploy the token contracts using the 5th Scilla test case for
    // fungible-token.
    ScillaTestUtil::ScillaTest fungibleTokenT5;
    if (!ScillaTestUtil::GetScillaTest(fungibleTokenT5, "fungible-token", 5)) {
      LOG_GENERAL(WARNING, "Unable to fetch test fungible-token_5;.");
      return;
    }

    ScillaTestUtil::RemoveThisAddressFromInit(fungibleTokenT5.init);
    ScillaTestUtil::RemoveCreationBlockFromInit(fungibleTokenT5.init);
    uint64_t bnum =
        ScillaTestUtil::GetBlockNumberFromJson(fungibleTokenT5.blockchain);
    std::string initStr = JSONUtils::convertJsontoStr(fungibleTokenT5.init);

    bytes deployTokenData(initStr.begin(), initStr.end());

    // Deploy TOKEN 1
    token1Addr =
        Account::GetAddressForContract(ownerToken1Addr, ownerToken1Nonce);
    Transaction txDeployToken1(1, ownerToken1Nonce, NullAddress, ownerToken1, 0,
                               PRECISION_MIN_VALUE, 500000,
                               fungibleTokenT5.code, deployTokenData);
    TransactionReceipt trDeplyoToken1;
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txDeployToken1,
                                               trDeplyoToken1);
    Account* token1Account = AccountStore::GetInstance().GetAccount(token1Addr);
    ownerToken1Nonce++;
    BOOST_CHECK_MESSAGE(token1Account != nullptr,
                        "Error with creation of token 1 account");

    // Deploy TOKEN 2
    token2Addr =
        Account::GetAddressForContract(ownerToken2Addr, ownerToken2Nonce);
    Transaction txDeployToken2(1, ownerToken2Nonce, NullAddress, ownerToken2, 0,
                               PRECISION_MIN_VALUE, 500000,
                               fungibleTokenT5.code, deployTokenData);
    TransactionReceipt trDeployToken2;
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txDeployToken2,
                                               trDeployToken2);
    Account* token2Account = AccountStore::GetInstance().GetAccount(token2Addr);
    ownerToken2Nonce++;
    BOOST_CHECK_MESSAGE(token2Account != nullptr,
                        "Error with creation of token 2 account");

    // Insert hodlers artifically
    for (unsigned int i = 0; i < hodlers; i++) {
      bytes hodler(ACC_ADDR_SIZE);
      std::string hodlerAddr;
      RAND_bytes(hodler.data(), ACC_ADDR_SIZE);
      DataConversion::Uint8VecToHexStr(hodler, hodlerAddr);
      std::string hodlerNumTokens = "1";

      Json::Value kvPair;
      kvPair["key"] = "0x" + hodlerAddr;
      kvPair["val"] = hodlerNumTokens;

      for (auto& it : fungibleTokenT5.state) {
        if (it["vname"] == "balances") {
          // we have to artifically insert the owner here
          if (i == 0) {
            Json::Value ownerBal;
            ownerBal["key"] = "0x" + ownerToken1Addr.hex();
            ownerBal["val"] = "88888888";
            it["value"][i] = ownerBal;
            continue;
          }

          if (i == 1) {
            Json::Value ownerBal;
            ownerBal["key"] = "0x" + ownerToken2Addr.hex();
            ownerBal["val"] = "88888888";
            it["value"][i] = ownerBal;
            continue;
          }

          it["value"][i] = kvPair;
        }
      }
    }

    // save the state
    std::vector<Contract::StateEntry> token_state_entries;
    for (auto& s : fungibleTokenT5.state) {
      // skip _balance
      if (s["vname"].asString() == "_balance") {
        continue;
      }

      std::string vname = s["vname"].asString();
      std::string type = s["type"].asString();
      std::string value = s["value"].isString()
                              ? s["value"].asString()
                              : JSONUtils::convertJsontoStr(s["value"]);

      token_state_entries.push_back(std::make_tuple(vname, true, type, value));
    }

    token1Account->SetStorage(token_state_entries, true);
    token2Account->SetStorage(token_state_entries, true);

    // Deploy DEX
    // Deploy the DEX contract with the 0th test case, but use custom messages
    // for makeOrder/fillOrder.
    ScillaTestUtil::ScillaTest dexT1;
    if (!ScillaTestUtil::GetScillaTest(dexT1, "simple-dex", 1)) {
      LOG_GENERAL(WARNING, "Unable to fetch test simple-dex_1.");
      return;
    }

    // remove _creation_block (automatic insertion later).
    ScillaTestUtil::RemoveThisAddressFromInit(dexT1.init);
    ScillaTestUtil::RemoveCreationBlockFromInit(dexT1.init);
    for (auto& p : dexT1.init) {
      if (p["vname"].asString() == "contractOwner") {
        p["value"] = "0x" + ownerDexAddr.hex();
        break;
      }
    }

    uint64_t dexBnum = ScillaTestUtil::GetBlockNumberFromJson(dexT1.blockchain);
    std::string dexInitStr = JSONUtils::convertJsontoStr(dexT1.init);
    bytes deployDexData(dexInitStr.begin(), dexInitStr.end());

    dexAddr = Account::GetAddressForContract(ownerDexAddr, ownerDexNonce);
    Transaction txDeployDex(1, ownerDexNonce, NullAddress, ownerDex, 0,
                            PRECISION_MIN_VALUE, 500000, dexT1.code,
                            deployDexData);
    TransactionReceipt trDeployDex;
    auto startTimeDeployment = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(dexBnum, 1, true, txDeployDex,
                                               trDeployDex);
    auto timeElapsedDeployment = r_timer_end(startTimeDeployment);
    Account* dexAccount = AccountStore::GetInstance().GetAccount(dexAddr);
    BOOST_CHECK_MESSAGE(dexAccount != nullptr,
                        "Error with creation of dex account");
    LOG_GENERAL(INFO, "\n\n=== Deployed DEX ===\n\n");
    LOG_GENERAL(INFO, "Contract size = "
                          << ScillaTestUtil::GetFileSize("input.scilla"));
    LOG_GENERAL(INFO, "Gas used (deployment) = " << trDeployDex.GetCumGas());
    LOG_GENERAL(INFO, "UpdateAccounts (deployment) (micro) = "
                          << timeElapsedDeployment);
    ownerDexNonce++;

    // Artificially populate the order book
    Json::Value orderBook;
    Json::Value orderInfo;
    std::vector<Contract::StateEntry> dex_state_entries;
    for (unsigned int i = 0; i < numOrders; i++) {
      Json::Value info;

      bytes sender(ACC_ADDR_SIZE);
      std::string sender_str;
      DataConversion::Uint8VecToHexStr(sender, sender_str);
      RAND_bytes(sender.data(), ACC_ADDR_SIZE);

      bytes orderId(COMMON_HASH_SIZE);
      std::string orderId_str;
      DataConversion::Uint8VecToHexStr(orderId, orderId_str);
      RAND_bytes(orderId.data(), COMMON_HASH_SIZE);
      std::string orderIdHex = "0x" + orderId_str;

      info["key"] = orderIdHex;
      info["val"]["constructor"] = "Pair";
      info["val"]["argtypes"][0] = "ByStr20";
      info["val"]["argtypes"][1] = "BNum";
      info["val"]["arguments"][0] = "0x" + sender_str;
      info["val"]["arguments"][1] = "168";
      orderInfo[i] = info;

      // Token1
      Json::Value sell;
      sell["constructor"] = "Pair";
      sell["argtypes"][0] = "ByStr20";
      sell["argtypes"][1] = "Uint128";
      sell["arguments"][0] = "0x" + token1Addr.hex();
      sell["arguments"][1] = "1";

      // Token 2
      Json::Value buy;
      buy["constructor"] = "Pair";
      buy["argtypes"][0] = "ByStr20";
      buy["argtypes"][1] = "Uint128";
      buy["arguments"][0] = "0x" + token2Addr.hex();
      buy["arguments"][1] = "1";

      Json::Value order;
      order["key"] = orderIdHex;
      order["val"]["constructor"] = "Pair";
      order["val"]["argtypes"][0] = "Pair (ByStr20) (Uint128)";
      order["val"]["argtypes"][1] = "Pair (ByStr20) (Uint128)";
      order["val"]["arguments"][0] = sell;
      order["val"]["arguments"][1] = buy;

      orderBook[i] = order;
    }

    // Update the state directly.
    dex_state_entries.push_back(
        std::make_tuple("orderbook", true,
                        "Map (ByStr32) (Pair (Pair (ByStr20) (Uint128)) "
                        "(Pair (ByStr20) (Uint128)))",
                        JSONUtils::convertJsontoStr(orderBook)));
    dex_state_entries.push_back(std::make_tuple(
        "orderInfo", true, "Map (ByStr32) (Pair (ByStr20) (BNum))",
        JSONUtils::convertJsontoStr(orderInfo)));
    dexAccount->SetStorage(dex_state_entries, true);

    // Approve DEX on Token A and Token B respectively
    Json::Value dataApprove = fungibleTokenT5.message;
    dataApprove["params"][0]["value"] = "0x" + dexAddr.hex();
    bytes dataApproveBytes;
    ScillaTestUtil::PrepareMessageData(dataApprove, dataApproveBytes);

    // Execute Approve on Token A in favour of DEX
    Transaction txApproveToken1(1, ownerToken1Nonce, token1Addr, ownerToken1, 0,
                                PRECISION_MIN_VALUE, 88888888, {},
                                dataApproveBytes);
    TransactionReceipt trApproveToken1;

    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txApproveToken1,
                                               trApproveToken1);
    ownerToken1Nonce++;

    // Execute Approve on Token B in favour of DEX
    Transaction txApproveToken2(1, ownerToken2Nonce, token2Addr, ownerToken2, 0,
                                PRECISION_MIN_VALUE, 88888888, {},
                                dataApproveBytes);
    TransactionReceipt trApproveToken2;

    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txApproveToken2,
                                               trApproveToken2);
    ownerToken2Nonce++;

    // Execute updateAddress as dexOwner
    Json::Value dataUpdateAddress = dexT1.message;
    dataUpdateAddress["params"][0]["value"] = "0x" + ownerDexAddr.hex();

    bytes dataUpdateAddressBytes;
    ScillaTestUtil::PrepareMessageData(dataUpdateAddress,
                                       dataUpdateAddressBytes);

    Transaction txUpdateAddress(1, ownerDexNonce, dexAddr, ownerDex, 0,
                                PRECISION_MIN_VALUE, 88888888, {},
                                dataUpdateAddressBytes);
    TransactionReceipt trUpdateAddress;

    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txUpdateAddress,
                                               trUpdateAddress);
    ownerDexNonce++;

    // Execute makeOrder as ownerToken1
    Json::Value dataMakeOrder = dexT1.message;
    Json::Value dataMakeOrderParams;

    dataMakeOrder["_tag"] = "makeOrder";

    dataMakeOrderParams[0]["vname"] = "tokenA";
    dataMakeOrderParams[0]["type"] = "ByStr20";
    dataMakeOrderParams[0]["value"] = "0x" + token1Addr.hex();

    dataMakeOrderParams[1]["vname"] = "tokenB";
    dataMakeOrderParams[1]["type"] = "ByStr20";
    dataMakeOrderParams[1]["value"] = "0x" + token2Addr.hex();

    dataMakeOrderParams[2]["vname"] = "valueA";
    dataMakeOrderParams[2]["type"] = "Uint128";
    dataMakeOrderParams[2]["value"] = "168";

    dataMakeOrderParams[3]["vname"] = "valueB";
    dataMakeOrderParams[3]["type"] = "Uint128";
    dataMakeOrderParams[3]["value"] = "168";

    dataMakeOrderParams[4]["vname"] = "expirationBlock";
    dataMakeOrderParams[4]["type"] = "BNum";
    dataMakeOrderParams[4]["value"] = "200";

    dataMakeOrder["params"] = dataMakeOrderParams;

    bytes dataMakeOrderBytes;
    ScillaTestUtil::PrepareMessageData(dataMakeOrder, dataMakeOrderBytes);

    Transaction txMakeOrder(1, ownerToken1Nonce, dexAddr, ownerToken1, 0,
                            PRECISION_MIN_VALUE, 88888888, {},
                            dataMakeOrderBytes);
    TransactionReceipt trMakeOrder;

    LOG_GENERAL(INFO, "\n\n=== EXECUTING makeOrder ===\n\n");
    auto startMakeOrder = r_timer_start();
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, txMakeOrder,
                                               trMakeOrder);
    auto timeMakeOrder = r_timer_end(startMakeOrder);
    ownerToken1Nonce++;

    // At this point:
    // - sender's balance should have decreased, because the DEX contract will
    // have taken custody of the token.
    // - there should be an additional order in simple-dex.
    Json::Value token1State = token1Account->GetStateJson(true);
    for (auto& s : token1State) {
      if (s["vname"] == "balances") {
        for (auto& hodl : s["value"]) {
          if (hodl["key"] == "0x" + ownerToken1Addr.hex()) {
            BOOST_CHECK_MESSAGE(hodl["val"] == "88888720",
                                "Owner 1's balance did not decrease!");
            LOG_GENERAL(INFO, "Owner 1 balance = " << hodl["val"]);
          }
        }
      }
    }

    Json::Value logs = trMakeOrder.GetJsonValue();
    std::string id = logs["event_logs"][0]["params"][0]["value"].asString();
    LOG_GENERAL(INFO, "New order ID = " << id);

    Json::Value simpleDexState = dexAccount->GetStateJson(true);
    bool hasNewOrder = false;

    for (auto& s : simpleDexState) {
      if (s["vname"] == "orderbook") {
        for (auto& ord : s["value"]) {
          if (ord["key"] == id) {
            hasNewOrder = true;
            LOG_GENERAL(INFO, "New order = " << ord["val"]);
          }
        }
      }
    }

    BOOST_CHECK_MESSAGE(hasNewOrder == true,
                        "Did not receive a new order in simple-dex!");

    LOG_GENERAL(
        INFO, "Size of output = " << ScillaTestUtil::GetFileSize("output.json"))
    LOG_GENERAL(INFO, "Size of map (Token A) = " << hodlers);
    LOG_GENERAL(INFO, "Size of map (Token B) = " << hodlers);
    LOG_GENERAL(INFO, "Receipt makeOrder = " << trMakeOrder.GetString());
    LOG_GENERAL(INFO, "Gas used (makeOrder) = " << trMakeOrder.GetCumGas());
    LOG_GENERAL(INFO, "Time elapsed (updateAccount) = " << timeMakeOrder);
    LOG_GENERAL(INFO, "\n\n=== END TEST ITERATION ===\n\n");
  }
}

BOOST_AUTO_TEST_SUITE_END()
