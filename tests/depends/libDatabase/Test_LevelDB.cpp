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
#include <array>
#include <string>
#include <thread>
#include <vector>

#define BOOST_TEST_MODULE trietest
#define BOOST_TEST_DYN_LINK
#include <boost/filesystem/path.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <boost/test/unit_test.hpp>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/LevelDB.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

BOOST_AUTO_TEST_SUITE(trietest)

BOOST_AUTO_TEST_CASE(fat_trie) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  LevelDB m_testDB("test");

  m_testDB.Insert((boost::multiprecision::uint256_t)1, "ABB");

  BOOST_CHECK_MESSAGE(
      m_testDB.Lookup((boost::multiprecision::uint256_t)1) == "ABB",
      "ERROR: (boost_int, string)");

  m_testDB.Insert((boost::multiprecision::uint256_t)2, "apples");

  BOOST_CHECK_MESSAGE(
      m_testDB.Lookup((boost::multiprecision::uint256_t)2) == "apples",
      "ERROR: (boost_int, string)");

  bytes mangoMsg = {'m', 'a', 'n', 'g', 'o'};

  m_testDB.Insert((boost::multiprecision::uint256_t)3, mangoMsg);

  LOG_GENERAL(INFO, m_testDB.Lookup((boost::multiprecision::uint256_t)3));
}

BOOST_AUTO_TEST_SUITE_END()
