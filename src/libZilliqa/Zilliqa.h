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

#include <vector>

#include "libDirectoryService/DirectoryService.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Peer.h"
#include "libNode/Node.h"
#include "libServer/LookupServer.h"
#include "libServer/StatusServer.h"
#include "libUtils/ThreadPool.h"

/// Main Zilliqa class.
class Zilliqa {
  Mediator m_mediator;
  DirectoryService m_ds;
  Lookup m_lookup;
  std::shared_ptr<ValidatorBase> m_validator;
  Node m_n;
  // ConsensusUser m_cu; // Note: This is just a test class to demo Consensus
  // usage
  boost::lockfree::queue<std::pair<bytes, Peer>*> m_msgQueue;

  std::unique_ptr<StatusServer> m_statusServer;
  std::unique_ptr<LookupServer> m_lookupServer;
  std::unique_ptr<jsonrpc::AbstractServerConnector> m_statusServerConnector;
  std::unique_ptr<jsonrpc::AbstractServerConnector> m_lookupServerConnector;

  ThreadPool m_queuePool{MAXMESSAGE, "QueuePool"};

  void ProcessMessage(std::pair<bytes, Peer>* message);

 public:
  /// Constructor.
  Zilliqa(const PairOfKey& key, const Peer& peer,
          SyncType syncType = SyncType::NO_SYNC,
          bool toRetrieveHistory = false);

  /// Destructor.
  ~Zilliqa();

  void LogSelfNodeInfo(const PairOfKey& key, const Peer& peer);

  /// Forwards an incoming message for processing by the appropriate subclass.
  void Dispatch(std::pair<bytes, Peer>* message);

  static std::string FormatMessageName(unsigned char msgType,
                                       unsigned char instruction);
};

#endif  // __ZILLIQA_H__
