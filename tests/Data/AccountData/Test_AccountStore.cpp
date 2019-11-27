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
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/AccountStoreSC.h"
#include "libData/AccountData/Address.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

#include "../ScillaTestUtil.h"

BOOST_AUTO_TEST_SUITE(accountstoretest)

// BOOST_AUTO_TEST_CASE(commitAndRollback) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   AccountStore::GetInstance().Init();

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateDeltaHash() == dev::h256(),
//       "Wrong StateDeltaHash: AccountStore initial state is wrong "
//       "(state delta hash != empty hash)!");

//   // Check account store is initially empty
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateRootHash() == dev::EmptyTrie,
//       "Wrong root: AccountStore initial state is wrong "
//       "(root hash != empty hash)!");

//   // Populate the account store
//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);
//   Account account1(1, 11);
//   AccountStore::GetInstance().AddAccount(address1, account1);

//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();
//   BOOST_CHECK_MESSAGE(
//       root1 != dev::EmptyTrie,
//       "Wrong root: Call to AddAccount(addr, account) did not change"
//       " root!");

//   // Commit to persistent storage
//   // Check that root hash is unchanged
//   AccountStore::GetInstance().MoveUpdatesToDisk();
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateRootHash() == root1,
//       "Wrong root: Call to MoveUpdatesToDisk() has changed the root "
//       "hash! current: "
//           << AccountStore::GetInstance().GetStateRootHash());

//   // Retrieve entry from persistent storage
//   // Check that account store is restored
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
//                       "Wrong balance: Call to GetBalance(addr) failed to "
//                       "retrieve account at addr!");
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateRootHash() == root1,
//       "Wrong root: Call to GetBalance() failed to restore AccountStore!");

//   // Update entry contents
//   // Check that balance and root hash have changed
//   AccountStore::GetInstance().IncreaseBalance(address1, 9);
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 10,
//                       "Wrong balance: Call to IncreaseBalance(addr) failed "
//                       "to update account at addr!");
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateRootHash() != root1,
//       "Wrong root: Call to GetBalance() failed to update the root hash!");

//   // Roll back
//   // Check that balance and root hash are also rolled back
//   AccountStore::GetInstance().DiscardUnsavedUpdates();
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
//                       "Wrong balance: Call to DiscardUnsavedUpdates() failed
//                       " "to revert account at addr!");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() ==
//   root1,
//                       "Wrong root: Call to DiscardUnsavedUpdates() failed to
//                       " "revert the root hash!");

//   // Update entry contents
//   AccountStore::GetInstance().IncreaseBalance(address1, 9);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root2 = AccountStore::GetInstance().GetStateRootHash();

//   // Commit changes to disk
//   // Check that balance is updated and root hash is unchanged
//   AccountStore::GetInstance().MoveUpdatesToDisk();
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 10,
//                       "Wrong balance: Call to MoveUpdatesToDisk() has "
//                       "changed the balance at addr!");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() ==
//   root2,
//                       "Wrong root: Call to MoveUpdatesToDisk() has changed
//                       the " "root hash! current: "
//                           << AccountStore::GetInstance().GetStateRootHash());

//   AccountStore::GetInstance().RetrieveFromDisk();
// }

// BOOST_AUTO_TEST_CASE(varyingOrderOfAddAccountCalls) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().IsAccountExist(address1) == false,
//       "IsAccountExist returning true wrongly!");

//   /*
//   ============================================================
//   */

//   Account account1(1, 11);
//   AccountStore::GetInstance().AddAccount(address1, account1);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().IsAccountExist(address1) == true,
//       "IsAccountExist returning true wrongly!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
//                       "GetBalance returning wrong balance!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
//                       "GetBalance returning wrong nonce!");

//   /*
//   ============================================================
//   */

//   Account account2(2, 22);
//   AccountStore::GetInstance().AddAccount(address1, account2);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root2 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
//                       "Wrong balance: Call to AddAccount(addr, account) on "
//                       "already existing address-account worked!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
//                       "Wrong nonce: Call to AddAccount(addr, account) on "
//                       "already existing address-account worked!");

//   BOOST_CHECK_MESSAGE(root1 == root2,
//                       "Wrong root: Call to AddAccount(addr, account) on "
//                       "already existing address-account changed root!");

//   /*
//   ============================================================
//   */

//   AccountStore::GetInstance().AddAccount(pubKey1, account2);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root3 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
//                       "Wrong balance: Call to AddAccount(pubKey, account) on
//                       " "already existing address-account worked!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
//                       "Wrong nonce: Call to AddAccount(pubKey, account) on "
//                       "already existing address-account worked!");

//   BOOST_CHECK_MESSAGE(root1 == root3,
//                       "Wrong root: Call to AddAccount(pubKey, account) on "
//                       "already existing address-account changed root!");

//   /*
//   ============================================================
//   */

//   PubKey pubKey2 = Schnorr::GenKeyPair().second;
//   Address address2 = Account::GetAddressFromPublicKey(pubKey2);

