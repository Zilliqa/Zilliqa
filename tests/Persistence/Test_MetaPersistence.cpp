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
