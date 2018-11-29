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