//   AccountStore::GetInstance().AddAccount(pubKey2, account2);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root4 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address2) == 2,
//                       "Second unqiue call to AddAccount(pubKey, account) "
//                       "followed by GetBalance not working!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address2) == 22,
//                       "Second unqiue call to AddAccount(pubKey, account) "
//                       "followed by GetNonce not working!");

//   BOOST_CHECK_MESSAGE(
//       root1 != root4,
//       "Wrong root: Call to AddAccount(pubKey, account) didn't change root!");

//   /*
//   ============================================================
//   */

//   PubKey pubKey3 = Schnorr::GenKeyPair().second;
//   Address address3 = Account::GetAddressFromPublicKey(pubKey3);

//   Account account3(3, 33);
//   AccountStore::GetInstance().AddAccount(address3, account3);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root5 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address3) == 3,
//                       "Third unqiue call to AddAccount(addr, account) "
//                       "followed by GetBalance not working!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address3) == 33,
//                       "Third unqiue call to AddAccount(addr, account) "
//                       "followed by GetNonce not working!");

//   BOOST_CHECK_MESSAGE(
//       root1 != root4 && root4 != root5,
//       "Wrong root: Call to AddAccount(addr, account) didn't change root!");
// }

// BOOST_AUTO_TEST_CASE(increaseBalance) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccount(address1, account1);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();

//   AccountStore::GetInstance().IncreaseBalance(address1, 9);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root2 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 30,
//                       "IncreaseBalance didn't increase balance rightly!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 211,
//                       "IncreaseBalance changed nonce!");

//   BOOST_CHECK_MESSAGE(root1 != root2, "IncreaseBalance didn't change root!");
// }

// BOOST_AUTO_TEST_CASE(decreaseBalance) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccount(address1, account1);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();

//   AccountStore::GetInstance().DecreaseBalance(address1, 1);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root2 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 20,
//                       "DecreaseBalance didn't decrease balance! ");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 211,
//                       "DecreaseBalance changed nonce!");

//   BOOST_CHECK_MESSAGE(root1 != root2, "DecreaseBalance didn't change root!");

//   /*
//   ============================================================
//   */

//   AccountStore::GetInstance().DecreaseBalance(address1, 21);
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root3 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 20,
//                       "DecreaseBalance succeeded even below 0! ");

//   BOOST_CHECK_MESSAGE(
//       root2 == root3,
//       "DecreaseBalance changed root even though result goes below 0!");
// }

// BOOST_AUTO_TEST_CASE(transferBalance) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccount(address1, account1);

//   PubKey pubKey2 = Schnorr::GenKeyPair().second;
//   Address address2 = Account::GetAddressFromPublicKey(pubKey2);

//   Account account2(0, 1);
//   AccountStore::GetInstance().AddAccount(address2, account2);

//   AccountStore::GetInstance().TransferBalance(address1, address2, 1);

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address1) == 20,
//       "DecreaseBalance didn't decrease balance in call to TransferBalance!");

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address2) == 1,
//       "IncreaseBalance didn't increase balance in call to TransferBalance!");

//   /*
//   ============================================================
//   */

//   AccountStore::GetInstance().TransferBalance(address1, address2, 21);

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 20,
//                       "DecreaseBalance decreased balance in call to "
//                       "TransferBalance even though balance<delta!");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address2) == 1,
//                       "IncreaseBalance increased balance in call to "
//                       "TransferBalance even though balance<delta!");
// }

// BOOST_AUTO_TEST_CASE(increaseNonce) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccount(address1, account1);

//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK(AccountStore::GetInstance().IncreaseNonce(address1));
//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root2 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 21,
//                       "IncreaseNonce changed balance! ");

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 212,
//                       "IncreaseNonce didn't change nonce rightly!");

//   BOOST_CHECK_MESSAGE(root1 != root2, "IncreaseNonce didn't change root!");
// }

// BOOST_AUTO_TEST_CASE(serialization) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   AccountStore& as = AccountStore::GetInstance();

//   bytes src;
//   bytes dst;
//   BOOST_CHECK_EQUAL(true, as.Serialize(src, 0));
//   BOOST_CHECK_EQUAL(false, as.Deserialize(dst, 0));
//   BOOST_CHECK_EQUAL(true, as.SerializeDelta());
//   as.GetSerializedDelta(dst);
//   BOOST_CHECK_EQUAL(true, as.DeserializeDelta(dst, 0, true));
//   BOOST_CHECK_EQUAL(true, as.DeserializeDelta(dst, 0, false));
//   BOOST_CHECK_EQUAL(true, as.DeserializeDeltaTemp(dst, 0));

//   // Increase coverage
//   as.AddAccountDuringDeserialization(Address(), Account(), Account(), false,
//                                      true);
//   as.AddAccountDuringDeserialization(Address(), Account(), Account(), true,
//                                      true);
// }

