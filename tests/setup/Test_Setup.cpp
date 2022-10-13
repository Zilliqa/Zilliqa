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
#define BOOST_TEST_MODULE setuptest
#define BOOST_TEST_DYN_LINK
#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>
#include <string>
#include "common/Constants.h"

BOOST_AUTO_TEST_SUITE(setuptest)

BOOST_AUTO_TEST_CASE(test_configuration) {
  INIT_STDOUT_LOGGER();

  LOG_GENERAL(INFO, "Checking Configuration is correct for tests");

  BOOST_CHECK_EQUAL(false, LOOKUP_NODE_MODE);
  BOOST_CHECK_EQUAL(false, ENABLE_SCILLA_MULTI_VERSION);

  if (ENABLE_EVM) {
    boost::filesystem::path evm_image(EVM_SERVER_BINARY);
    if (not boost::filesystem::exists(evm_image)) {
      LOG_GENERAL(WARNING,
                  "evm image does not seem to exist " << EVM_SERVER_BINARY);
    }
    if (not boost::filesystem::is_regular_file(EVM_SERVER_BINARY)) {
      LOG_GENERAL(WARNING,
                  "evm image is not a regular file " << EVM_SERVER_BINARY);
    }
  }
  if (ENABLE_EVM) {
    boost::filesystem::path scilla_image(SCILLA_ROOT);
    if (not boost::filesystem::exists(scilla_image)) {
      LOG_GENERAL(WARNING,
                  "scilla directory does not seem to exist " << SCILLA_ROOT);
    }
    if (not boost::filesystem::is_directory(SCILLA_ROOT)) {
      LOG_GENERAL(WARNING, "scilla root does not exist as a directory "
                               << SCILLA_ROOT);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
