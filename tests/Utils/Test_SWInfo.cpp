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

#include "common/Constants.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SWInfo.h"
#include "libUtils/SysCommand.h"

#define BOOST_TEST_MODULE SWInfo
#define BOOST_TEST_DYN_LINK
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <boost/test/unit_test.hpp>
#pragma GCC diagnostic pop

#include <map>
#include <vector>

using namespace std;

BOOST_AUTO_TEST_SUITE(SWInfoTest)

/// SW Info test of constructors and getters
BOOST_AUTO_TEST_CASE(swinfo_copy_constructor) {
  SWInfo swInfo(1, 2, 3, 4, 5);
  SWInfo swInfoCopy(swInfo);
  BOOST_CHECK(!(swInfo > swInfoCopy));
  BOOST_CHECK(!(swInfo < swInfoCopy));
  BOOST_CHECK(!(swInfo != swInfoCopy));
  BOOST_CHECK_EQUAL(1, swInfo.GetMajorVersion());
  BOOST_CHECK_EQUAL(2, swInfo.GetMinorVersion());
  BOOST_CHECK_EQUAL(3, swInfo.GetFixVersion());
  BOOST_CHECK_EQUAL(4, swInfo.GetUpgradeDS());
  BOOST_CHECK_EQUAL(5, swInfo.GetCommit());
}

/// SysCommand test
BOOST_AUTO_TEST_CASE(syscommand_test) {
  std::string input = "echo TEST";
  std::string output;
  SysCommand::ExecuteCmdWithOutput(input, output);
  BOOST_CHECK_EQUAL(output, "TEST\n");
}

BOOST_AUTO_TEST_SUITE_END()
