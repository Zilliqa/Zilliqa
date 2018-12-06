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
#include "common/MessageNames.h"
#include "libUtils/Logger.h"
#include "libZilliqa/Zilliqa.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(message_name_test)

BOOST_AUTO_TEST_CASE(init) { INIT_STDOUT_LOGGER(); }

BOOST_AUTO_TEST_CASE(test_message_name) {
  {
    MessageType msgType = MessageType::DIRECTORY;
    DSInstructionType instruction = DSInstructionType::DSBLOCKCONSENSUS;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "DS_DSBLOCKCONSENSUS");
  }

  {
    MessageType msgType = MessageType::LOOKUP;
    LookupInstructionType instruction =
        LookupInstructionType::VCGETLATESTDSTXBLOCK;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "LOOKUP_VCGETLATESTDSTXBLOCK");
  }

  {
    MessageType msgType = MessageType::NODE;
    NodeInstructionType instruction = NodeInstructionType::PROPOSEGASPRICE;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "NODE_PROPOSEGASPRICE");
  }

  {
    MessageType msgType = MessageType::PEER;
    auto instruction = 0;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "INVALID_MESSAGE");
  }

  {
    MessageType msgType = CONSENSUSUSER;
    auto instruction = 0;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "INVALID_MESSAGE");
  }

  {
    MessageType msgType = CONSENSUSUSER;
    auto instruction = -3;
    auto messageName = Zilliqa::FormatMessageName(msgType, instruction);
    BOOST_REQUIRE(messageName == "INVALID_MESSAGE");
  }
}

BOOST_AUTO_TEST_SUITE_END()
