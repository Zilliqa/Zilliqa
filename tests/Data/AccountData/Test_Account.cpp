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
#include "libData/AccountData/Account.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE accounttest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;
using namespace Contract;

BOOST_AUTO_TEST_SUITE(accounttest)

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
  BOOST_CHECK(acc1.IncreaseNonce());
  BOOST_CHECK(acc1.IncreaseNonceBy(nonce_incr));
  BOOST_CHECK_EQUAL(nonce + nonce_incr + 1, acc1.GetNonce());

  nonce = TestUtils::DistUint64();
  acc1.SetNonce(nonce);
  BOOST_CHECK_EQUAL(nonce, acc1.GetNonce());
}

// BOOST_AUTO_TEST_CASE(testSerialize) {
//   uint128_t CURRENT_BALANCE = TestUtils::DistUint128();
//   PubKey pubKey1 = Schnorr::GenKeyPair().second;
//   Address addr1 = Account::GetAddressFromPublicKey(pubKey1);
//
//   Account acc1(CURRENT_BALANCE, 0);
//   acc1.SetAddress(addr1);
//   bytes message1;
//
//   bytes code = dev::h256::random().asBytes();
//   SHA2<HashType::HASH_VARIANT_256> sha2;
//   sha2.Update(code);
//   dev::h256 hash = dev::h256(sha2.Finalize());
//   acc1.SetCode(code);
//
//   BOOST_CHECK_MESSAGE(acc1.Serialize(message1, 0), "Account unserializable");
//
//   Account acc2(message1, 0);
//   // Account::Deserialize is deprecated for Contract Account,
//   // it will fail in the half way
//
//   bytes message2;
//   BOOST_CHECK_EQUAL(true, acc2.Serialize(message2, TestUtils::DistUint8()));
//
//   const uint128_t& acc2Balance = acc2.GetBalance();
//
//   BOOST_CHECK_MESSAGE(
//       CURRENT_BALANCE == acc2Balance,
//       "expected: " << CURRENT_BALANCE << " actual: " << acc2Balance << "\n");
//
//   BOOST_CHECK_MESSAGE(
//       acc2.GetCodeHash() == hash,
//       "expected: " << hash << " actual: " << acc2.GetCodeHash() << "\n");
// }

BOOST_AUTO_TEST_CASE(testOstream) {
  uint128_t balance = TestUtils::DistUint128();
  uint64_t nonce = TestUtils::DistUint64();
  dev::h256 storageRoot = dev::h256::random();
  dev::h256 codeHash = dev::h256::random();
  std::ostringstream oss_test;
  oss_test << balance << " " << nonce << " " << storageRoot << " " << codeHash;

  Account acc1(balance, nonce);
  acc1.SetNonce(nonce);
  acc1.SetStorageRoot(storageRoot);
  acc1.SetCodeHash(codeHash);

  std::ostringstream oss;
  oss << acc1;
  BOOST_CHECK_EQUAL(0, oss_test.str().compare(oss.str()));
}

BOOST_AUTO_TEST_CASE(test_balance_check_and_overflow) {
  const auto maxBalance{std::numeric_limits<uint128_t>::max()};
  Account account(maxBalance, 0, 1);

  /// balance checks
  BOOST_CHECK_EQUAL(account.GetBalance(), maxBalance);
  BOOST_CHECK(!account.IncreaseBalance(0x01));
  BOOST_CHECK(account.DecreaseBalance(0x01));
  BOOST_CHECK_EQUAL(account.GetBalance(), maxBalance - 1);
  BOOST_CHECK(
      !account.ChangeBalance(std::numeric_limits<int256_t>::max() * -1));

  try {
    account.SetBalance(std::numeric_limits<uint256_t>::max());
    BOOST_FAIL("Expected an exception, but did not receive it");
  } catch (const std::exception& e) {
    BOOST_CHECK_EQUAL(e.what(), "Balance overflow error");
  }
}

BOOST_AUTO_TEST_CASE(test_nonce_check_and_overflow) {
  Account account({}, std::numeric_limits<uint64_t>::max(), 1);

  /// nonce checks
  BOOST_CHECK_EQUAL(account.GetNonce(), 0xFFFFFFFFFFFFFFFF);
  BOOST_CHECK(!account.IncreaseNonce());
  BOOST_CHECK(!account.IncreaseNonceBy(42));
  try {
    account.SetNonce(std::numeric_limits<uint128_t>::max());
    BOOST_FAIL("Expected an exception, but did not receive it");
  } catch (const std::exception& e) {
    BOOST_CHECK_EQUAL(e.what(), "Nonce overflow error");
  }
}
BOOST_AUTO_TEST_SUITE_END()
