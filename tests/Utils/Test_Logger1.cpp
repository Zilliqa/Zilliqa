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

#include "libUtils/JoinableFunction.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

void test() {
  LOG_MARKER();
  LOG_GENERAL(INFO, "Hello world");
}

BOOST_AUTO_TEST_CASE(testLogger1) {
  // Write to a file
  INIT_FILE_LOGGER("test1", "./");
  bytes bytestream = {0x12, 0x34, 0x56, 0x78, 0x9A};

  LOG_GENERAL(INFO, "Hello world");
  LOG_PAYLOAD(INFO, "Hello world", bytestream,
              Logger::MAX_BYTES_TO_DISPLAY);  // use default max payload length
  LOG_PAYLOAD(INFO, "Hello world", bytestream,
              5);  // use max payload length = payload length
  LOG_PAYLOAD(INFO, "Hello world", bytestream,
              4);  // use max payload length < payload length

  // Try in different thread
  JoinableFunction(1, test);
}

BOOST_AUTO_TEST_SUITE_END()
