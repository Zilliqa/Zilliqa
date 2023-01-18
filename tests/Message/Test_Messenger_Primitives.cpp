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

#include <limits>
#include <random>
#include "libMessage/Messenger.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(messenger_primitives_test)

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  TestUtils::Initialize();
}

BOOST_AUTO_TEST_CASE(test_GetDSCommitteeHash) {
  DequeOfNode dsCommittee;
  CommitteeHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    dsCommittee.emplace_back(TestUtils::GenerateRandomPubKey(),
                             TestUtils::GenerateRandomPeer());
  }

  BOOST_CHECK(Messenger::GetDSCommitteeHash(dsCommittee, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardHash) {
  Shard shard;
  CommitteeHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    shard.emplace_back(TestUtils::GenerateRandomPubKey(),
                       TestUtils::GenerateRandomPeer(),
                       TestUtils::DistUint16());
  }

  BOOST_CHECK(Messenger::GetShardHash(shard, dst));
}

BOOST_AUTO_TEST_CASE(test_GetShardingStructureHash) {
  DequeOfShard shards;
  ShardingHash dst;

  for (unsigned int i = 0, count = TestUtils::Dist1to99(); i < count; i++) {
    shards.emplace_back();
    for (unsigned int j = 0, countj = TestUtils::Dist1to99(); j < countj; j++) {
      shards.back().emplace_back(TestUtils::GenerateRandomPubKey(),
                                 TestUtils::GenerateRandomPeer(),
                                 TestUtils::DistUint16());
    }
  }

  BOOST_CHECK(Messenger::GetShardingStructureHash(SHARDINGSTRUCTURE_VERSION,
                                                  shards, dst));
}

#if 0
BOOST_AUTO_TEST_CASE(test_SetAndGetDSBlock) {
  zbytes dst;
  unsigned int offset = 0;

  DSBlock dsBlock(TestUtils::GenerateRandomDSBlockHeader(),
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetDSBlock(dst, offset, dsBlock));

  DSBlock dsBlockDeserialized;

  BOOST_CHECK(Messenger::GetDSBlock(dst, offset, dsBlockDeserialized));

  BOOST_CHECK(dsBlock == dsBlockDeserialized);
}
#endif

#if 0
BOOST_AUTO_TEST_CASE(test_SetAndGetMicroBlock) {
  zbytes dst;
  unsigned int offset = 0;

  MicroBlockHeader microBlockHeader =
      TestUtils::GenerateRandomMicroBlockHeader();
  vector<TxnHash> tranHashes;

  for (unsigned int i = 0; i < microBlockHeader.GetNumTxs(); i++) {
    tranHashes.emplace_back();
  }

  MicroBlock microBlock(microBlockHeader, tranHashes,
                        TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetMicroBlock(dst, offset, microBlock));

  MicroBlock microBlockDeserialized;

  BOOST_CHECK(Messenger::GetMicroBlock(dst, offset, microBlockDeserialized));

  BOOST_CHECK(microBlock == microBlockDeserialized);
}
#endif

#if 0
BOOST_AUTO_TEST_CASE(test_SetAndGetTxBlock) {
  zbytes dst;
  unsigned int offset = 0;

  TxBlockHeader txBlockHeader = TestUtils::GenerateRandomTxBlockHeader();
  vector<MicroBlockInfo> microBlockInfo(0);

  TxBlock txBlock(txBlockHeader, microBlockInfo,
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetTxBlock(dst, offset, txBlock));

  TxBlock txBlockDeserialized;

  BOOST_CHECK(Messenger::GetTxBlock(dst, offset, txBlockDeserialized));

  BOOST_CHECK(txBlock == txBlockDeserialized);
}
#endif

#if 0
BOOST_AUTO_TEST_CASE(test_SetAndGetVCBlock) {
  zbytes dst;
  unsigned int offset = 0;
  VCBlock vcBlock(TestUtils::GenerateRandomVCBlockHeader(),
                  TestUtils::GenerateRandomCoSignatures());

  BOOST_CHECK(Messenger::SetVCBlock(dst, offset, vcBlock));

  VCBlock vcBlockDeserialized;

  BOOST_CHECK(Messenger::GetVCBlock(dst, offset, vcBlockDeserialized));

  BOOST_CHECK(vcBlock == vcBlockDeserialized);
}
#endif

BOOST_AUTO_TEST_CASE(test_CopyWithSizeCheck) {
  zbytes arr;
  dev::h256 result;

  generate(result.asArray().begin(), result.asArray().end(),
           []() -> unsigned char { return TestUtils::DistUint8(); });

  // Test source smaller by one byte
  arr.resize(result.asArray().size() - 1);
  generate(arr.begin(), arr.end(),
           []() -> unsigned char { return TestUtils::DistUint8(); });
  BOOST_CHECK(Messenger::CopyWithSizeCheck(arr, result.asArray()) == false);
  BOOST_CHECK(equal(arr.begin(), arr.end(), result.begin(), result.end()) ==
              false);

  // Test source larger by one byte
  arr.resize(result.asArray().size() + 1);
  BOOST_CHECK(Messenger::CopyWithSizeCheck(arr, result.asArray()) == false);
  BOOST_CHECK(equal(arr.begin(), arr.end(), result.begin(), result.end()) ==
              false);

  // Test equal sizes
  arr.resize(result.asArray().size());
  BOOST_CHECK(Messenger::CopyWithSizeCheck(arr, result.asArray()) == true);
  BOOST_CHECK(equal(arr.begin(), arr.end(), result.begin(), result.end()) ==
              true);
}

BOOST_AUTO_TEST_SUITE_END()
