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

#include <arpa/inet.h>
#include <string>
#include "libNetwork/Guard.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE ipfilter_test
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(ipfilter_test)

BOOST_AUTO_TEST_CASE(test1) {
  INIT_STDOUT_LOGGER();

  struct sockaddr_in serv_addr {};

  inet_pton(AF_INET, "0.0.0.0", &serv_addr.sin_addr);

  bool b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);
  BOOST_CHECK_MESSAGE(!b, "0.0.0.0 is not a valid IP");

  inet_pton(AF_INET, "255.255.255.255", &serv_addr.sin_addr);
  b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

  BOOST_CHECK_MESSAGE(!b, "255.255.255.255 is not a valid IP");

  if (EXCLUDE_PRIV_IP) {
    Guard::GetInstance().AddToExclusionList("172.16.0.0", "172.31.255.255");
    // Guard::GetInstance().Init();
    inet_pton(AF_INET, "172.25.4.3", &serv_addr.sin_addr);

    b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

    BOOST_CHECK_MESSAGE(!b, "The address should not be valid");
  }
  inet_pton(AF_INET, "172.14.4.3", &serv_addr.sin_addr);
  b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

  BOOST_CHECK_MESSAGE(b, "The address should be valid");
}

BOOST_AUTO_TEST_SUITE_END()
