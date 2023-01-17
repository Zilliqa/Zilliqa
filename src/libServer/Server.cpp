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

#include "Server.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNode/Node.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace jsonrpc;
using namespace std;

Server::~Server() {
  // destructor
}

string Server::GetCurrentMiniEpoch() {
  LOG_MARKER();

  return to_string(m_mediator.m_currentEpochNum);
}

string Server::GetCurrentDSEpoch() {
  LOG_MARKER();

  return to_string(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum());
}

string Server::GetNodeType() {
  if (!m_mediator.m_lookup->AlreadyJoinedNetwork()) {
    return "Not in network, synced till epoch " +
           to_string(m_mediator.m_currentEpochNum);
  } else if (LOOKUP_NODE_MODE && ARCHIVAL_LOOKUP) {
    return "Seed";
  } else if (LOOKUP_NODE_MODE) {
    return "Lookup";
  } else if (m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE) {
    return "DS Node";
  } else {
    return string("Shard Node of shard ") +
           to_string(m_mediator.m_node->GetShardId());
  }
}

uint8_t Server::GetPrevDSDifficulty() {
  return m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
}

uint8_t Server::GetPrevDifficulty() {
  return m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
}