// BOOST_AUTO_TEST_CASE(temporaries) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   Address addr = Address().random();
//   TransactionReceipt tr = TransactionReceipt();
//   BOOST_CHECK_EQUAL(false, AccountStore::GetInstance().UpdateAccountsTemp(
//                                TestUtils::DistUint64(),
//                                TestUtils::DistUint32(), true, Transaction(),
//                                tr));
//   BOOST_CHECK_EQUAL(false, AccountStore::GetInstance().UpdateCoinbaseTemp(
//                                ++addr, ++addr, TestUtils::DistUint128()));

//   BOOST_CHECK_EQUAL(true,
//                     AccountStore::GetInstance().GetNonceTemp(++addr) == 0);
// }

// BOOST_AUTO_TEST_CASE(commit) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   // Increase coverage
//   AccountStore::GetInstance().CommitTemp();
//   AccountStore::GetInstance().CommitTempRevertible();
//   AccountStore::GetInstance().RevertCommitTemp();
// }T

void RunTransaction(const PairOfKey& sender, unsigned int major_index,
                    unsigned int minor_index, ScillaTestUtil::ScillaTest& t,
                    const Address& contrAddr, uint64_t& nonce, bool deploy,
                    bool retrieve) {
  uint64_t bnum = ScillaTestUtil::GetBlockNumberFromJson(t.blockchain);
  std::string initStr = JSONUtils::GetInstance().convertJsontoStr(t.init);
  bytes data;
  uint64_t amount = 0;
  if (deploy) {
    data = bytes(initStr.begin(), initStr.end());
  } else {
    amount = ScillaTestUtil::PrepareMessageData(t.message, data);
  }
  bytes _bytes;
  Transaction tx(DataConversion::Pack(CHAIN_ID, 1), nonce, contrAddr, sender,
                 amount, PRECISION_MIN_VALUE, 20000, deploy ? t.code : _bytes,
                 data);
  TransactionReceipt tr;
  AccountStore::GetInstance().UpdateAccountsTemp(bnum, 1, true, tx, tr);
  nonce++;

  AccountStore::GetInstance().ProcessStorageRootUpdateBufferTemp();

  LOG_GENERAL(INFO, "tr" << std::to_string(major_index) << "_"
                         << std::to_string(minor_index)
                         << " processing finished");

  if (retrieve) {
    BOOST_CHECK_MESSAGE(AccountStore::GetInstance().SerializeDelta(),
                        "SerializeDelta failed");
    AccountStore::GetInstance().CommitTemp();
    AccountStore::GetInstance().MoveUpdatesToDisk();
    AccountStore::GetInstance().InitSoft();
    AccountStore::GetInstance().RetrieveFromDisk();
  }
}

