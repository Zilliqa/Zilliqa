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

#include <cstring>
#include <iostream>
#include <mutex>
#include "libUtils/Logger.h"
#include "libUtils/TimeLockedFunction.h"

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

mutex m;
int counter;

void main_function(int count_up_to) {
  LOG_MARKER();

  counter = 0;
  for (int i = 0; i < count_up_to; i++) {
    {
      lock_guard<mutex> guard(m);
      counter++;
    }
    this_thread::sleep_for(chrono::seconds(1));
  }
}

void expiry_function(int count_up_to) {
  LOG_MARKER();

  lock_guard<mutex> guard(m);

  if (counter == count_up_to) {
    LOG_GENERAL(
        INFO, "Last count = " << counter << " => main_func executed on time!");
  } else {
    LOG_GENERAL(
        INFO, "Last count = " << counter << " => main_func executed too slow!");
  }
}

void test(int target, int delay) {
  LOG_MARKER();

  LOG_GENERAL(INFO,
              "Test: Count to " << target << " before " << delay << " seconds");

  auto main_func = [target]() -> void { main_function(target); };
  auto expiry_func = [target]() -> void { expiry_function(target); };
  TimeLockedFunction tlf(delay, main_func, expiry_func, true);
}

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(testTimeLockedFunction) {
  INIT_STDOUT_LOGGER();

  test(5, 4);
  test(5, 5);
  test(5, 10);
}

BOOST_AUTO_TEST_SUITE_END()
