/*
 * Copyright (C) 2023 Zilliqa
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

#define BOOST_TEST_MODULE ethabitest
#define BOOST_TEST_DYN_LINK
#include <json/value.h>
#include <boost/test/unit_test.hpp>
#include "libEth/Eth.h"

BOOST_AUTO_TEST_SUITE(EthAbiTest)

BOOST_AUTO_TEST_CASE(EmptyString) {
  std::string empty;
  const auto string = Eth::ConvertScillaEventToEthAbi(empty);
  const std::string EXPECTED =
      "0000000000000000000000000000000000000000000000000000000000000020000000"
      "0000000000000000000000000000000000000000000000000000000000";
  BOOST_CHECK_EQUAL(EXPECTED, string);
}

BOOST_AUTO_TEST_CASE(SingleCharacter) {
  std::string data{"1"};
  const auto string = Eth::ConvertScillaEventToEthAbi(data);

  const std::string EXPECTED =
      "0000000000000000000000000000000000000000000000000000000000000020000000"
      "000000000000000000000000000000000000000000000000000000000131000000000000"
      "00000000000000000000000000000000000000000000000000";
  BOOST_CHECK_EQUAL(EXPECTED, string);
}

BOOST_AUTO_TEST_CASE(SimpleText) {
  std::string data{"HelloWorld"};
  const auto string = Eth::ConvertScillaEventToEthAbi(data);
  const std::string EXPECTED =
      "000000000000000000000000000000000000000000000000000000000000002000000000"
      "0000000000000000000000000000000000000000000000000000000A48656C6C6F576F72"
      "6C6400000000000000000000000000000000000000000000";

  BOOST_CHECK_EQUAL(EXPECTED, string);
}

BOOST_AUTO_TEST_CASE(LongText) {
  std::string data{
      "123456789012345678901234567890123456789012345678901234567890"};
  const auto string = Eth::ConvertScillaEventToEthAbi(data);

  const std::string EXPECTED =
      "000000000000000000000000000000000000000000000000000000000000002000000000"
      "0000000000000000000000000000000000000000000000000000003C3132333435363738"
      "393031323334353637383930313233343536373839303132333435363738393031323334"
      "3536373839303132333435363738393000000000";

  BOOST_CHECK_EQUAL(EXPECTED, string);
}

BOOST_AUTO_TEST_CASE(VeryLongText) {
  std::string data{
      "123456789012345678901234567890123456789012345678901234567890123456789012"
      "345678901234567890123456789012345678901234567890123456789012345678901234"
      "567890123456789012345678901234567890123456789012345678901234567890123456"
      "789012345678901234567890123456789012345678901234567890123456789012345678"
      "901234567890"};
  const auto string = Eth::ConvertScillaEventToEthAbi(data);

  std::cerr << string << std::endl;

  const std::string EXPECTED =
      "000000000000000000000000000000000000000000000000000000000000002000000000"
      "0000000000000000000000000000000000000000000000000000012C3132333435363738"
      "393031323334353637383930313233343536373839303132333435363738393031323334"
      "353637383930313233343536373839303132333435363738393031323334353637383930"
      "313233343536373839303132333435363738393031323334353637383930313233343536"
      "373839303132333435363738393031323334353637383930313233343536373839303132"
      "333435363738393031323334353637383930313233343536373839303132333435363738"
      "393031323334353637383930313233343536373839303132333435363738393031323334"
      "353637383930313233343536373839303132333435363738393031323334353637383930"
      "313233343536373839303132333435363738393031323334353637383930313233343536"
      "373839300000000000000000000000000000000000000000";

  BOOST_CHECK_EQUAL(EXPECTED, string);
}

BOOST_AUTO_TEST_SUITE_END()
