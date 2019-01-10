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

#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE safemath
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(data_conversion)

BOOST_AUTO_TEST_CASE(test_uint8_t) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Test IntegerToBytes start...");
  LOG_GENERAL(INFO, "Test uint8_t start...");

  uint8_t num1 = 0x01, num2 = 0xAB;
  bytes bytesOfNum1 =
      DataConversion::IntegerToBytes<uint8_t, sizeof(uint8_t)>(num1);
  BOOST_REQUIRE(bytesOfNum1 == bytes{num1});

  bytes bytesOfNum2 =
      DataConversion::IntegerToBytes<uint8_t, sizeof(uint8_t)>(num2);
  BOOST_REQUIRE(bytesOfNum2 == bytes{num2});

  {
    LOG_GENERAL(INFO, "Test uint32_t start...");

    uint32_t uint32Num1 = 0x01234567;
    bytes bytesOfUint32Num1 =
        DataConversion::IntegerToBytes<uint32_t, sizeof(uint32_t)>(uint32Num1);
    bytes goldenResult{0x01, 0x23, 0x45, 0x67};
    BOOST_REQUIRE(bytesOfUint32Num1 == goldenResult);
  }

  {
    LOG_GENERAL(INFO, "Test uint64_t start...");

    uint64_t uint64Num1 = 0x01234567;
    bytes bytesOfUint64Num1 =
        DataConversion::IntegerToBytes<uint64_t, sizeof(uint64_t)>(uint64Num1);
    bytes goldenResult{0x00, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45, 0x67};
    BOOST_REQUIRE(bytesOfUint64Num1 == goldenResult);

    LOG_GENERAL(INFO, "Test IntegerToBytes done!");
  }
}

BOOST_AUTO_TEST_SUITE_END()