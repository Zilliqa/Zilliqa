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

#define BOOST_TEST_MODULE data_conversion
#define BOOST_TEST_DYN_LINK
#include <boost/algorithm/string.hpp>
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

BOOST_AUTO_TEST_CASE(test_hexstr) {
  LOG_GENERAL(INFO, "Test HexString Conversion start...");

  {
    LOG_GENERAL(INFO, "Test HexStringToUint64 start...");

    uint64_t expected_num = 0xFEEB2048DEADBEEF;

    std::vector<std::string> pass_vector = {
        "feeb2048deadbeef",       "0feeb2048deadbeef",
        "00feeb2048deadbeef",     "0000feeb2048deadbeef",
        "0xfeeb2048deadbeef",     "0x0feeb2048deadbeef",
        "0x00feeb2048deadbeef",   "0x0000feeb2048deadbeef",
        " 0xfeeb2048deadbeef",    "0x0feeb2048deadbeef ",
        " 0x00feeb2048deadbeef ", " 0x0000feeb2048deadbeef  ",
        "Feeb2048DeadBeef",       "0xFEEB2048DeadBeef",
        "0xfeeb2048DEADBEEF"};

    std::vector<std::string> fail_vector = {
        "feeb2048deadbeef0",
        "feeb2048deadbeef00",
        "0xxfeeb2048deadbeef",
        "xfeeb2048deadbeef",
        "00000000",
        "FFFFFFFF",
        "0xEEEB2048DEADBEEF",
        "FEEB 2048 DEAD BEEF",
    };

    for (auto& hex_str : pass_vector) {
      uint64_t value = 0xFFFFFFFF;
      bool res = DataConversion::HexStringToUint64(hex_str, &value);

      BOOST_CHECK_MESSAGE(res == true, "Test Failed: " << hex_str);
      BOOST_CHECK_MESSAGE(value == expected_num,
                          "Test Failed: " << hex_str << "Expected: "
                                          << expected_num << "Got: " << value);

      std::string upper_str = boost::to_upper_copy(hex_str);
      res = DataConversion::HexStringToUint64(hex_str, &value);

      BOOST_CHECK_MESSAGE(res == true, "Test Failed: " << upper_str);
      BOOST_CHECK_MESSAGE(value == expected_num,
                          "Test Failed: " << upper_str << "Expected: "
                                          << expected_num << "Got: " << value);
    }

    for (auto& hex_str : fail_vector) {
      uint64_t value = 0x0000;
      DataConversion::HexStringToUint64(hex_str, &value);

      BOOST_CHECK_MESSAGE(value != expected_num, "Test Failed: " << hex_str);

      std::string upper_str = boost::to_upper_copy(hex_str);
      DataConversion::HexStringToUint64(hex_str, &value);

      BOOST_CHECK_MESSAGE(value != expected_num, "Test Failed: " << upper_str);
    }
  }

  {
    LOG_GENERAL(INFO, "Test NormalizeHexString start...");
    std::string expected_str = "feeb2048deadbeef";

    std::vector<std::string> pass_vector = {
        "feeb2048deadbeef",   "0xfeeb2048deadbeef", "0Xfeeb2048deadbeef",
        "Feeb2048deadbeef",   "0xFeeb2048deadbeef", "feeb2048deadBEEF",
        "FEEB2048deadbeef",   "FEEB2048DEADBEEF",   "0XFEEB2048DEADBEEF",
        "0xFEEB2048DEADBEEF",
    };

    for (auto& hex_str : pass_vector) {
      bool res = DataConversion::NormalizeHexString(hex_str);

      BOOST_CHECK_MESSAGE(res == true, "Test Failed: " << hex_str);

      BOOST_CHECK_MESSAGE(hex_str == expected_str,
                          "Test Failed: "
                              << "Expected: " << expected_str
                              << "Got: " << hex_str);
    }
  }

  LOG_GENERAL(INFO, "Test HexString Conversion done!");
}

BOOST_AUTO_TEST_SUITE_END()