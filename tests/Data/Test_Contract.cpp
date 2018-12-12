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

Address toAddress;
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

KeyPair owner(priv1, {priv1}), donor1(priv2, {priv2}), donor2(priv3, {priv3});
Address ownerAddr, donor1Addr, donor2Addr, contrAddr;
uint64_t nonce = 0;

// Create Transaction to create contract
BOOST_AUTO_TEST_CASE(testCrowdfunding) {
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

  contrAddr = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "CrowdFunding Address: " << contrAddr);

  {
    // Deploying the contract can use data from the 1st Scilla test.
    ScillaTestUtil::ScillaTest t1;
    if (!ScillaTestUtil::GetScillaTest(t1, "crowdfunding", 1)) {
      LOG_GENERAL(WARNING, "Unable to fetch test crowdfunding_1.");
      return;
    }

    // Replace owner address in init.json.
    // At the same time, find the index of _creation_block
    int creation_block_index = -1;
    for (auto it = t1.init.begin(); it != t1.init.end(); it++) {
      if ((*it)["vname"] == "owner") (*it)["value"] = "0x" + ownerAddr.hex();
      if ((*it)["vname"] == "_creation_block")
        creation_block_index = it - t1.init.begin();
    }
    // Remove _creation_block from init.json as it will be inserted
    // automatically.
    if (creation_block_index >= 0) {
      Json::Value dummy;
      t1.init.removeIndex(Json::ArrayIndex(creation_block_index), &dummy);
    }

    // Get blocknumber from blockchain.json
    uint64_t bnum = 0;
    for (auto it = t1.blockchain.begin(); it != t1.blockchain.end(); it++)
      if ((*it)["vname"] == "BLOCKNUMBER")
        bnum = atoi((*it)["value"].asCString());

    // Transaction to deploy contract.
    std::string initStr = JSONUtils::convertJsontoStr(t1.init);
    std::vector<unsigned char> data(initStr.begin(), initStr.end());
    Transaction tx0(1, nonce, NullAddress, owner, 0, PRECISION_MIN_VALUE, 5000,
                    t1.code, data);
    TransactionReceipt tr0;
    AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx0, tr0);
    Account* account = AccountStore::GetInstance().GetAccount(toAddress);
    // We should now have a new account.
    BOOST_CHECK_MESSAGE(account == nullptr,
                        "Error with creation of contract account");
    nonce++;

    // Execute message_1, the Donate transaction.
    uint64_t amount = atoi(t1.message["_amount"].asCString());
    // Remove _amount and _sender as they will be automatically inserted.
    t1.message.removeMember("_amount");
    t1.message.removeMember("_sender");
    std::string msgStr = JSONUtils::convertJsontoStr(t1.message);
    std::vector<unsigned char> dataDonate(msgStr.begin(), msgStr.end());

    Transaction tx1(1, nonce, contrAddr, donor1, amount, PRECISION_MIN_VALUE,
                    5000, {}, dataDonate);
    TransactionReceipt tr1;
    if (AccountStore::GetInstance().UpdateAccounts(bnum, 1, true, tx1, tr1)) {
      nonce++;
    }

    Json::Value iOutput;
    if (!ScillaTestUtil::ParseJsonFile(iOutput, OUTPUT_JSON)) {
      LOG_GENERAL(WARNING, "Unable to parse output of interpreter.");
      return;
    }

    uint128_t contrBal = AccountStore::GetInstance().GetBalance(contrAddr);
    // Get balance as given by the interpreter.
    uint128_t oBal = 0;
    Json::Value states = iOutput["states"];
    for (auto it = states.begin(); it != states.end(); it++) {
      if ((*it)["vname"] == "_balance") oBal = atoi((*it)["value"].asCString());
    }

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
    BOOST_CHECK_MESSAGE(contrBal == oBal, "Balance mis-match after Donate");

    // TODO: Do the other tests.
  }
}

BOOST_AUTO_TEST_SUITE_END()
