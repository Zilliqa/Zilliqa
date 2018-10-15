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

#include "libNetwork/Blacklist.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE blacklist
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(blacklist)

BOOST_AUTO_TEST_CASE(test_fundamental) {
  INIT_STDOUT_LOGGER();

  Blacklist& bl = Blacklist::GetInstance();
  bl.Clear();

  for (boost::multiprecision::uint128_t i = 0; i < 200; ++i) {
    BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                        "Bad IP should not existed in the blacklist!");
  }

  LOG_GENERAL(INFO, "Test Blacklist initialization done!");

  for (boost::multiprecision::uint128_t i = 0; i < 100; ++i) {
    bl.Add(i);
  }

  for (boost::multiprecision::uint128_t i = 0; i < 200; ++i) {
    if (i < 100) {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == true,
                          "Bad IP should existed in the blacklist!");
    } else {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                          "Bad IP should not existed in the blacklist!");
    }
  }

  LOG_GENERAL(INFO, "Test Blacklist addition done!");

  for (boost::multiprecision::uint128_t i = 0; i < 200; i += 2) {
    bl.Remove(i);
  }

  for (boost::multiprecision::uint128_t i = 0; i < 200; ++i) {
    if (i < 100 && (1 == (i % 2))) {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == true,
                          "Bad IP should existed in the blacklist!");
    } else {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                          "Bad IP should not existed in the blacklist!");
    }
  }

  LOG_GENERAL(INFO, "Test Blacklist removal done!");

  bl.Clear();

  for (boost::multiprecision::uint128_t i = 0; i < 200; ++i) {
    BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                        "Bad IP should not existed in the blacklist!");
  }

  LOG_GENERAL(INFO, "Test Blacklist termination done!");
}

BOOST_AUTO_TEST_SUITE_END()
