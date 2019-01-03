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

#ifndef __ZILLIQA_H__
#define __ZILLIQA_H__

#include <jsonrpccpp/server/connectors/httpserver.h>
#include <vector>

#include "libArchival/Archival.h"
#include "libArchival/ArchiveDB.h"
#include "libConsensus/ConsensusUser.h"
#include "libDirectoryService/DirectoryService.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Peer.h"
#include "libNetwork/PeerManager.h"
#include "libNetwork/PeerStore.h"
#include "libNode/Node.h"
#include "libServer/Server.h"
#include "libUtils/ThreadPool.h"

/// Main Zilliqa class.
class Zilliqa {
  PeerManager m_pm;
  Mediator m_mediator;
  DirectoryService m_ds;
  Lookup m_lookup;
  std::shared_ptr<ValidatorBase> m_validator;
  Node m_n;
  ArchiveDB m_db;
  Archival m_arch;
  // ConsensusUser m_cu; // Note: This is just a test class to demo Consensus
  // usage
  boost::lockfree::queue<std::pair<bytes, Peer>*> m_msgQueue;

  jsonrpc::HttpServer m_httpserver;
  Server m_server;

  ThreadPool m_queuePool{MAXMESSAGE, "QueuePool"};

  void ProcessMessage(std::pair<bytes, Peer>* message);

 public:
  /// Constructor.
  Zilliqa(const std::pair<PrivKey, PubKey>& key, const Peer& peer,
          bool loadConfig, unsigned int syncType = SyncType::NO_SYNC,
          bool toRetrieveHistory = false);

  /// Destructor.
  ~Zilliqa();

  void LogSelfNodeInfo(const std::pair<PrivKey, PubKey>& key, const Peer& peer);

  /// Forwards an incoming message for processing by the appropriate subclass.
  void Dispatch(std::pair<bytes, Peer>* message);

  /// Returns a list of broadcast peers based on the specified message and
  /// instruction types.
  std::vector<Peer> RetrieveBroadcastList(unsigned char msg_type,
                                          unsigned char ins_type,
                                          const Peer& from);

  static std::string FormatMessageName(unsigned char msgType,
                                       unsigned char instruction);
};

#endif  // __ZILLIQA_H__