void RunCFContract(
    bool full, [[gnu::unused]] Address& contrAddr1,
    [[gnu::unused]] Address& contrAddr2, Address& contrAddr3,
    Address& contrAddr4, [[gnu::unused]] dev::h256& codeHash1,
    [[gnu::unused]] dev::h256& codeHash2, dev::h256& codeHash3,
    dev::h256& codeHash4, [[gnu::unused]] dev::h256& contrStateHash1,
    [[gnu::unused]] dev::h256& contrStateHash2, dev::h256& contrStateHash3,
    dev::h256& contrStateHash4, [[gnu::unused]] bytes& contrCode1,
    [[gnu::unused]] bytes& contrCode2, bytes& contrCode3, bytes& contrCode4,
    [[gnu::unused]] bytes& initData1, [[gnu::unused]] bytes& initData2,
    bytes& initData3, bytes& initData4, [[gnu::unused]] Json::Value& stateJson1,
    [[gnu::unused]] Json::Value& stateJson2, Json::Value& stateJson3,
    Json::Value& stateJson4, [[gnu::unused]] uint128_t& contrBalance1,
    [[gnu::unused]] uint128_t& contrBalance2, uint128_t& contrBalance3,
    uint128_t& contrBalance4) {
  LOG_MARKER();

  uint64_t nonce = 0;
  PairOfKey owner = Schnorr::GenKeyPair();
  PubKey ownerPubKey = owner.second;
  Address ownerAddr = Account::GetAddressFromPublicKey(ownerPubKey);
  AccountStore::GetInstance().AddAccountTemp(ownerAddr, {20000000000, nonce});

  contrAddr1 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "contrAddr1: " << contrAddr1.hex());

  std::vector<ScillaTestUtil::ScillaTest> tests;

  for (unsigned int i = 1; i <= 16; i++) {
    ScillaTestUtil::ScillaTest test;
    BOOST_CHECK_MESSAGE(
        ScillaTestUtil::GetScillaTest(test, "map_corners_test", i),
        "Unable to fetch test map_corners_test_" << i << ".");
    tests.emplace_back(test);
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

  if (full) {
    for (unsigned int i = 0; i < tests.size() + 1; i++) {
      bool deploy = i == 0;
      RunTransaction(owner, 1, i, tests.at(deploy ? i : i - 1),
                     deploy ? NullAddress : contrAddr1, nonce, deploy, true);
    }

    Account* account1 = AccountStore::GetInstance().GetAccountTemp(contrAddr1);
    codeHash1 = account1->GetCodeHash();
    contrStateHash1 = account1->GetStorageRoot();
    contrCode1 = account1->GetCode();
    initData1 = account1->GetInitData();
    BOOST_CHECK_MESSAGE(account1->FetchStateJson(stateJson1),
                        "fetch stateJson for account 1 failed");
    contrBalance1 = account1->GetBalance();

    contrAddr2 = Account::GetAddressForContract(ownerAddr, nonce);
    LOG_GENERAL(INFO, "contrAddr2: " << contrAddr2.hex());

    RunTransaction(owner, 2, 0, tests.at(0), NullAddress, nonce, true, true);

    Account* account2 = AccountStore::GetInstance().GetAccountTemp(contrAddr2);
    codeHash2 = account2->GetCodeHash();
    contrStateHash2 = account2->GetStorageRoot();
    contrCode2 = account2->GetCode();
    initData2 = account2->GetInitData();
    BOOST_CHECK_MESSAGE(account2->FetchStateJson(stateJson2),
                        "fetch stateJson for account 2 failed");
    contrBalance2 = account2->GetBalance();
  }

  contrAddr3 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "contrAddr3: " << contrAddr3.hex());

  RunTransaction(owner, 3, 0, tests.at(0), NullAddress, nonce, true, false);

  Account* account3 = AccountStore::GetInstance().GetAccountTemp(contrAddr3);
  codeHash3 = account3->GetCodeHash();
  contrStateHash3 = account3->GetStorageRoot();
  contrCode3 = account3->GetCode();
  initData3 = account3->GetInitData();
  BOOST_CHECK_MESSAGE(account3->FetchStateJson(stateJson3, "", {}, true),
                      "fetch stateJson for account 3 failed");
  contrBalance3 = account3->GetBalance();

  contrAddr4 = Account::GetAddressForContract(ownerAddr, nonce);
  LOG_GENERAL(INFO, "contrAddr4: " << contrAddr4.hex());

  for (unsigned int i = 0; i < tests.size() + 1; i++) {
    bool deploy = i == 0;
    RunTransaction(owner, 4, i, tests.at(deploy ? i : i - 1),
                   deploy ? NullAddress : contrAddr4, nonce, deploy, false);
  }

  Account* account4 = AccountStore::GetInstance().GetAccountTemp(contrAddr4);
  codeHash4 = account4->GetCodeHash();
  contrStateHash4 = account4->GetStorageRoot();
  contrCode4 = account4->GetCode();
  initData4 = account4->GetInitData();
  BOOST_CHECK_MESSAGE(account4->FetchStateJson(stateJson4, "", {}, true),
                      "fetch stateJson for account 4 failed");
  contrBalance4 = account4->GetBalance();
}

