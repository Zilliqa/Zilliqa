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
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libPersistence/ContractStorage.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE accounttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(accounttest)

BOOST_AUTO_TEST_CASE(testInitEmpty) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Account acc1(TestUtils::DistUint64(), 0);
  std::vector<unsigned char> data;
  acc1.InitContract(data);
<<<<<<< HEAD
=======
  acc1.SetInitData(data);
>>>>>>> 0ed1f52... Icrease coverage of libData/AccountData/Account

  Account acc2(data, 0);
  BOOST_CHECK_EQUAL(false, acc2.isContract());
  acc2.InitStorage();
}

BOOST_AUTO_TEST_CASE(testInit) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

<<<<<<< HEAD
  Account acc1(TestUtils::DistUint64(), 0);
=======
  Account acc1 = Account();
>>>>>>> 0ed1f52... Icrease coverage of libData/AccountData/Account

  uint64_t CREATEBLOCKNUM = TestUtils::DistUint64();
  acc1.SetCreateBlockNum(CREATEBLOCKNUM);
  BOOST_CHECK_EQUAL(CREATEBLOCKNUM, acc1.GetCreateBlockNum());

  std::string message =
      "[{\"vname\":\"name\",\"type\":\"sometype\",\"value\":\"somevalue\"}]";
  std::vector<unsigned  char> data(message.begin(), message.end());
  acc1.InitContract(data);

  BOOST_CHECK_EQUAL(true, data == acc1.GetInitData());

  BOOST_CHECK_EQUAL(std::to_string(CREATEBLOCKNUM), acc1.GetInitJson()[1]["value"].asString());
}

BOOST_AUTO_TEST_CASE(testStorage) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

<<<<<<< HEAD
  Account acc1(TestUtils::DistUint64(), 0);
=======
  Account acc1 = Account();
  acc1.GetStorageJson();  // Improve coverage
  acc1.RollBack();        // Improve coverage
>>>>>>> 0ed1f52... Icrease coverage of libData/AccountData/Account

  std::vector<unsigned char> code;
  acc1.SetCode(code);
  BOOST_CHECK_EQUAL(true, code == acc1.GetCode());

  LOG_GENERAL(INFO, "SetStorage 1");
  acc1.SetStorage("", "", "", false);
  dev::h256 hash = dev::h256();
  std::string rlpStr;
  acc1.SetStorage(hash, rlpStr);
  acc1.SetStorageRoot(hash);

  acc1.GetStorageRoot();
  BOOST_CHECK_EQUAL(0, acc1.GetStorage("").size());
  acc1.GetRawStorage(hash);

  size_t CODE_LEN = TestUtils::DistUint16() + 1;
  code.resize(CODE_LEN, '0');

  acc1.SetCode(code);
  BOOST_CHECK_EQUAL(true, code == acc1.GetCode());
<<<<<<< HEAD
=======
  acc1.SetStorage(hash, rlpStr);  // Improve coverage
  acc1.InitStorage();             // Improve coverage

  dev::h256 storageRoot = acc1.GetStorageRoot();
  acc1.SetStorageRoot(storageRoot);
  acc1.RollBack();  // Improve coverage
>>>>>>> 0ed1f52... Icrease coverage of libData/AccountData/Account

  uint64_t CREATEBLOCKNUM = TestUtils::DistUint64();
  acc1.SetCreateBlockNum(CREATEBLOCKNUM);
  BOOST_CHECK_EQUAL(CREATEBLOCKNUM, acc1.GetCreateBlockNum());

  acc1.SetStorage("", "", "", false);
  acc1.SetStorage(hash, rlpStr);
  acc1.SetStorageRoot(hash);

  acc1.GetStorageRoot();
  acc1.GetStorage("");
  acc1.GetRawStorage(hash);
  std::vector<dev::h256> storageKeyHashes = acc1.GetStorageKeyHashes();
  Json::Value storage = acc1.GetStorageJson();
  acc1.Commit();
  acc1.RollBack();
}

BOOST_AUTO_TEST_CASE(testBalance) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  uint128_t balance = TestUtils::DistUint32();
  Account acc1(balance, 0);

  uint32_t balance_incr = TestUtils::DistUint32();
  acc1.IncreaseBalance(balance_incr);
  uint128_t CURRENT_BALANCE = acc1.GetBalance();

  BOOST_CHECK_EQUAL(CURRENT_BALANCE, balance + balance_incr);

  BOOST_CHECK_MESSAGE(
      !acc1.DecreaseBalance(CURRENT_BALANCE + TestUtils::DistUint64()),
      "Balance can't be decreased to negative values");

  int64_t delta = (int64_t)TestUtils::DistUint64();
  if (delta > CURRENT_BALANCE && delta < 0) {
    BOOST_CHECK_MESSAGE(
        !acc1.ChangeBalance(delta),
        "Balance" << CURRENT_BALANCE << "can't be changed by delta" << delta);
  } else {
    BOOST_CHECK_MESSAGE(
        acc1.ChangeBalance(delta),
        "Balance" << CURRENT_BALANCE << "has to be changed by delta" << delta);
  }
  balance = TestUtils::DistUint128();
  acc1.SetBalance(balance);
  BOOST_CHECK_EQUAL(balance, acc1.GetBalance());
}

BOOST_AUTO_TEST_CASE(testAddresses) {
  Account acc1(0, 0);
  Address addr = acc1.GetAddressFromPublicKey(TestUtils::GenerateRandomPubKey());
  acc1.GetAddressForContract(addr, TestUtils::DistUint64());
}

BOOST_AUTO_TEST_CASE(testNonce) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  uint64_t nonce = TestUtils::DistUint16();
  uint64_t nonce_incr = TestUtils::DistUint16();

  Account acc1(0, nonce);
  acc1.IncreaseNonce();
  acc1.IncreaseNonceBy(nonce_incr);
  BOOST_CHECK_EQUAL(nonce + nonce_incr + 1, acc1.GetNonce());

  nonce = TestUtils::DistUint64();
  acc1.SetNonce(nonce);
  BOOST_CHECK_EQUAL(nonce, acc1.GetNonce());
}

BOOST_AUTO_TEST_CASE(testSerialize) {
  uint128_t CURRENT_BALANCE = TestUtils::DistUint128();
  Account acc1(CURRENT_BALANCE, 0);
  std::vector<unsigned char> message1;

  std::vector<unsigned char> code = dev::h256::random().asBytes();
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(code);
  dev::h256 hash = dev::h256(sha2.Finalize());
  acc1.SetCode(code);

  BOOST_CHECK_MESSAGE(acc1.Serialize(message1, 0), "Account unserializable");

  Account acc2(message1, 0);

  std::vector<unsigned char> message2;
  acc2.Serialize(message2, 0);

  uint128_t acc2Balance = acc2.GetBalance();

  BOOST_CHECK_MESSAGE(
      CURRENT_BALANCE == acc2Balance,
      "expected: " << CURRENT_BALANCE << " actual: " << acc2Balance << "\n");

  BOOST_CHECK_MESSAGE(
      acc2.GetCodeHash() == hash,
      "expected: " << hash << " actual: " << acc2.GetCodeHash() << "\n");

  std::vector<unsigned char> dst;
  BOOST_CHECK_EQUAL(true, acc2.SerializeDelta(dst, 0, &acc1, acc2));
  BOOST_CHECK_EQUAL(true, acc2.DeserializeDelta(dst, 0, acc1, true));
}

BOOST_AUTO_TEST_SUITE_END()
