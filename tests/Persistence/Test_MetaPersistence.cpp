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

#include <algorithm>
#include <vector>

#include "common/Constants.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testReadWriteSimpleStringToDB) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DB db("test.db");

  db.WriteToDB("fruit", "vegetable");

  std::string ret = db.ReadFromDB("fruit");

  BOOST_CHECK_MESSAGE(
      ret == "vegetable",
      "ERROR: return value from DB not equal to inserted value");
}

BOOST_AUTO_TEST_CASE(testWriteAndReadSTATEROOT) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  dev::h256 in_root;
  std::fill(in_root.asArray().begin(), in_root.asArray().end(), 0x77);
  BlockStorage::GetBlockStorage().PutMetadata(STATEROOT, in_root.asBytes());

  bytes rootBytes;
  BlockStorage::GetBlockStorage().GetMetadata(STATEROOT, rootBytes);
  dev::h256 out_root(rootBytes);

  BOOST_CHECK_MESSAGE(
      in_root == out_root,
      "STATEROOT hash shouldn't change after writing to /reading from disk");
}

BOOST_AUTO_TEST_SUITE_END()