void CheckRFContract(
    bool full, [[gnu::unused]] const Address& contrAddr1,
    [[gnu::unused]] const Address& contrAddr2, const Address& contrAddr3,
    const Address& contrAddr4, [[gnu::unused]] const dev::h256& codeHash1,
    [[gnu::unused]] const dev::h256& codeHash2, const dev::h256& codeHash3,
    const dev::h256& codeHash4,
    [[gnu::unused]] const dev::h256& contrStateHash1,
    [[gnu::unused]] const dev::h256& contrStateHash2,
    const dev::h256& contrStateHash3, const dev::h256& contrStateHash4,
    [[gnu::unused]] const bytes& contrCode1,
    [[gnu::unused]] const bytes& contrCode2, const bytes& contrCode3,
    const bytes& contrCode4, [[gnu::unused]] const bytes& initData1,
    [[gnu::unused]] const bytes& initData2, const bytes& initData3,
    const bytes& initData4, [[gnu::unused]] const Json::Value& stateJson1,
    [[gnu::unused]] const Json::Value& stateJson2,
    const Json::Value& stateJson3, const Json::Value& stateJson4,
    [[gnu::unused]] const uint128_t& contrBalance1,
    [[gnu::unused]] const uint128_t& contrBalance2,
    const uint128_t& contrBalance3, const uint128_t& contrBalance4) {
  LOG_MARKER();

  if (full) {
    // Check the contract with invocation
    Account* account1;
    account1 = AccountStore::GetInstance().GetAccountTemp(contrAddr1);
    BOOST_CHECK_MESSAGE(codeHash1 == account1->GetCodeHash(),
                        "CodeHash1 doesn't match");
    BOOST_CHECK_MESSAGE(contrStateHash1 == account1->GetStorageRoot(),
                        "ContrStateHash1 doesn't match");
    BOOST_CHECK_MESSAGE(contrCode1 == account1->GetCode(),
                        "ContrCode1 doesn't match");
    BOOST_CHECK_MESSAGE(initData1 == account1->GetInitData(),
                        "initData1 doesn't match");
    Json::Value t_stateJson_1;
    BOOST_CHECK_MESSAGE(account1->FetchStateJson(t_stateJson_1, "", {}, true),
                        "FetchStateJson for account 1 failed");
    BOOST_CHECK_MESSAGE(stateJson1 == t_stateJson_1,
                        "StateJson1 doesn't match");
    BOOST_CHECK_MESSAGE(contrBalance1 == account1->GetBalance(),
                        "ContrBalance doesn't match");

    // Check the contract without invocation
    Account* account2;
    account2 = AccountStore::GetInstance().GetAccountTemp(contrAddr2);
    BOOST_CHECK_MESSAGE(codeHash2 == account2->GetCodeHash(),
                        "CodeHash2 doesn't match");
    BOOST_CHECK_MESSAGE(contrStateHash2 == account2->GetStorageRoot(),
                        "ContrStateHash2 doesn't match");
    BOOST_CHECK_MESSAGE(contrCode2 == account2->GetCode(),
                        "ContrCode2 doesn't match");
    BOOST_CHECK_MESSAGE(initData2 == account2->GetInitData(),
                        "initData2 doesn't match");
    Json::Value t_stateJson_2;
    BOOST_CHECK_MESSAGE(account2->FetchStateJson(t_stateJson_2, "", {}, true),
                        "FetchStateJson for account 2 failed");
    BOOST_CHECK_MESSAGE(stateJson2 == t_stateJson_2,
                        "StateJson3 doesn't match");
    BOOST_CHECK_MESSAGE(contrBalance2 == account2->GetBalance(),
                        "ContrBalance doesn't match");
  }

  // Check the contract without invocation and retrieval
  Account* account3;
  account3 = AccountStore::GetInstance().GetAccountTemp(contrAddr3);
  BOOST_CHECK_MESSAGE(codeHash3 == account3->GetCodeHash(),
                      "CodeHash3 doesn't match");
  BOOST_CHECK_MESSAGE(contrStateHash3 == account3->GetStorageRoot(),
                      "ContrStateHash3 doesn't match");
  BOOST_CHECK_MESSAGE(contrCode3 == account3->GetCode(),
                      "ContrCode3 doesn't match");
  BOOST_CHECK_MESSAGE(initData3 == account3->GetInitData(),
                      "initData3 doesn't match");
  Json::Value t_stateJson_3;
  BOOST_CHECK_MESSAGE(account3->FetchStateJson(t_stateJson_3, "", {}, true),
                      "FetchStateJson for account 3 failed");
  BOOST_CHECK_MESSAGE(stateJson3 == t_stateJson_3, "StateJson3 doesn't match");
  BOOST_CHECK_MESSAGE(contrBalance3 == account3->GetBalance(),
                      "ContrBalance doesn't match");

  // Check the contract with invocation and without retrieval
  Account* account4;
  account4 = AccountStore::GetInstance().GetAccountTemp(contrAddr4);
  BOOST_CHECK_MESSAGE(codeHash4 == account4->GetCodeHash(),
                      "CodeHash4 doesn't match");
  BOOST_CHECK_MESSAGE(contrStateHash4 == account4->GetStorageRoot(),
                      "ContrStateHash4 doesn't match");
  BOOST_CHECK_MESSAGE(contrCode4 == account4->GetCode(),
                      "ContrCode4 doesn't match");
  BOOST_CHECK_MESSAGE(initData4 == account4->GetInitData(),
                      "initData4 doesn't match");
  Json::Value t_stateJson_4;
  BOOST_CHECK_MESSAGE(account4->FetchStateJson(t_stateJson_4, "", {}, true),
                      "FetchStateJson for account 4 failed");
  BOOST_CHECK_MESSAGE(stateJson4 == t_stateJson_4, "StateJson4 doesn't match");
  BOOST_CHECK_MESSAGE(contrBalance4 == account4->GetBalance(),
                      "ContrBalance doesn't match");
}

BOOST_AUTO_TEST_CASE(serializeAndDeserialize) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  PubKey pubKey1 = Schnorr::GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  Account account1(21, 211);
  AccountStore::GetInstance().AddAccountTemp(address1, account1);

  Address contrAddr1, contrAddr2, contrAddr3, contrAddr4;
  dev::h256 codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
      contrStateHash2, contrStateHash3, contrStateHash4;
  bytes contrCode1, contrCode2, contrCode3, contrCode4, initData1, initData2,
      initData3, initData4;
  Json::Value stateJson1, stateJson2, stateJson3, stateJson4;
  uint128_t contrBalance1, contrBalance2, contrBalance3, contrBalance4;

  if (!SCILLA_ROOT.empty()) {
    RunCFContract(true, contrAddr1, contrAddr2, contrAddr3, contrAddr4,
                  codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
                  contrStateHash2, contrStateHash3, contrStateHash4, contrCode1,
                  contrCode2, contrCode3, contrCode4, initData1, initData2,
                  initData3, initData4, stateJson1, stateJson2, stateJson3,
                  stateJson4, contrBalance1, contrBalance2, contrBalance3,
                  contrBalance4);
  }

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().SerializeDelta(),
                      "Failed to SerializeDelta");
  AccountStore::GetInstance().CommitTemp();

  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();
  bytes rawstates;
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().Serialize(rawstates, 0),
                      "AccountStore::Serialize failed");

  AccountStore::GetInstance().Init();
  auto root0 = AccountStore::GetInstance().GetStateRootHash();
  BOOST_CHECK_MESSAGE(root1 != root0,
                      "State root didn't change after AccountStore::Init");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().Deserialize(rawstates, 0),
                      "AccountStore::Deserialize failed");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 21,
                      "IncreaseNonce changed balance! ");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 211,
                      "IncreaseNonce didn't change nonce rightly!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() == root1,
                      "State root didn't match after deserialize");

  if (!SCILLA_ROOT.empty()) {
    CheckRFContract(true, contrAddr1, contrAddr2, contrAddr3, contrAddr4,
                    codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
                    contrStateHash2, contrStateHash3, contrStateHash4,
                    contrCode1, contrCode2, contrCode3, contrCode4, initData1,
                    initData2, initData3, initData4, stateJson1, stateJson2,
                    stateJson3, stateJson4, contrBalance1, contrBalance2,
                    contrBalance3, contrBalance4);
  }
}

