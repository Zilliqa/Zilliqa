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

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Messages.h"
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
  const RRS::RumorHolder& dummy_holder_copy(dummy_holder);
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
