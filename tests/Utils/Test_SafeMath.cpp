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

  uint128_t num1("0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2("0x0000000000000000000000000000000B"), addRes, subRes1, subRes2,
      subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint128_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = uint128_t("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = uint128_t("0x00000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint128_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = uint128_t("0x00000000000000000FFFFFFFFFFFFFFF");
  num2 = uint128_t("0x000000000000000000000BBBBBBBBBBB");
  uint128_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint128_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = uint128_t("0x000000000FFFFFFFFFFFFFFFFFFFFFFF");
  num2 = uint128_t("0x0000000000CCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint128_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint128_t>::div(num2, 0, divRes1),
                      "Test div error-handling failed!");

  LOG_GENERAL(INFO, "Test boost_uint128_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_uint256_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_uint256_t start...");

  uint256_t num1(
      "0x0FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"),
      num2(
          "0x000000000000000000000000000000000000000000000000000000000000000"
          "B"),
      addRes, subRes1, subRes2, subRes3;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::add(num1, num2, addRes),
                      "Test add failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::sub(addRes, num1, subRes1),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::sub(addRes, num2, subRes2),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint256_t>::sub(0, num2, subRes3),
                      "Test sub failed!");
  BOOST_CHECK_MESSAGE(num1 == subRes2, "Test add/sub failed!");
  BOOST_CHECK_MESSAGE(num2 == subRes1, "Test add/sub failed!");

  num1 = uint256_t(
      "0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = uint256_t(
      "0x0000000000000000000000000000000000000000000000000000000000000001");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint256_t>::add(num1, num2, addRes),
                      "Test add overflow failed!");

  num1 = uint256_t(
      "0x000000000000000000000000000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = uint256_t(
      "0x00000000000000000000000000000000000000BBBBBBBBBBBBBBBBBBBBBBBBBB");
  uint256_t mulRes, divRes1, divRes2;
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::mul(num1, num2, mulRes),
                      "Test mul failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::div(mulRes, num1, divRes1),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(true == SafeMath<uint256_t>::div(mulRes, num2, divRes2),
                      "Test div failed!");
  BOOST_CHECK_MESSAGE(num1 == divRes2, "Test mul/div failed!");
  BOOST_CHECK_MESSAGE(num2 == divRes1, "Test mul/div failed!");

  num1 = uint256_t(
      "0x000000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
  num2 = uint256_t(
      "0x0000000000CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint256_t>::mul(num1, num2, mulRes),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<uint256_t>::div(num2, 0, divRes1),
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

BOOST_AUTO_TEST_CASE(test_int8_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test int8_t start...");

  int8_t num1 = std::numeric_limits<int8_t>::max(), num2 = 1, addRes1, addRes2,
         addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::add(num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::add(num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<int8_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::add(num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::add(num2, num1, addRes4),
                      "Test add underflow failed!");

  int8_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<int8_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::sub(num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<int8_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::sub(num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<int8_t>::min();
  num2 = -1;
  int8_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::div(num1, num2, divRes),
                      "Test div overflow failed!");

  int8_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<int8_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int8_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<int8_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int8_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int8_t>::mul(num1, num2, mulRes6),
                      "Test mul underflow failed!");

  int8_t subRes;
  BOOST_CHECK_MESSAGE(
      false ==
          SafeMath<int8_t>::sub(0, std::numeric_limits<int8_t>::min(), subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      false ==
          SafeMath<int8_t>::sub(1, std::numeric_limits<int8_t>::min(), subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      true ==
          SafeMath<int8_t>::sub(-1, std::numeric_limits<int8_t>::min(), subRes),
      "Test signed sub failed!");
  BOOST_REQUIRE(std::numeric_limits<int8_t>::max() == subRes);

  LOG_GENERAL(INFO, "Test int8_t done!");
}

BOOST_AUTO_TEST_CASE(test_int16_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test int16_t start...");

  int16_t num1 = std::numeric_limits<int16_t>::max(), num2 = 1, addRes1,
          addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::add(num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::add(num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<int16_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::add(num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::add(num2, num1, addRes4),
                      "Test add underflow failed!");

  int16_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<int16_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::sub(num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<int16_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::sub(num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<int16_t>::min();
  num2 = -1;
  int16_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::div(num1, num2, divRes),
                      "Test div overflow failed!");

  int16_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<int16_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int16_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<int16_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int16_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int16_t>::mul(num1, num2, mulRes6),
                      "Test mul underflow failed!");

  int16_t subRes;
  BOOST_CHECK_MESSAGE(
      false == SafeMath<int16_t>::sub(0, std::numeric_limits<int16_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      false == SafeMath<int16_t>::sub(1, std::numeric_limits<int16_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      true == SafeMath<int16_t>::sub(-1, std::numeric_limits<int16_t>::min(),
                                     subRes),
      "Test signed sub failed!");
  BOOST_REQUIRE(std::numeric_limits<int16_t>::max() == subRes);

  LOG_GENERAL(INFO, "Test int16_t done!");
}

BOOST_AUTO_TEST_CASE(test_int32_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test int32_t start...");

  int32_t num1 = std::numeric_limits<int32_t>::max(), num2 = 1, addRes1,
          addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::add(num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::add(num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<int32_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::add(num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::add(num2, num1, addRes4),
                      "Test add underflow failed!");

  int32_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<int32_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::sub(num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<int32_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::sub(num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<int32_t>::min();
  num2 = -1;
  int32_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::div(num1, num2, divRes),
                      "Test div overflow failed!");

  int32_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<int32_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int32_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<int32_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int32_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int32_t>::mul(num1, num2, mulRes6),
                      "Test mul underflow failed!");

  int32_t subRes;
  BOOST_CHECK_MESSAGE(
      false == SafeMath<int32_t>::sub(0, std::numeric_limits<int32_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      false == SafeMath<int32_t>::sub(1, std::numeric_limits<int32_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      true == SafeMath<int32_t>::sub(-1, std::numeric_limits<int32_t>::min(),
                                     subRes),
      "Test signed sub failed!");
  BOOST_REQUIRE(std::numeric_limits<int32_t>::max() == subRes);

  LOG_GENERAL(INFO, "Test int32_t done!");
}

BOOST_AUTO_TEST_CASE(test_int64_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test int64_t start...");

  int64_t num1 = std::numeric_limits<int64_t>::max(), num2 = 1, addRes1,
          addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::add(num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::add(num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<int64_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::add(num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::add(num2, num1, addRes4),
                      "Test add underflow failed!");

  int64_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<int64_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::sub(num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<int64_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::sub(num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<int64_t>::min();
  num2 = -1;
  int64_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::div(num1, num2, divRes),
                      "Test div overflow failed!");

  int64_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<int64_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int64_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<int64_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<int64_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<int64_t>::mul(num1, num2, mulRes6),
                      "Test mul underflow failed!");

  int64_t subRes;
  BOOST_CHECK_MESSAGE(
      false == SafeMath<int64_t>::sub(0, std::numeric_limits<int64_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      false == SafeMath<int64_t>::sub(1, std::numeric_limits<int64_t>::min(),
                                      subRes),
      "Test signed sub overflow failed!");

  BOOST_CHECK_MESSAGE(
      true == SafeMath<int64_t>::sub(-1, std::numeric_limits<int64_t>::min(),
                                     subRes),
      "Test signed sub failed!");
  BOOST_REQUIRE(std::numeric_limits<int64_t>::max() == subRes);

  LOG_GENERAL(INFO, "Test int64_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_int128_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_int128_t start...");

  boost::multiprecision::int128_t
      num1 = std::numeric_limits<boost::multiprecision::int128_t>::max(),
      num2 = 1, addRes1, addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::add(
                                   num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::add(
                                   num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int128_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::add(
                                   num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::add(
                                   num2, num1, addRes4),
                      "Test add underflow failed!");

  boost::multiprecision::int128_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<boost::multiprecision::int128_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::sub(
                                   num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<boost::multiprecision::int128_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::sub(
                                   num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<boost::multiprecision::int128_t>::min();
  num2 = -1;
  boost::multiprecision::int128_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::div(
                                   num1, num2, divRes),
                      "Test div overflow failed!");

  boost::multiprecision::int128_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<boost::multiprecision::int128_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int128_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<boost::multiprecision::int128_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int128_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int128_t>::mul(
                                   num1, num2, mulRes6),
                      "Test mul underflow failed!");

  boost::multiprecision::int128_t subRes;
  BOOST_CHECK_MESSAGE(
      true == SafeMath<boost::multiprecision::int128_t>::sub(
                  0,
                  std::numeric_limits<boost::multiprecision::int128_t>::min(),
                  subRes),
      "Test signed sub overflow failed!");
  BOOST_REQUIRE(std::numeric_limits<boost::multiprecision::int128_t>::max() ==
                subRes);

  BOOST_CHECK_MESSAGE(
      false == SafeMath<boost::multiprecision::int128_t>::sub(
                   1,
                   std::numeric_limits<boost::multiprecision::int128_t>::min(),
                   subRes),
      "Test signed sub overflow failed!");

  LOG_GENERAL(INFO, "Test boost_int128_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_int256_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_int256_t start...");

  boost::multiprecision::int256_t
      num1 = std::numeric_limits<boost::multiprecision::int256_t>::max(),
      num2 = 1, addRes1, addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::add(
                                   num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::add(
                                   num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int256_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::add(
                                   num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::add(
                                   num2, num1, addRes4),
                      "Test add underflow failed!");

  boost::multiprecision::int256_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<boost::multiprecision::int256_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::sub(
                                   num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<boost::multiprecision::int256_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::sub(
                                   num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<boost::multiprecision::int256_t>::min();
  num2 = -1;
  boost::multiprecision::int256_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::div(
                                   num1, num2, divRes),
                      "Test div overflow failed!");

  boost::multiprecision::int256_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<boost::multiprecision::int256_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int256_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<boost::multiprecision::int256_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int256_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int256_t>::mul(
                                   num1, num2, mulRes6),
                      "Test mul underflow failed!");

  boost::multiprecision::int256_t subRes;
  BOOST_CHECK_MESSAGE(
      true == SafeMath<boost::multiprecision::int256_t>::sub(
                  0,
                  std::numeric_limits<boost::multiprecision::int256_t>::min(),
                  subRes),
      "Test signed sub overflow failed!");
  BOOST_REQUIRE(std::numeric_limits<boost::multiprecision::int256_t>::max() ==
                subRes);

  BOOST_CHECK_MESSAGE(
      false == SafeMath<boost::multiprecision::int256_t>::sub(
                   1,
                   std::numeric_limits<boost::multiprecision::int256_t>::min(),
                   subRes),
      "Test signed sub overflow failed!");

  LOG_GENERAL(INFO, "Test boost_int256_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_int512_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_int512_t start...");

  boost::multiprecision::int512_t
      num1 = std::numeric_limits<boost::multiprecision::int512_t>::max(),
      num2 = 1, addRes1, addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::add(
                                   num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::add(
                                   num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int512_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::add(
                                   num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::add(
                                   num2, num1, addRes4),
                      "Test add underflow failed!");

  boost::multiprecision::int512_t subRes1, subRes2;
  num1 = 1;
  num2 = num1 - std::numeric_limits<boost::multiprecision::int512_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::sub(
                                   num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<boost::multiprecision::int512_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::sub(
                                   num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<boost::multiprecision::int512_t>::min();
  num2 = -1;
  boost::multiprecision::int512_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::div(
                                   num1, num2, divRes),
                      "Test div overflow failed!");

  boost::multiprecision::int512_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<boost::multiprecision::int512_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int512_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<boost::multiprecision::int512_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int512_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int512_t>::mul(
                                   num1, num2, mulRes6),
                      "Test mul underflow failed!");

  boost::multiprecision::int512_t subRes;
  BOOST_CHECK_MESSAGE(
      true == SafeMath<boost::multiprecision::int512_t>::sub(
                  0,
                  std::numeric_limits<boost::multiprecision::int512_t>::min(),
                  subRes),
      "Test signed sub overflow failed!");
  BOOST_REQUIRE(std::numeric_limits<boost::multiprecision::int512_t>::max() ==
                subRes);

  BOOST_CHECK_MESSAGE(
      false == SafeMath<boost::multiprecision::int512_t>::sub(
                   1,
                   std::numeric_limits<boost::multiprecision::int512_t>::min(),
                   subRes),
      "Test signed sub overflow failed!");

  LOG_GENERAL(INFO, "Test boost_int512_t done!");
}

BOOST_AUTO_TEST_CASE(test_boost_int1024_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test boost_int1024_t start...");

  boost::multiprecision::int1024_t
      num1 = std::numeric_limits<boost::multiprecision::int1024_t>::max(),
      num2 = 1, addRes1, addRes2, addRes3, addRes4;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::add(
                                   num1, num2, addRes1),
                      "Test add overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::add(
                                   num2, num1, addRes2),
                      "Test add overflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int1024_t>::min();
  num2 = -1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::add(
                                   num1, num2, addRes3),
                      "Test add underflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::add(
                                   num2, num1, addRes4),
                      "Test add underflow failed!");

  boost::multiprecision::int1024_t subRes1, subRes2;
  num1 = 1;
  num2 =
      num1 - std::numeric_limits<boost::multiprecision::int1024_t>::max() - 1;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::sub(
                                   num1, num2, subRes1),
                      "Test sub overflow failed!");
  num1 = -2;
  num2 = std::numeric_limits<boost::multiprecision::int1024_t>::max();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::sub(
                                   num1, num2, subRes2),
                      "Test sub underflow failed!");

  num1 = std::numeric_limits<boost::multiprecision::int1024_t>::min();
  num2 = -1;
  boost::multiprecision::int1024_t mulRes1, mulRes2, divRes;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num1, num2, mulRes1),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num2, num1, mulRes2),
                      "Test mul overflow failed!");
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::div(
                                   num1, num2, divRes),
                      "Test div overflow failed!");

  boost::multiprecision::int1024_t mulRes3, mulRes4, mulRes5, mulRes6;
  num1 = std::numeric_limits<boost::multiprecision::int1024_t>::min();
  num2 = -2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num1, num2, mulRes3),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int1024_t>::min();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num1, num2, mulRes4),
                      "Test mul underflow failed!");
  num1 = 2;
  num2 = std::numeric_limits<boost::multiprecision::int1024_t>::min();
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num1, num2, mulRes5),
                      "Test mul underflow failed!");
  num1 = std::numeric_limits<boost::multiprecision::int1024_t>::max();
  num2 = 2;
  BOOST_CHECK_MESSAGE(false == SafeMath<boost::multiprecision::int1024_t>::mul(
                                   num1, num2, mulRes6),
                      "Test mul underflow failed!");

  boost::multiprecision::int1024_t subRes;
  BOOST_CHECK_MESSAGE(
      true == SafeMath<boost::multiprecision::int1024_t>::sub(
                  0,
                  std::numeric_limits<boost::multiprecision::int1024_t>::min(),
                  subRes),
      "Test signed sub overflow failed!");

  BOOST_REQUIRE(std::numeric_limits<boost::multiprecision::int1024_t>::max() ==
                subRes);

  BOOST_CHECK_MESSAGE(
      false == SafeMath<boost::multiprecision::int1024_t>::sub(
                   1,
                   std::numeric_limits<boost::multiprecision::int1024_t>::min(),
                   subRes),
      "Test signed sub overflow failed!");

  LOG_GENERAL(INFO, "Test boost_int1024_t done!");
}

BOOST_AUTO_TEST_SUITE_END()
