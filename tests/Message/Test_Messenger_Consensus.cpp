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
  vector<unsigned char> dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  vector<unsigned char> blockHash(TestUtils::Dist1to99(),
                                  TestUtils::DistUint8());
  uint16_t backupID = max((uint16_t)2, (uint16_t)TestUtils::Dist1to99());
  CommitPoint commit = CommitPoint(CommitSecret());
  pair<PrivKey, PubKey> backupKey;
  backupKey.first = PrivKey();
  backupKey.second = PubKey(backupKey.first);

  BOOST_CHECK(Messenger::SetConsensusCommit(dst, offset, consensusID,
                                            blockNumber, blockHash, backupID,
                                            commit, backupKey));

  deque<pair<PubKey, Peer>> committeeKeys;
  for (unsigned int i = 0, count = max((unsigned int)backupID + 1,
                                       (unsigned int)TestUtils::Dist1to99());
       i < count; i++) {
    committeeKeys.emplace_back(
        (i == backupID) ? backupKey.second : TestUtils::GenerateRandomPubKey(),
        TestUtils::GenerateRandomPeer());
  }

  CommitPoint commitDeserialized;

  BOOST_CHECK(Messenger::GetConsensusCommit(dst, offset, consensusID,
                                            blockNumber, blockHash, backupID,
                                            commitDeserialized, committeeKeys));

  BOOST_CHECK(commit == commitDeserialized);
}

BOOST_AUTO_TEST_CASE(test_SetAndGetConsensusChallenge) {
  vector<unsigned char> dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  uint16_t subsetID = TestUtils::DistUint16();
  vector<unsigned char> blockHash(TestUtils::Dist1to99(),
                                  TestUtils::DistUint8());
  uint16_t leaderID = TestUtils::DistUint8();
  CommitPoint aggregatedCommit = CommitPoint(CommitSecret());
  PubKey aggregatedKey = PubKey(PrivKey());
  Challenge challenge(
      aggregatedCommit, aggregatedKey,
      vector<unsigned char>(TestUtils::Dist1to99(), TestUtils::DistUint8()));
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
  vector<unsigned char> dst;
  unsigned int offset = 0;
  uint32_t consensusID = TestUtils::DistUint32();
  uint64_t blockNumber = TestUtils::DistUint32();
  vector<unsigned char> blockHash(TestUtils::Dist1to99(),
                                  TestUtils::DistUint8());
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