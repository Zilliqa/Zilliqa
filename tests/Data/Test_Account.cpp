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
  bytes data;
  acc1.InitContract(data, Address());
  acc1.SetInitData(data);

  Account acc2(data, 0);
  BOOST_CHECK_EQUAL(false, acc2.isContract());
}

BOOST_AUTO_TEST_CASE(testInit) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Account acc1 = Account();

  uint64_t CREATEBLOCKNUM = TestUtils::DistUint64();
  acc1.SetCreateBlockNum(CREATEBLOCKNUM);
  BOOST_CHECK_EQUAL(CREATEBLOCKNUM, acc1.GetCreateBlockNum());

  std::string invalidmessage = "[{\"vname\"]";
  bytes data(invalidmessage.begin(), invalidmessage.end());
  acc1.InitContract(data, Address());

  invalidmessage = "[{\"vname\":\"name\"}]";
  data = bytes(invalidmessage.begin(), invalidmessage.end());
  acc1.InitContract(data, Address());

  std::string message =
      "[{\"vname\":\"name\",\"type\":\"sometype\",\"value\":\"somevalue\"}]";
  data = bytes(message.begin(), message.end());
  acc1.InitContract(data, Address());

  BOOST_CHECK_EQUAL(true, data == acc1.GetInitData());

  BOOST_CHECK_EQUAL(std::to_string(CREATEBLOCKNUM),
                    acc1.GetInitJson()[1]["value"].asString());
}

BOOST_AUTO_TEST_CASE(testStorage) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Account acc1 = Account();
  acc1.GetStorageJson();  // Improve coverage
  acc1.RollBack();        // Improve coverage

  bytes code;
  acc1.SetCode(code);
  BOOST_CHECK_EQUAL(true, code == acc1.GetCode());

  acc1.SetStorage("", "", "", false);
  dev::h256 hash = dev::h256();
  std::string rlpStr;
  acc1.SetStorage(hash, rlpStr);
  acc1.SetStorageRoot(hash);

  BOOST_CHECK_EQUAL(0, acc1.GetStorage("").size());
  acc1.GetRawStorage(hash);

  size_t CODE_LEN = TestUtils::DistUint16() + 1;
  code.resize(CODE_LEN, '0');

  acc1.SetCode(code);
  BOOST_CHECK_EQUAL(true, code == acc1.GetCode());
  acc1.SetStorage(hash, rlpStr);  // Improve coverage
  acc1.InitStorage();             // Improve coverage
  acc1.GetStorageJson();          // Improve coverage
  dev::h256 storageRoot = acc1.GetStorageRoot();
  acc1.SetStorageRoot(storageRoot);
  acc1.RollBack();  // coverage

  uint64_t CREATEBLOCKNUM = TestUtils::DistUint64();
  acc1.SetCreateBlockNum(CREATEBLOCKNUM);
  BOOST_CHECK_EQUAL(CREATEBLOCKNUM, acc1.GetCreateBlockNum());

  acc1.SetStorage("", "", "", false);
  acc1.SetStorage(hash, rlpStr);
  acc1.SetStorageRoot(hash);

  acc1.GetStorage("");
  acc1.GetRawStorage(hash);
  std::vector<dev::h256> storageKeyHashes = acc1.GetStorageKeyHashes();
  Json::Value storage = acc1.GetStorageJson();
  acc1.Commit();
  acc1.RollBack();
  acc1.InitStorage();  // Improve coverage
}

BOOST_AUTO_TEST_CASE(testBalance) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  uint128_t BALANCE = TestUtils::DistUint32();
  Account acc1(BALANCE, 0);

  uint32_t balance_incr = TestUtils::DistUint32();
  acc1.IncreaseBalance(balance_incr);
  uint128_t CURRENT_BALANCE = acc1.GetBalance();

  BOOST_CHECK_EQUAL(CURRENT_BALANCE, BALANCE + balance_incr);

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
  BALANCE = TestUtils::DistUint128();
  if (BALANCE == 0) {
    BALANCE++;
  };
  acc1.SetBalance(BALANCE);
  acc1.DecreaseBalance(1);
  BOOST_CHECK_EQUAL(BALANCE - 1, acc1.GetBalance());
}

BOOST_AUTO_TEST_CASE(testAddresses) {
  Account acc1(0, 0);
  Address addr =
      acc1.GetAddressFromPublicKey(TestUtils::GenerateRandomPubKey());
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
  bytes message1;

  bytes code = dev::h256::random().asBytes();
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(code);
  dev::h256 hash = dev::h256(sha2.Finalize());
  acc1.SetCode(code);

  BOOST_CHECK_MESSAGE(acc1.Serialize(message1, 0), "Account unserializable");

  Account acc2(message1, 0);

  bytes message2;
  BOOST_CHECK_EQUAL(true, acc2.Serialize(message2, TestUtils::DistUint8()));

  uint128_t acc2Balance = acc2.GetBalance();

  BOOST_CHECK_MESSAGE(
      CURRENT_BALANCE == acc2Balance,
      "expected: " << CURRENT_BALANCE << " actual: " << acc2Balance << "\n");

  BOOST_CHECK_MESSAGE(
      acc2.GetCodeHash() == hash,
      "expected: " << hash << " actual: " << acc2.GetCodeHash() << "\n");

  // bytes dst;
  // BOOST_CHECK_EQUAL(true, acc2.SerializeDelta(dst, 0, &acc1, acc2));
  // BOOST_CHECK_EQUAL(true, acc2.DeserializeDelta(dst, 0, acc1, true));
}

BOOST_AUTO_TEST_SUITE_END()
