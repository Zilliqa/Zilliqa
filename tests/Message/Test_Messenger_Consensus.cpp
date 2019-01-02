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

BOOST_AUTO_TEST_CASE(test_SetAndGetConsensusCommit) {
  bytes dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  bytes blockHash(TestUtils::Dist1to99(), TestUtils::DistUint8());
  uint16_t backupID = max((uint16_t)2, (uint16_t)TestUtils::Dist1to99());
  CommitPoint commitPoint = CommitPoint(CommitSecret());
  CommitPointHash commitPointHash(commitPoint);
  pair<PrivKey, PubKey> backupKey;
  backupKey.first = PrivKey();
  backupKey.second = PubKey(backupKey.first);

  BOOST_CHECK(Messenger::SetConsensusCommit(
      dst, offset, consensusID, blockNumber, blockHash, backupID, commitPoint,
      commitPointHash, backupKey));

  deque<pair<PubKey, Peer>> committeeKeys;
  for (unsigned int i = 0, count = max((unsigned int)backupID + 1,
                                       (unsigned int)TestUtils::Dist1to99());
       i < count; i++) {
    committeeKeys.emplace_back(
        (i == backupID) ? backupKey.second : TestUtils::GenerateRandomPubKey(),
        TestUtils::GenerateRandomPeer());
  }

  CommitPoint commitPointDeserialized;
  CommitPointHash commitPointHashDeserialized;

  BOOST_CHECK(Messenger::GetConsensusCommit(
      dst, offset, consensusID, blockNumber, blockHash, backupID,
      commitPointDeserialized, commitPointHashDeserialized, committeeKeys));

  BOOST_CHECK(commitPoint == commitPointDeserialized);
  BOOST_CHECK(commitPointHash == commitPointHashDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetConsensusChallenge) {
  bytes dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  uint16_t subsetID = TestUtils::DistUint16();
  bytes blockHash(TestUtils::Dist1to99(), TestUtils::DistUint8());
  uint16_t leaderID = TestUtils::DistUint8();
  CommitPoint aggregatedCommit = CommitPoint(CommitSecret());
  PubKey aggregatedKey = PubKey(PrivKey());
  Challenge challenge(aggregatedCommit, aggregatedKey,
                      bytes(TestUtils::Dist1to99(), TestUtils::DistUint8()));
  pair<PrivKey, PubKey> leaderKey;
  leaderKey.first = PrivKey();
  leaderKey.second = PubKey(leaderKey.first);

  BOOST_CHECK(Messenger::SetConsensusChallenge(
      dst, offset, consensusID, blockNumber, subsetID, blockHash, leaderID,
      aggregatedCommit, aggregatedKey, challenge, leaderKey));

  Challenge challengeDeserialized;

  BOOST_CHECK(Messenger::GetConsensusChallenge(
      dst, offset, consensusID, blockNumber, subsetID, blockHash, leaderID,
      aggregatedCommit, aggregatedKey, challengeDeserialized,
      leaderKey.second));

  BOOST_CHECK(challenge == challengeDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetConsensusConsensusFailure) {
  bytes dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  bytes blockHash(TestUtils::Dist1to99(), TestUtils::DistUint8());
  uint16_t leaderID = TestUtils::DistUint8();
  pair<PrivKey, PubKey> leaderKey;
  leaderKey.first = PrivKey();
  leaderKey.second = PubKey(leaderKey.first);

  BOOST_CHECK(Messenger::SetConsensusConsensusFailure(
      dst, offset, consensusID, blockNumber, blockHash, leaderID, leaderKey));

  BOOST_CHECK(Messenger::GetConsensusConsensusFailure(
      dst, offset, consensusID, blockNumber, blockHash, leaderID,
      leaderKey.second));
}

BOOST_AUTO_TEST_SUITE_END()