// BOOST_AUTO_TEST_CASE(stateDelta) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   AccountStore::GetInstance().Init();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccountTemp(address1, account1);

//   Address contrAddr1, contrAddr2, contrAddr3, contrAddr4;
//   dev::h256 codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
//       contrStateHash2, contrStateHash3, contrStateHash4;
//   bytes contrCode1, contrCode2, contrCode3, contrCode4, initData1, initData2,
//       initData3, initData4;
//   Json::Value stateJson1, stateJson2, stateJson3, stateJson4;
//   uint128_t contrBalance1, contrBalance2, contrBalance3, contrBalance4;

//   if (!SCILLA_ROOT.empty()) {
//     RunCFContract(contrAddr1, contrAddr2, contrAddr3, contrAddr4, codeHash1,
//                   codeHash2, codeHash3, codeHash4, contrStateHash1,
//                   contrStateHash2, contrStateHash3, contrStateHash4,
//                   contrCode1, contrCode2, contrCode3, contrCode4, initData1,
//                   initData2, initData3, initData4, stateJson1, stateJson2,
//                   stateJson3, stateJson4, contrBalance1, contrBalance2,
//                   contrBalance3, contrBalance4);
//   }

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().SerializeDelta(),
//                       "SerializeDelta failed");
//   auto statehash = AccountStore::GetInstance().GetStateDeltaHash();
//   BOOST_CHECK_MESSAGE(statehash != dev::h256(), "StateDeltaHash is null");

//   bytes rawdelta;
//   AccountStore::GetInstance().GetSerializedDelta(rawdelta);

//   AccountStore::GetInstance().InitTemp();
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().DeserializeDeltaTemp(rawdelta, 0),
//       "AccountStore::DeserializeDeltaTemp failed");
//   AccountStore::GetInstance().SerializeDelta();
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetStateDeltaHash() == statehash,
//       "StateDeltaHash after DeserializeDeltaTemp doesn't match original");

//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address1) == 0,
//       "address1 in AccountStore has balance before deserializing delta");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().DeserializeDelta(rawdelta,
//   0),
//                       "AccountStore::DeserializeDelta failed");
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address1) == 21,
//       "address1 in AccountStore has no balance after deserializing delta");

//   if (!SCILLA_ROOT.empty()) {
//     CheckRFContract(contrAddr1, contrAddr2, contrAddr3, contrAddr4,
//     codeHash1,
//                     codeHash2, codeHash3, codeHash4, contrStateHash1,
//                     contrStateHash2, contrStateHash3, contrStateHash4,
//                     contrCode1, contrCode2, contrCode3, contrCode4,
//                     initData1, initData2, initData3, initData4, stateJson1,
//                     stateJson2, stateJson3, stateJson4, contrBalance1,
//                     contrBalance2, contrBalance3, contrBalance4);
//   }
// }

