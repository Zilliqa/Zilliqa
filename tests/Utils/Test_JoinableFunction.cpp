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

#include <memory>
#include <mutex>
#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

mutex m;

void test1() {
  LOG_MARKER();
  LOG_GENERAL(INFO, "Sleep for 3 secs...");
  this_thread::sleep_for(chrono::seconds(3));
}

void test2(const shared_ptr<vector<string>>& s) {
  LOG_MARKER();

  lock_guard<mutex> guard(m);
  LOG_GENERAL(INFO, s->back().c_str());
  s->pop_back();
}

BOOST_AUTO_TEST_CASE(testJoinableFunction) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  JoinableFunction jf1(1, test1);

  shared_ptr<vector<string>> s = make_shared<vector<string>>();
  s->emplace_back("one");
  s->emplace_back("two");
  s->emplace_back("three");

  JoinableFunction jf2(3, test2, s);
}

BOOST_AUTO_TEST_SUITE_END()
