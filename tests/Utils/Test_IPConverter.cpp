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

#include <string>
#include "libUtils/IPConverter.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ipconverter
#define BOOST_TEST_DYN_LINK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(ipconverter)

BOOST_AUTO_TEST_CASE(test_IPNumericaltoString) {
  INIT_STDOUT_LOGGER();

  std::string result = IPConverter::ToStrFromNumericalIP(
      (boost::multiprecision::uint128_t)16777343);
  BOOST_CHECK_MESSAGE(result == "127.0.0.1",
                      "Expected: 127.0.0.1. Result: " + result);
}

BOOST_AUTO_TEST_CASE(test_IPStringToNumerical) {
  INIT_STDOUT_LOGGER();

  boost::multiprecision::uint128_t result =
      IPConverter::ToNumericalIPFromStr("127.0.0.1");
  BOOST_CHECK_MESSAGE(result == 16777343, "Expected: 16777343. Result: " +
                                              result.convert_to<std::string>());
}

BOOST_AUTO_TEST_SUITE_END()
