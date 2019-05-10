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

  for (uint128_t i = 0; i < 200; ++i) {
    BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                        "Bad IP should not existed in the blacklist!");
  }

  LOG_GENERAL(INFO, "Test Blacklist initialization done!");

  for (uint128_t i = 0; i < 100; ++i) {
    bl.Add(i);
  }

  for (uint128_t i = 0; i < 200; ++i) {
    if (i < 100) {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == true,
                          "Bad IP should existed in the blacklist!");
    } else {
      BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                          "Bad IP should not existed in the blacklist!");
    }
  }

  LOG_GENERAL(INFO, "Test Blacklist addition done!");

  for (uint128_t i = 0; i < 200; i += 2) {
    bl.Remove(i);
  }

  for (uint128_t i = 0; i < 200; ++i) {
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

  for (uint128_t i = 0; i < 200; ++i) {
    BOOST_CHECK_MESSAGE(bl.Exist(i) == false,
                        "Bad IP should not existed in the blacklist!");
  }

  LOG_GENERAL(INFO, "Test Blacklist termination done!");
}

BOOST_AUTO_TEST_CASE(test_pop) {
  INIT_STDOUT_LOGGER();

  Blacklist& bl = Blacklist::GetInstance();
  bl.Clear();

  for (uint128_t i = 0; i < 100; ++i) {
    bl.Add(i);
  }

  bl.Pop(5);
  BOOST_CHECK_MESSAGE(bl.SizeOfBlacklist() == 95,
                      "Unexpected blacklist size: " << bl.SizeOfBlacklist());

  bl.Pop(1000);
  BOOST_CHECK_MESSAGE(bl.SizeOfBlacklist() == 0,
                      "Unexpected blacklist size: " << bl.SizeOfBlacklist());

  LOG_GENERAL(INFO, "Test Blacklist pop done!");
}

BOOST_AUTO_TEST_SUITE_END()
