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

#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

#define BOOST_TEST_MODULE safemath
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(safemath)

BOOST_AUTO_TEST_CASE(test_uint8_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test uint8_t start...");

  uint8_t num1 = 0x0F, num2 = 0x0B, addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint8_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = 0xFF;
  num2 = 0x01;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint8_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = 0x0F;
  num2 = 0x0B;
  uint8_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint8_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = 0xFF;
  num2 = 0xCC;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint8_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint8_t>::div(num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test uint8_t done!");
}

BOOST_AUTO_TEST_CASE(test_uint16_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test uint16_t start...");

  uint16_t num1 = 0x0FFF, num2 = 0x0BBB, addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint16_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = 0xFFFF;
  num2 = 0x0001;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint16_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = 0x0FFF;
  num2 = 0x000B;
  uint16_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint16_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = 0xFFFF;
  num2 = 0xCCCC;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint16_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint16_t>::div(num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test uint16_t done!");
}

BOOST_AUTO_TEST_CASE(test_uint32_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test uint32_t start...");

  uint32_t num1 = 0x0000FFFF, num2 = 0x00000BBB, addRes, subRes1, subRes2,
           subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint32_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = 0xFFFFFFFF;
  num2 = 0x00000001;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint32_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = 0x00FFFFFF;
  num2 = 0x000000BB;
  uint32_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint32_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = 0xFFFFFFFF;
  num2 = 0xCCCCCCCC;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint32_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint32_t>::div(num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test uint32_t done!");
}

BOOST_AUTO_TEST_CASE(test_uint64_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test uint64_t start...");

  uint64_t num1 = 0x000FFFFFFFFFFFFF, num2 = 0x00BBBBBBBBBBBBBB, addRes,
           subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint64_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = 0xFFFFFFFFFFFFFFFF;
  num2 = 0x0000000000000001;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint64_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = 0x000000FFFFFFFFFF;
  num2 = 0x000000000000BBBB;
  uint64_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint64_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = 0xFFFFFFFFFFFFFFFF;
  num2 = 0xCCCCCCCCCCCCCCCC;
  BOOST_CHECK_MESSAGE(false == SafeMath<uint64_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint64_t>::div(num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test uint64_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_uint128_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_uint128_t start...");

  boost::multiprecision::uint128_t num1("0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2("0x0000000000000000000000000000000B"), addRes, subRes1, subRes2,
      subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::add(
                                  num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::sub(
                                  addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::sub(
                                  addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint128_t>::sub(
                                   0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = boost::multiprecision::uint128_t("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint128_t("0x00000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint128_t>::add(
                                   num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = boost::multiprecision::uint128_t("0x00000000000000000FFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint128_t("0x000000000000000000000BBBBBBBBBBB");
  boost::multiprecision::uint128_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::mul(
                                  num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::div(
                                  mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint128_t>::div(
                                  mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = boost::multiprecision::uint128_t("0x000000000FFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint128_t("0x0000000000CCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint128_t>::mul(
                                   num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint128_t>::div(
                                   num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test boost_uint128_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_uint256_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_uint256_t start...");

  boost::multiprecision::uint256_t num1(
      "0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2(
          "0x000000000000000000000000000000000000000000000000000000000000000"
          "B"),
      addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::add(
                                  num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::sub(
                                  addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::sub(
                                  addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint256_t>::sub(
                                   0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = boost::multiprecision::uint256_t(
      "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint256_t(
      "0x0000000000000000000000000000000000000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint256_t>::add(
                                   num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = boost::multiprecision::uint256_t(
      "0x000000000000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint256_t(
      "0x00000000000000000000000000000000000000BBBBBBBBBBBBBBBBBBBBBBBBBB");
  boost::multiprecision::uint256_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::mul(
                                  num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::div(
                                  mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint256_t>::div(
                                  mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = boost::multiprecision::uint256_t(
      "0x000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint256_t(
      "0x0000000000CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint256_t>::mul(
                                   num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint256_t>::div(
                                   num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test boost_uint256_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_uint512_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_uint512_t start...");

  boost::multiprecision::uint512_t num1(
      "0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2(
          "0x000000000000000000000000000000000000000000000000000000000000000"
          "0000000000000000000000000000000000000000000000000000000000000000"
          "B"),
      addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::add(
                                  num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::sub(
                                  addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::sub(
                                  addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint512_t>::sub(
                                   0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = boost::multiprecision::uint512_t(
      "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint512_t(
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint512_t>::add(
                                   num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = boost::multiprecision::uint512_t(
      "0x0000000000000000000000000000000000000000000000000000FFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint512_t(
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "000000000BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
  boost::multiprecision::uint512_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::mul(
                                  num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::div(
                                  mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint512_t>::div(
                                  mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = boost::multiprecision::uint512_t(
      "0x000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint512_t(
      "0x0000000000CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
      "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint512_t>::mul(
                                   num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint512_t>::div(
                                   num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test boost_uint512_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_uint1024_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_uint1024_t start...");

  boost::multiprecision::uint1024_t num1(
      "0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2(
          "0x000000000000000000000000000000000000000000000000000000000000000"
          "00000000000000000000000000000000000000000000000000000000000000000"
          "00000000000000000000000000000000000000000000000000000000000000000"
          "00000000000000000000000000000000000000000000000000000000000000B"),
      addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::add(
                                  num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::sub(
                                  addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::sub(
                                  addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint1024_t>::sub(
                                   0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = boost::multiprecision::uint1024_t(
      "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint1024_t(
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint1024_t>::add(
                                   num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = boost::multiprecision::uint1024_t(
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "00000000000000000000000000000000000000000000000000000FFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint1024_t(
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
      "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB");
  boost::multiprecision::uint1024_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::mul(
                                  num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::div(
                                  mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<boost::multiprecision::uint1024_t>::div(
                                  mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = boost::multiprecision::uint1024_t(
      "0x00000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
      "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = boost::multiprecision::uint1024_t(
      "0x0000000000CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
      "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
      "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
      "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint1024_t>::mul(
                                   num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::uint1024_t>::div(
                                   num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test boost_uint1024_t done!");
}

BOOST_AUTO_TEST_SUITE_END()
