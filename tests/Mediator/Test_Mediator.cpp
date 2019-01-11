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
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(mediator_test)

Mediator* m;

BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  rng.seed(std::random_device()());
  PairOfKey kp = TestUtils::GenerateRandomKeyPair();
  Peer p = Peer();
  m = new Mediator(kp, p);
}

BOOST_AUTO_TEST_CASE(RegisterColleagues) {
  DirectoryService ds(*m);
  Node node(*m, 0, false);
  Lookup lookup(*m);
  Validator validator(*m);

  m->RegisterColleagues(&ds, &node, &lookup, &validator);
}

BOOST_AUTO_TEST_CASE(UpdateDSBlockRand) {
  m->UpdateDSBlockRand(false);
  m->UpdateDSBlockRand(true);
}

BOOST_AUTO_TEST_CASE(UpdateTxBlockRand) {
  m->UpdateTxBlockRand(false);
  m->UpdateTxBlockRand(true);
}

BOOST_AUTO_TEST_CASE(HeartBeat) { m->HeartBeatLaunch(); }

BOOST_AUTO_TEST_CASE(VacuousEpoch) {
  uint64_t VACUOUS_EPOCH = NUM_FINAL_BLOCK_PER_POW - 1;
  uint64_t ve = (NUM_FINAL_BLOCK_PER_POW *
                 TestUtils::RandomIntInRng<uint64_t>(1, 200000)) -
                1;
  BOOST_CHECK_MESSAGE(m->GetIsVacuousEpoch(ve),
                      "Incorrect Vacuous epoch. POW period:" +
                          std::to_string(NUM_FINAL_BLOCK_PER_POW) +
                          ". Current block: " + std::to_string(ve));

  uint64_t i = 1;
  for (; i < VACUOUS_EPOCH; i++) {
    m->IncreaseEpochNum();
    BOOST_CHECK_MESSAGE(!m->GetIsVacuousEpoch(),
                        "Incorrect Vacuous epoch. Final block number per POW " +
                            std::to_string(NUM_FINAL_BLOCK_PER_POW) +
                            ". Current block: " + std::to_string(i));
  }
  m->IncreaseEpochNum();
  BOOST_CHECK_MESSAGE(m->GetIsVacuousEpoch(),
                      "Missed Vacuous epoch. Final block number per POW " +
                          std::to_string(NUM_FINAL_BLOCK_PER_POW) +
                          ". Current block: " + std::to_string(i));
}

BOOST_AUTO_TEST_CASE(GetNodeMode) {
  uint32_t size = TestUtils::RandomIntInRng<uint32_t>(2, 100);
  for (uint32_t i = 1; i <= size; i++) {
    m->m_DSCommittee->push_front(
        std::make_pair(TestUtils::GenerateRandomPubKey(),
                       TestUtils::GenerateRandomPeer(0, true)));
  }

  Peer p_unknown = TestUtils::GenerateRandomPeer(0, false);

  string EXPECTED_MODE = "SHRD";
  string mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(
      mode == EXPECTED_MODE,
      "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSBU";
  size_t dsc_size = m->m_DSCommittee->size();
  size_t pair_i = TestUtils::RandomIntInRng<size_t>(1, dsc_size - 1);
  (*m->m_DSCommittee)[pair_i].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(
      mode == EXPECTED_MODE,
      "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSLD";
  (*m->m_DSCommittee)[0].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(
      mode == EXPECTED_MODE,
      "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);
}

BOOST_AUTO_TEST_CASE(GetShardSize) {
  DirectoryService ds(*m);
  ds.m_shards = TestUtils::GenerateDequeueOfShard(10);
  m->m_ds = &ds;

  uint32_t EXPECTED_SHARDSIZE = 651;
  uint32_t shardsize = m->GetShardSize(true);

  BOOST_CHECK_MESSAGE(
      std::to_string(shardsize) == std::to_string(EXPECTED_SHARDSIZE),
      "Wrong mode. Expected " + std::to_string(EXPECTED_SHARDSIZE) +
          ". Result: " + std::to_string(shardsize));
}

BOOST_AUTO_TEST_CASE(CleanUp) { delete m; }

BOOST_AUTO_TEST_SUITE_END()