BOOST_AUTO_TEST_CASE(commitRevertible) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  PubKey pubKey1 = Schnorr::GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  Account account1(21, 211);
  AccountStore::GetInstance().AddAccountTemp(address1, account1);

  Address contrAddr1, contrAddr2, contrAddr3, contrAddr4;
  dev::h256 codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
      contrStateHash2, contrStateHash3, contrStateHash4;
  bytes contrCode1, contrCode2, contrCode3, contrCode4, initData1, initData2,
      initData3, initData4;
  Json::Value stateJson1, stateJson2, stateJson3, stateJson4;
  uint128_t contrBalance1, contrBalance2, contrBalance3, contrBalance4;

  if (!SCILLA_ROOT.empty()) {
    RunCFContract(false, contrAddr1, contrAddr2, contrAddr3, contrAddr4,
                  codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
                  contrStateHash2, contrStateHash3, contrStateHash4, contrCode1,
                  contrCode2, contrCode3, contrCode4, initData1, initData2,
                  initData3, initData4, stateJson1, stateJson2, stateJson3,
                  stateJson4, contrBalance1, contrBalance2, contrBalance3,
                  contrBalance4);
  }

  AccountStore::GetInstance().SerializeDelta();
  auto root0 = AccountStore::GetInstance().GetStateRootHash();
  AccountStore::GetInstance().CommitTempRevertible();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetBalance(address1) == 21,
      "address1 in AccountStore has no balance after CommitTempRevertible");
  BOOST_CHECK_MESSAGE(root1 != root0,
                      "StateRootHash didn't change after CommitTempRevertible");

  if (!SCILLA_ROOT.empty()) {
    CheckRFContract(false, contrAddr1, contrAddr2, contrAddr3, contrAddr4,
                    codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
                    contrStateHash2, contrStateHash3, contrStateHash4,
                    contrCode1, contrCode2, contrCode3, contrCode4, initData1,
                    initData2, initData3, initData4, stateJson1, stateJson2,
                    stateJson3, stateJson4, contrBalance1, contrBalance2,
                    contrBalance3, contrBalance4);
  }

  AccountStore::GetInstance().RevertCommitTemp();
  AccountStore::GetInstance().InitTemp();
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 0,
                      "address1 in AccountStore balance didn't revert");
  auto root2 = AccountStore::GetInstance().GetStateRootHash();
  BOOST_CHECK_MESSAGE((root2 != root1) && (root2 == root0),
                      "StateRootHash didn't revert");

  if (!SCILLA_ROOT.empty()) {
    // Check the contract with invocation
    Account* account3 = AccountStore::GetInstance().GetAccount(contrAddr3);

    BOOST_CHECK_MESSAGE(account3 == nullptr,
                        "account3 is not reverted to nullptr");

    // Check the contract without invocation
    Account* account4 = AccountStore::GetInstance().GetAccount(contrAddr4);

    BOOST_CHECK_MESSAGE(account4 == nullptr,
                        "account4 is not reverted to nullptr");

    Account t_account3;
    t_account3.SetAddress(contrAddr3);
    t_account3.SetCodeHash(codeHash3);
    AccountStore::GetInstance().AddAccountTemp(contrAddr3, t_account3);
    Json::Value t_stateJson3;
    BOOST_CHECK_MESSAGE(t_account3.FetchStateJson(t_stateJson3, "", {}, true),
                        "fetch stateJson for account 3 failed");
    BOOST_CHECK_MESSAGE(!t_stateJson3.isMember("f_s1"),
                        "state still exists for account3 after reverting!");

    Account t_account4;
    t_account4.SetAddress(contrAddr4);
    t_account4.SetCodeHash(codeHash4);
    AccountStore::GetInstance().AddAccountTemp(contrAddr4, t_account4);
    Json::Value t_stateJson4;
    BOOST_CHECK_MESSAGE(t_account4.FetchStateJson(t_stateJson4, "", {}, true),
                        "fetch stateJson for account 4 failed");
    BOOST_CHECK_MESSAGE(!t_stateJson4.isMember("f_s1"),
                        "state still exists for account4 after reverting!");
  }
}

BOOST_AUTO_TEST_CASE(commitRevertible2) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  PubKey pubKey1 = Schnorr::GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  Account account1(21, 211);
  AccountStore::GetInstance().AddAccountTemp(address1, account1);
  AccountStore::GetInstance().SerializeDelta();
  AccountStore::GetInstance().CommitTempRevertible();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();

  AccountStore::GetInstance().IncreaseBalanceTemp(address1, 1);
  AccountStore::GetInstance().SerializeDelta();
  AccountStore::GetInstance().CommitTempRevertible();
  auto root2 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 22,
                      "address1 in AccountStore balance didn't change after "
                      "CommitTempRevertible");
  BOOST_CHECK_MESSAGE(root1 != root2,
                      "StateRootHash didn't change after CommitTempRevertible");

  AccountStore::GetInstance().RevertCommitTemp();
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 21,
                      "address1 in AccountStore balance didn't revert");
  auto root3 = AccountStore::GetInstance().GetStateRootHash();
  BOOST_CHECK_MESSAGE((root2 != root3) && (root3 == root1),
                      "StateRootHash didn't revert");
}

// BOOST_AUTO_TEST_CASE(DiskOperation) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   AccountStore::GetInstance().Init();

//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//   Account account1(21, 211);
//   AccountStore::GetInstance().AddAccountTemp(address1, account1);

//   auto balance1 = AccountStore::GetInstance().GetBalance(address1);

//   Address contrAddr1, contrAddr2, contrAddr3, contrAddr4;
//   dev::h256 codeHash1, codeHash2, codeHash3, codeHash4, contrStateHash1,
//       contrStateHash2, contrStateHash3, contrStateHash4;
//   bytes contrCode1, contrCode2, contrCode3, contrCode4, initData1, initData2,
//       initData3, initData4;
//   Json::Value stateJson1, stateJson2, stateJson3, stateJson4;
//   uint128_t contrBalance1, contrBalance2, contrBalance3, contrBalance4;

