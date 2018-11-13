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
#include "common/Messages.h"
#include "libCrypto/Schnorr.h"
#include "libRumorSpreading/MemberID.h"
#include "libRumorSpreading/Message.h"
#include "libRumorSpreading/NetworkConfig.h"
#include "libRumorSpreading/RumorHolder.h"
#include "libRumorSpreading/RumorSpreadingInterface.h"
#include "libRumorSpreading/RumorStateMachine.h"
#include "libTestUtils/TestUtils.h"

#define BOOST_TEST_MODULE RumorSpreading
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(RumorSpreadingTestSuite)

/**
 * \brief Rumor Spreading test initialization
 *
 * \details
 */
BOOST_AUTO_TEST_CASE(RRS_Initialization) {
  /// Test initialization
  std::unordered_set<int> dummy_peerIdSet;
  for (int i = 0; i < 16; i++) {
    dummy_peerIdSet.insert(i);
  }
  // Copy constructor check
  RRS::RumorHolder dummy_holder(dummy_peerIdSet, 2);
  RRS::RumorHolder dummy_holder_copy(dummy_holder);
  /// Rumor Holder == operator check
  BOOST_CHECK(dummy_holder == dummy_holder_copy);

  /// NetworkConfig
  RRS::NetworkConfig dummy_networkconfig(16);
  BOOST_TEST_MESSAGE("Rumor Spreading max rounds total: "
                     << dummy_networkconfig.maxRoundsTotal());
  BOOST_TEST_MESSAGE("Rumor Spreading max rounds in B phase: "
                     << dummy_networkconfig.maxRoundsInB());
  BOOST_TEST_MESSAGE("Rumor Spreading max rounds in C phase: "
                     << dummy_networkconfig.maxRoundsInC());
  // Messaging
  RRS::Message dummy_message_undefined(RRS::Message::Type::UNDEFINED, 0, 0);
  RRS::Message dummy_message_push(RRS::Message::Type::PUSH, 0, 0);
  // Test comparison and stream oeprator
  BOOST_CHECK(dummy_message_push != dummy_message_undefined);
  BOOST_CHECK(dummy_message_push == dummy_message_push);
  BOOST_TEST_MESSAGE("RRS Message undefined: " << dummy_message_undefined);
}
BOOST_AUTO_TEST_SUITE_END()
