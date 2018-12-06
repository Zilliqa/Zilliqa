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
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Address.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

BOOST_AUTO_TEST_SUITE(accountstoretest)

BOOST_AUTO_TEST_CASE(commitAndRollback) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  AccountStore::GetInstance().Init();

  // Check account store is initially empty
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetStateRootHash() == dev::EmptyTrie,
      "Wrong root: AccountStore initial state is wrong"
      "(root hash != empty hash)!");

  // Populate the account store
  PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);
  Account account1(1, 11);
  AccountStore::GetInstance().AddAccount(address1, account1);

  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();
  BOOST_CHECK_MESSAGE(
      root1 != dev::EmptyTrie,
      "Wrong root: Call to AddAccount(addr, account) did not change"
      " root!");

  // Commit to persistent storage
  // Check that root hash is unchanged
  AccountStore::GetInstance().MoveUpdatesToDisk();
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetStateRootHash() == root1,
      "Wrong root: Call to MoveUpdatesToDisk() has changed the root "
      "hash!");

  // Retrieve entry from persistent storage
  // Check that account store is restored
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
                      "Wrong balance: Call to GetBalance(addr) failed to "
                      "retrieve account at addr!");
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetStateRootHash() == root1,
      "Wrong root: Call to GetBalance() failed to restore AccountStore!");

  // Update entry contents
  // Check that balance and root hash have changed
  AccountStore::GetInstance().IncreaseBalance(address1, 9);
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 10,
                      "Wrong balance: Call to IncreaseBalance(addr) failed "
                      "to update account at addr!");
  AccountStore::GetInstance().UpdateStateTrieAll();
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetStateRootHash() != root1,
      "Wrong root: Call to GetBalance() failed to update the root hash!");

  // Roll back
  // Check that balance and root hash are also rolled back
  AccountStore::GetInstance().DiscardUnsavedUpdates();
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
                      "Wrong balance: Call to DiscardUnsavedUpdates() failed "
                      "to revert account at addr!");
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetStateRootHash() == root1,
                      "Wrong root: Call to DiscardUnsavedUpdates() failed to "
                      "revert the root hash!");

  // Update entry contents
  AccountStore::GetInstance().IncreaseBalance(address1, 9);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root2 = AccountStore::GetInstance().GetStateRootHash();

  // Commit changes to disk
  // Check that balance is updated and root hash is unchanged
  AccountStore::GetInstance().MoveUpdatesToDisk();
  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 10,
                      "Wrong balance: Call to MoveUpdatesToDisk() has "
                      "changed the balance at addr!");
  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().GetStateRootHash() == root2,
      "Wrong root: Call to MoveUpdatesToDisk() has changed the root hash!");
}

BOOST_AUTO_TEST_CASE(varyingOrderOfAddAccountCalls) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().IsAccountExist(address1) == false,
      "IsAccountExist returning true wrongly!");

  /*
  ============================================================
  */

  Account account1(1, 11);
  AccountStore::GetInstance().AddAccount(address1, account1);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(
      AccountStore::GetInstance().IsAccountExist(address1) == true,
      "IsAccountExist returning true wrongly!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
                      "GetBalance returning wrong balance!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
                      "GetBalance returning wrong nonce!");

  /*
  ============================================================
  */

  Account account2(2, 22);
  AccountStore::GetInstance().AddAccount(address1, account2);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root2 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
                      "Wrong balance: Call to AddAccount(addr, account) on "
                      "already existing address-account worked!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
                      "Wrong nonce: Call to AddAccount(addr, account) on"
                      "already existing address-account worked!");

  BOOST_CHECK_MESSAGE(root1 == root2,
                      "Wrong root: Call to AddAccount(addr, account) on "
                      "already existing address-account changed root!");

  /*
  ============================================================
  */

  AccountStore::GetInstance().AddAccount(pubKey1, account2);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root3 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 1,
                      "Wrong balance: Call to AddAccount(pubKey, account) on "
                      "already existing address-account worked!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 11,
                      "Wrong nonce: Call to AddAccount(pubKey, account) on "
                      "already existing address-account worked!");

  BOOST_CHECK_MESSAGE(root1 == root3,
                      "Wrong root: Call to AddAccount(pubKey, account) on "
                      "already existing address-account changed root!");

  /*
  ============================================================
  */

  PubKey pubKey2 = Schnorr::GetInstance().GenKeyPair().second;
  Address address2 = Account::GetAddressFromPublicKey(pubKey2);

  AccountStore::GetInstance().AddAccount(pubKey2, account2);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root4 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address2) == 2,
                      "Second unqiue call to AddAccount(pubKey, account) "
                      "followed by GetBalance not working!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address2) == 22,
                      "Second unqiue call to AddAccount(pubKey, account) "
                      "followed by GetNonce not working!");

  BOOST_CHECK_MESSAGE(
      root1 != root4,
      "Wrong root: Call to AddAccount(pubKey, account) didn't change root!");

  /*
  ============================================================
  */

  PubKey pubKey3 = Schnorr::GetInstance().GenKeyPair().second;
  Address address3 = Account::GetAddressFromPublicKey(pubKey3);

  Account account3(3, 33);
  AccountStore::GetInstance().AddAccount(address3, account3);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root5 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address3) == 3,
                      "Third unqiue call to AddAccount(addr, account) "
                      "followed by GetBalance not working!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address3) == 33,
                      "Third unqiue call to AddAccount(addr, account) "
                      "followed by GetNonce not working!");

  BOOST_CHECK_MESSAGE(
      root1 != root4 && root4 != root5,
      "Wrong root: Call to AddAccount(addr, account) didn't change root!");
}

