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

  struct sockaddr_in serv_addr;

  inet_aton("0.0.0.0", &serv_addr.sin_addr);

  bool b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);
  BOOST_CHECK_MESSAGE(!b, "0.0.0.0 is not a valid IP");

  inet_aton("255.255.255.255", &serv_addr.sin_addr);
  b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

  BOOST_CHECK_MESSAGE(!b, "255.255.255.255 is not a valid IP");

  if (EXCLUDE_PRIV_IP) {
    Guard::GetInstance().AddToExclusionList("172.16.0.0", "172.31.255.255");
    // Guard::GetInstance().Init();
    inet_aton("172.25.4.3", &serv_addr.sin_addr);

    b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

    BOOST_CHECK_MESSAGE(!b, "The address should not be valid");
  }
  inet_aton("172.14.4.3", &serv_addr.sin_addr);
  b = Guard::GetInstance().IsValidIP(serv_addr.sin_addr.s_addr);

  BOOST_CHECK_MESSAGE(b, "The address should be valid");
}

BOOST_AUTO_TEST_SUITE_END()
