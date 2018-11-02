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
#include "libMediator/Mediator.h"
#include "libUtils/Logger.h"
#include "libTestUtils/TestUtils.h"
#include "libDB/ArchiveDB.h"
#include "libDB/Archival.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(mediator_test)

Mediator* m;
KeyPair kp;
Peer p;


BOOST_AUTO_TEST_CASE(init) {
  INIT_STDOUT_LOGGER();
  rng.seed(std::random_device()());
  kp = TestUtils::GenerateRandomKeyPair();
  p = Peer();
  m = new Mediator (kp, p);
}

/*TODO: Decision to let it as testcase
 *We can test of the member equals to the initializers
 */
BOOST_AUTO_TEST_CASE(test_RegisterColleagues) {
  DirectoryService ds(*m);
  Node node(*m, 0, false);
  Lookup lookup(*m);
  Validator validator(*m);
  ArchiveDB archDB("name", "txn", "txBlock", "dsBlock", "accountState");
  Archival arch(*m);

  m->RegisterColleagues(&ds, &node, &lookup, &validator, &archDB, &arch);
}

/*TODO: Decision to let it as testcase
 *Possible testing of the member m_dsBlockRand
 */
BOOST_AUTO_TEST_CASE(UpdateDSBlockRand) {
  m->UpdateDSBlockRand(false);
  m->UpdateDSBlockRand(true);
}

/*TODO: Decision to let it as testcase
 *Possible testing of the member m_dsBlockRand
 */
BOOST_AUTO_TEST_CASE(UpdateTxBlockRand) {
  m->UpdateTxBlockRand();
  m->UpdateDSBlockRand(true);
}

void replacePeerDSCommittee(TestUtils::DS_Comitte_t& dsCommittee, Peer& p){
  size_t dsc_size = dsCommittee.size();
  size_t peer_i = TestUtils::RandomIntInRng<size_t>(0, dsc_size - 1);
  dsCommittee[peer_i].second = p;
}

BOOST_AUTO_TEST_CASE(GetNodeMode) {
  uint32_t size = TestUtils::RandomIntInRng<uint32_t>(2,100);
  for (uint32_t i = 1; i <= size; i++){
    m->m_DSCommittee->push_front(std::make_pair(TestUtils::GenerateRandomPubKey(), TestUtils::GenerateRandomPeer(0,true)));
  }

  Peer p_unknown = TestUtils::GenerateRandomPeer(0,false);

  string EXPECTED_MODE = "SHRD";
  string mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSBU";
  size_t dsc_size = m->m_DSCommittee->size();
  size_t pair_i = TestUtils::RandomIntInRng<size_t>(1, dsc_size - 1);
  (*m->m_DSCommittee)[pair_i].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSLD";
  (*m->m_DSCommittee)[0].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);
}

BOOST_AUTO_TEST_CASE(GetShardSize) {
  Directory




  uint32_t size = TestUtils::RandomIntInRng<uint32_t>(2,100);
  for (uint32_t i = 1; i <= size; i++){
    m->m_DSCommittee->push_front(std::make_pair(TestUtils::GenerateRandomPubKey(), TestUtils::GenerateRandomPeer(0,true)));
  }

  Peer p_unknown = TestUtils::GenerateRandomPeer(0,false);

  string EXPECTED_MODE = "SHRD";
  string mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSBU";
  size_t dsc_size = m->m_DSCommittee->size();
  size_t pair_i = TestUtils::RandomIntInRng<size_t>(1, dsc_size - 1);
  (*m->m_DSCommittee)[pair_i].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);

  EXPECTED_MODE = "DSLD";
  (*m->m_DSCommittee)[0].second = p_unknown;
  mode = m->GetNodeMode(p_unknown);
  BOOST_CHECK_MESSAGE(mode == EXPECTED_MODE, "Wrong mode. Expected " + EXPECTED_MODE + ". Result: " + mode);
}




BOOST_AUTO_TEST_SUITE_END()
