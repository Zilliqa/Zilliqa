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

#include "libServer/EthRpcMethods.h"

#define BOOST_TEST_MODULE ethrpcmethodstest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

struct Fixture {
  Fixture() { INIT_STDOUT_LOGGER() }
};

BOOST_GLOBAL_FIXTURE(Fixture);

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_UnpackRevert1) {
  std::string message;
  zbytes input = DataConversion::HexStrToUint8VecRet(
      "08c379a0000000000000000000000000000000000000000000000000000000000000"
      "0020000000000000000000000000000000000000000000000000000000000000001a"
      "5a61703a2057726f6e6720747261646520646972656374696f6e000000000000");
  std::string input_str(input.begin(), input.end());
  EthRpcMethods::UnpackRevert(input_str, message);
  BOOST_CHECK_EQUAL(message, "Zap: Wrong trade direction");
}

BOOST_AUTO_TEST_CASE(test_UnpackRevert2) {
  std::string message;
  // prefix is wrong!
  zbytes input = DataConversion::HexStrToUint8VecRet(
      "09c379a0000000000000000000000000000000000000000000000000000000000000"
      "0020000000000000000000000000000000000000000000000000000000000000001a"
      "5a61703a2057726f6e6720747261646520646972656374696f6e000000000000");
  std::string input_str(input.begin(), input.end());
  BOOST_ASSERT(!EthRpcMethods::UnpackRevert(input_str, message));
}

BOOST_AUTO_TEST_CASE(test_UnpackRevert3) {
  std::string message;
  // not long enough
  zbytes input = DataConversion::HexStrToUint8VecRet(
      "08c379a00000000000000000000000000000000"
      "000000000000000000000000000000020");
  std::string input_str(input.begin(), input.end());
  BOOST_ASSERT(!EthRpcMethods::UnpackRevert(input_str, message));
}

BOOST_AUTO_TEST_CASE(test_UnpackRevert4) {
  std::string message;
  // not long enough
  zbytes input = DataConversion::HexStrToUint8VecRet(
      "08c379a000000000000000000000000000000000000000000000000000");
  std::string input_str(input.begin(), input.end());
  BOOST_ASSERT(!EthRpcMethods::UnpackRevert(input_str, message));
}

BOOST_AUTO_TEST_SUITE_END()