//   if (!SCILLA_ROOT.empty()) {
//     RunCFContract(contrAddr1, contrAddr2, contrAddr3, contrAddr4, codeHash1,
//                   codeHash2, codeHash3, codeHash4, contrStateHash1,
//                   contrStateHash2, contrStateHash3, contrStateHash4,
//                   contrCode1, contrCode2, contrCode3, contrCode4, initData1,
//                   initData2, initData3, initData4, stateJson1, stateJson2,
//                   stateJson3, stateJson4, contrBalance1, contrBalance2,
//                   contrBalance3, contrBalance4);
//   }

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().SerializeDelta(),
//                       "Failed to SerializeDelta");
//   AccountStore::GetInstance().CommitTemp();

//   AccountStore::GetInstance().UpdateStateTrieAll();
//   auto root1 = AccountStore::GetInstance().GetStateRootHash();

//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().MoveUpdatesToDisk(),
//                       "AccountStore::MoveUpdatesToDisk failed");
//   AccountStore::GetInstance().InitSoft();
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address1) != balance1,
//       "Balance after InitSoft is still the same");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() !=
//   root1,
//                       "StateRootHash after InitSoft is still the same");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().RetrieveFromDisk(),
//                       "AccountStore::RetrieveFromDisk failed");
//   BOOST_CHECK_MESSAGE(
//       AccountStore::GetInstance().GetBalance(address1) == balance1,
//       "Balance after RetrieveFromDisk is different");
//   BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() ==
//   root1,
//                       "StateRootHash after RetrieveFromDisk is different");

//   if (!SCILLA_ROOT.empty()) {
//     CheckRFContract(contrAddr1, contrAddr2, contrAddr3, contrAddr4,
//     codeHash1,
//                     codeHash2, codeHash3, codeHash4, contrStateHash1,
//                     contrStateHash2, contrStateHash3, contrStateHash4,
//                     contrCode1, contrCode2, contrCode3, contrCode4,
//                     initData1, initData2, initData3, initData4, stateJson1,
//                     stateJson2, stateJson3, stateJson4, contrBalance1,
//                     contrBalance2, contrBalance3, contrBalance4);
//   }
// }

// BOOST_AUTO_TEST_CASE(DiskOperation2) {
//   INIT_STDOUT_LOGGER();

//   LOG_MARKER();

//   AccountStore::GetInstance().Init();

//   std::vector<std::vector<Address>> list_addresses;

//   for (auto i = 0; i < 1; i++) {
//     std::vector<Address> addresses;
//     int num_address = 10000;
//     for (auto i = 0; i < num_address; i++) {
//       PubKey pubKey = Schnorr::GenKeyPair().second;
//       Address address = Account::GetAddressFromPublicKey(pubKey);

//       Account account(21, 211);
//       AccountStore::GetInstance().AddAccountTemp(address, account);
//       addresses.push_back(address);

//       if ((i % (num_address / 10)) == 0) {
//         LOG_GENERAL(INFO, i << " Added");
//       }
//     }

//     list_addresses.push_back(addresses);

//     LOG_GENERAL(INFO, "Start UpdateStateTrieAll() 1");
//     AccountStore::GetInstance().UpdateStateTrieAll();
//     AccountStore::GetInstance().MoveUpdatesToDisk();

//     std::string output;
//     if (SysCommand::ExecuteCmdWithOutput("du -hs persistence/state", output))
//     {
//       LOG_GENERAL(INFO, "Num of AddAccount: "
//                             << list_addresses.size() * addresses.size()
//                             << ", size of state db: " << output);
//     }

//     for (auto i = 0; i < 2; i++) {
//       AccountStore::GetInstance().InitSoft();
//       AccountStore::GetInstance().RetrieveFromDisk();

//       for (const auto& addr : addresses) {
//         AccountStore::GetInstance().IncreaseBalance(addr, 1);
//       }

//       LOG_GENERAL(INFO, "Start UpdateStateTrieAll() 2_" << (i + 1));
//       AccountStore::GetInstance().UpdateStateTrieAll();
//       AccountStore::GetInstance().MoveUpdatesToDisk();

//       output.clear();
//       if (SysCommand::ExecuteCmdWithOutput("du -hs persistence/state",
//                                            output)) {
//         LOG_GENERAL(INFO,
//                     "After IncreaseBalance, size of state db: " << output);
//       }
//     }
//   }

//   AccountStore::GetInstance().InitSoft();
//   AccountStore::GetInstance().RetrieveFromDisk();

//   int num_errors = 0;

//   for (const auto& addresses : list_addresses) {
//     for (const auto& address : addresses) {
//       if (AccountStore::GetInstance().GetBalance(address) == 0) {
//         num_errors++;
//       }
//     }
//   }

//   BOOST_CHECK_EQUAL(0, num_errors);
// }

BOOST_AUTO_TEST_SUITE_END()
