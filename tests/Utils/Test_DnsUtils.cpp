/*
 * Copyright (C) 2021 Zilliqa
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

#include <Schnorr.h>
#include "libUtils/DnsUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE dnsutils
#define BOOST_TEST_DYN_LINK
#include <algorithm>
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(dnsutils)

BOOST_AUTO_TEST_CASE(testGetPubKeyUrl) {
  string url = "zilliqa-seedpubs.dev.z7a.xyz";
  string ip = "54.148.35.87";
  string output = GetPubKeyUrl(ip, url);
  BOOST_CHECK_MESSAGE(output.compare("pub-54-148-35-87.dev.z7a.xyz") == 0,
                      "Incorrect output: " << output);
}

BOOST_AUTO_TEST_SUITE_END()