BOOST_AUTO_TEST_CASE(increaseBalance) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  Account account1(21, 211);
  AccountStore::GetInstance().AddAccount(address1, account1);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root1 = AccountStore::GetInstance().GetStateRootHash();

  AccountStore::GetInstance().IncreaseBalance(address1, 9);
  AccountStore::GetInstance().UpdateStateTrieAll();
  auto root2 = AccountStore::GetInstance().GetStateRootHash();

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) == 30,
                      "IncreaseBalance didn't increase balance rightly!");

  BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) == 211,
                      "IncreaseBalance changed nonce!");

  BOOST_CHECK_MESSAGE(root1 != root2, "IncreaseBalance didn't change root!");
}

//   BOOST_AUTO_TEST_CASE(decreaseBalance)
//   {
//       INIT_STDOUT_LOGGER();

//       LOG_MARKER();

//       PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
//       Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//       Account account1(21, 211);
//       AccountStore::GetInstance().AddAccount(address1, account1);

//       auto root1 = AccountStore::GetInstance().GetStateRootHash();

//       AccountStore::GetInstance().DecreaseBalance(address1, 1);

//       auto root2 = AccountStore::GetInstance().GetStateRootHash();

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) ==
//       20,
//                           "DecreaseBalance didn't decrease balance! ");

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) ==
//       211,
//                           "DecreaseBalance changed nonce!");

//       BOOST_CHECK_MESSAGE(root1 != root2, "DecreaseBalance didn't change
//       root!");

//       /*
//       ============================================================
//       */

//       AccountStore::GetInstance().DecreaseBalance(address1, 21);

//       auto root3 = AccountStore::GetInstance().GetStateRootHash();

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) ==
//       20,
//                           "DecreaseBalance succeeded even below 0! ");

//       BOOST_CHECK_MESSAGE(
//           root2 == root3,
//           "DecreaseBalance changed root even though result goes below 0!");
//   }

//   BOOST_AUTO_TEST_CASE(transferBalance)
//   {
//       INIT_STDOUT_LOGGER();

//       LOG_MARKER();

//       PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
//       Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//       Account account1(21, 211);
//       AccountStore::GetInstance().AddAccount(address1, account1);

//       PubKey pubKey2 = Schnorr::GetInstance().GenKeyPair().second;
//       Address address2 = Account::GetAddressFromPublicKey(pubKey2);

//       Account account2(0, 1);
//       AccountStore::GetInstance().AddAccount(address2, account2);

//       AccountStore::GetInstance().TransferBalance(address1, address2, 1);

//       BOOST_CHECK_MESSAGE(
//           AccountStore::GetInstance().GetBalance(address1) == 20,
//           "DecreaseBalance didn't decrease balance in call to
//           TransferBalance!");

//       BOOST_CHECK_MESSAGE(
//           AccountStore::GetInstance().GetBalance(address2) == 1,
//           "IncreaseBalance didn't increase balance in call to
//           TransferBalance!");

//       /*
//       ============================================================
//       */

//       AccountStore::GetInstance().TransferBalance(address1, address2, 21);

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) ==
//       20,
//                           "DecreaseBalance decreased balance in call to "
//                           "TransferBalance even though balance<delta!");

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address2) ==
//       1,
//                           "IncreaseBalance increased balance in call to "
//                           "TransferBalance even though balance<delta!");
//   }

//   BOOST_AUTO_TEST_CASE(increaseNonce)
//   {
//       INIT_STDOUT_LOGGER();

//       LOG_MARKER();

//       PubKey pubKey1 = Schnorr::GetInstance().GenKeyPair().second;
//       Address address1 = Account::GetAddressFromPublicKey(pubKey1);

//       Account account1(21, 211);
//       AccountStore::GetInstance().AddAccount(address1, account1);

//       auto root1 = AccountStore::GetInstance().GetStateRootHash();

//       AccountStore::GetInstance().IncreaseNonce(address1);

//       auto root2 = AccountStore::GetInstance().GetStateRootHash();

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetBalance(address1) ==
//       21,
//                           "IncreaseNonce changed balance! ");

//       BOOST_CHECK_MESSAGE(AccountStore::GetInstance().GetNonce(address1) ==
//       212,
//                           "IncreaseNonce didn't change nonce rightly!");

//       BOOST_CHECK_MESSAGE(root1 != root2, "IncreaseNonce didn't change
//       root!");
// }

BOOST_AUTO_TEST_SUITE_END()
