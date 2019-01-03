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
