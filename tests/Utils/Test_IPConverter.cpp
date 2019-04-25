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

#include <string>
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ipconverter
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(ipconverter)

BOOST_AUTO_TEST_CASE(test_IPNumericaltoString) {
  INIT_STDOUT_LOGGER();

  std::string result = IPConverter::ToStrFromNumericalIP((uint128_t)16777343);
  BOOST_CHECK_MESSAGE(result == "127.0.0.1",
                      "Expected: 127.0.0.1. Result: " + result);
}

BOOST_AUTO_TEST_CASE(test_IPStringToNumerical) {
  INIT_STDOUT_LOGGER();

  uint128_t result;
  BOOST_CHECK_MESSAGE(IPConverter::ToNumericalIPFromStr("127.0.0.1", result),
                      "Conversion from IP "
                          << "127.0.0.1"
                          << " to integer failed.");
  BOOST_CHECK_MESSAGE(result == 16777343, "Expected: 16777343. Result: " +
                                              result.convert_to<std::string>());
}

BOOST_AUTO_TEST_SUITE_END()
