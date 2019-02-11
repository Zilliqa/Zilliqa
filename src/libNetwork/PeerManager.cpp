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

#include <arpa/inet.h>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <iostream>

#include "P2PComm.h"
#include "PeerManager.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

PeerManager::PeerManager(const PairOfKey& key, const Peer& peer,
                         bool loadConfig)
    : m_selfKey(key), m_selfPeer(peer) {
  LOG_MARKER();
  SetupLogLevel();
}

PeerManager::~PeerManager() {}

bool PeerManager::Execute(const bytes& message, unsigned int offset,
                          const Peer& from) {
  LOG_MARKER();

  bool result = false;

  typedef bool (PeerManager::*InstructionHandler)(const bytes&, unsigned int,
                                                  const Peer&);

  InstructionHandler ins_handlers[] = {
      &PeerManager::ProcessAddPeer,
  };

  const unsigned char ins_byte = message.at(offset);

  const unsigned int ins_handlers_count =
      sizeof(ins_handlers) / sizeof(InstructionHandler);

  if (ins_byte < ins_handlers_count) {
    result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);

    if (!result) {
      // To-do: Error recovery
    }
  } else {
    LOG_GENERAL(WARNING, "Unknown instruction byte "
                             << std::hex << (unsigned int)ins_byte << " from "
                             << from);
    LOG_PAYLOAD(WARNING, "Unknown payload is ", message, message.size());
  }

  return result;
}

void PeerManager::SetupLogLevel() {
  LOG_MARKER();
  switch (DEBUG_LEVEL) {
    case 1: {
      LOG_DISPLAY_LEVEL_ABOVE(FATAL);
      break;
    }
    case 2: {
      LOG_DISPLAY_LEVEL_ABOVE(WARNING);
      break;
    }
    case 3: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
    case 4: {
      LOG_DISPLAY_LEVEL_ABOVE(DEBUG);
      break;
    }
    default: {
      LOG_DISPLAY_LEVEL_ABOVE(INFO);
      break;
    }
  }
}
