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

#include "JSONConversion.h"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <iostream>
#include "depends/jsonrpc/include/jsonrpccpp/server.h"

#include "Server.h"
#include "common/MempoolEnum.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace jsonrpc;
using namespace std;

Server::~Server(){
    // destructor
};

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
    return "Not in network";
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
