/*
 * Copyright (C) 2020 Zilliqa
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

#include <array>
#include <string>
#include <thread>
#include <vector>

#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libTestUtils/TestUtils.h"
//#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  TestUtils::Initialize();
}

BOOST_AUTO_TEST_CASE(testPutExtSeedPubKey) {
  LOG_MARKER();

  if (LOOKUP_NODE_MODE && BlockStorage::GetBlockStorage().ResetDB(
                              BlockStorage::DBTYPE::EXTSEED_PUBKEYS)) {
    std::unordered_set<PubKey> in_pubks;

    for (int i = 0; i < 20; i++) {
      PubKey key = TestUtils::GenerateRandomPubKey();

      BlockStorage::GetBlockStorage().PutExtSeedPubKey(key);
      in_pubks.insert(key);
    }

    std::unordered_set<PubKey> out_pubks;
    BOOST_CHECK_MESSAGE(
        BlockStorage::GetBlockStorage().GetAllExtSeedPubKeys(out_pubks),
        "GetAllExtSeedPubKeys shouldn't fail");
    BOOST_CHECK_MESSAGE(
        out_pubks.size() == in_pubks.size(),
        "ExtSeed PubKeys shouldn't change after writting to reading from disk");
  }
}

BOOST_AUTO_TEST_CASE(testDeleteExtSeedPubKey) {
  // INIT_STDOUT_LOGGER();

  LOG_MARKER();

  if (LOOKUP_NODE_MODE && BlockStorage::GetBlockStorage().ResetDB(
                              BlockStorage::DBTYPE::EXTSEED_PUBKEYS)) {
    std::unordered_set<PubKey> in_pubks;

    for (int i = 0; i < 20; i++) {
      PubKey key = TestUtils::GenerateRandomPubKey();
      BlockStorage::GetBlockStorage().PutExtSeedPubKey(key);
      in_pubks.emplace(key);
    }

    std::unordered_set<PubKey> out_pubks;
    BOOST_CHECK_MESSAGE(
        BlockStorage::GetBlockStorage().GetAllExtSeedPubKeys(out_pubks),
        "GetAllExtSeedPubKeys shouldn't fail");

    BlockStorage::GetBlockStorage().DeleteExtSeedPubKey(*(out_pubks.begin()));

    out_pubks.clear();
    BOOST_CHECK_MESSAGE(
        BlockStorage::GetBlockStorage().GetAllExtSeedPubKeys(out_pubks),
        "GetAllExtSeedPubKeys shouldn't fail");

    BOOST_CHECK_MESSAGE(
        out_pubks.size() == in_pubks.size() - 1,
        "ExtSeed count should be reduced after delete from disk");
  }
}

BOOST_AUTO_TEST_SUITE_END()
