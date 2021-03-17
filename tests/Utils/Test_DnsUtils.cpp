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

BOOST_AUTO_TEST_CASE(testDnsUtilsLookup) {
  // vector<uint128_t> ipList;
  // bool res = ObtainIpListFromDns(ipList, "public.seed.zilliqa.com");
  // BOOST_CHECK_MESSAGE(res, "Unable to connect");
  // BOOST_CHECK_MESSAGE(ipList.size() > 0, "Suppose to have some ip
  // addresses");
}

BOOST_AUTO_TEST_CASE(testDnsUtilsQueryPubKeyFromUrl) {
  LOG_MARKER();

  // auto testUrl = "public.seed.zilliqa.com";

  // vector<string> ipList;
  // bool res = ObtainIpStrListFromDns(ipList, testUrl);
  // BOOST_CHECK_MESSAGE(res, "Unable to connect");
  // BOOST_CHECK_MESSAGE(ipList.size() > 0, "Suppose to have some ip
  // addresses");

  // for (const auto& ip : ipList) {
  //   bytes output;
  //   bool res = ObtainPubKeyFromUrl(output, ip, testUrl);

  //   PubKey pubKey(output, 0);

  //   BOOST_CHECK_MESSAGE(res,
  //                       "Query failed for ip: " << ip << " dns: " <<
  //                       testUrl);

  //   if (res) {
  //     BOOST_CHECK_MESSAGE(
  //         output.size() == 33,
  //         "Incorrect output for ip: " << ip << " dns: " << testUrl);
  //   }
  // }
}

BOOST_AUTO_TEST_SUITE_END()
