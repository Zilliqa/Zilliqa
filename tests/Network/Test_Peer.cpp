/*
 * Copyright (C) 2020 Zilliqa
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
#include "libNetwork/Peer.h"

#define BOOST_TEST_MODULE peer_test
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(peer_test)

BOOST_AUTO_TEST_CASE(test_print_ip_numerical_to_String) {
  Peer p((boost::multiprecision::uint128_t)16777343, 0);
  std::string result = p.GetPrintableIPAddress();
  BOOST_CHECK_MESSAGE(result == "127.0.0.1",
                      "Expected: 127.0.0.1 , Result: " + result);
  Peer p1;
  result = p1.GetPrintableIPAddress();
  BOOST_CHECK_MESSAGE(result == "0.0.0.0",
                      "Expected: 0.0.0.0 , Result: " + result);
}

BOOST_AUTO_TEST_SUITE_END()
