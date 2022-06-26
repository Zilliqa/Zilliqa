/*
 * Copyright (C) 2022 Zilliqa
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

#include "libUtils/EvmJsonResponse.h"
#include "libUtils/Logger.h"
#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(test_EvmJsonResponseGood) {
  INIT_STDOUT_LOGGER();

  Json::Value   input = "";
  evmproj::CallResponse result;

  auto response = evmproj::GetReturn(input,result);

  cout << "good" << endl;

}

BOOST_AUTO_TEST_CASE(test_EvmJsonResponseBad) {
  INIT_STDOUT_LOGGER();

  cout << "good" << endl;

}

BOOST_AUTO_TEST_SUITE_END()