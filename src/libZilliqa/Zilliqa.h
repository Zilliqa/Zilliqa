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

#ifndef ZILLIQA_SRC_LIBZILLIQA_ZILLIQA_H_
#define ZILLIQA_SRC_LIBZILLIQA_ZILLIQA_H_

#include <memory>
#include <vector>

#include "libDirectoryService/DirectoryService.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libMetrics/Api.h"
#include "libNetwork/P2PMessage.h"
#include "libNetwork/Peer.h"
#include "libNode/Node.h"
#include "libServer/LookupServer.h"
#include "libServer/StakingServer.h"
#include "libServer/StatusServer.h"
#include "libUtils/Queue.h"

#include <boost/thread/executors/basic_thread_pool.hpp>

/// Main Zilliqa class.
class Zilliqa {
 public:
  // TODO shared instead of unique due to lambda move capture limitations
  using Msg = std::shared_ptr<zil::p2p::Message>;

 private:
  Mediator m_mediator;
  DirectoryService m_ds;
  Lookup m_lookup;
  std::shared_ptr<Validator> m_validator;
  Node m_n;
  // ConsensusUser m_cu; // Note: This is just a test class to demo Consensus
  // usage

  utility::Queue<Msg> m_msgQueue;

  std::shared_ptr<LookupServer> m_lookupServer;
  std::shared_ptr<StakingServer> m_stakingServer;
  std::unique_ptr<StatusServer> m_statusServer;
  std::unique_ptr<jsonrpc::AbstractServerConnector> m_lookupServerConnector;
  std::unique_ptr<jsonrpc::AbstractServerConnector> m_stakingServerConnector;
  std::unique_ptr<jsonrpc::AbstractServerConnector> m_statusServerConnector;

  Z_I64GAUGE m_msgQueueSize{zil::metrics::FilterClass::MSG_DISPATCH,
                            "msg.dispatch.queue_size",
                            "Incoming P2P message queue size", "bytes", true};
  boost::executors::basic_thread_pool m_queuePool{MAXRECVMESSAGE};
  // ThreadPool m_queuePool{MAXRECVMESSAGE, "QueuePool"};

  void ProcessMessage(Msg& message);

 public:
  /// Constructor.
  Zilliqa(const PairOfKey& key, const Peer& peer,
          SyncType syncType = SyncType::NO_SYNC, bool toRetrieveHistory = false,
          bool multiplierSyncMode = true,
          PairOfKey extSeedKey = PairOfKey(PrivKey(), PubKey()));

  /// Destructor.
  ~Zilliqa();

  // For pending txn update workaround
  Mediator& GetMediator() { return m_mediator; }

  void LogSelfNodeInfo(const PairOfKey& key, const Peer& peer);

  /// Forwards an incoming message for processing by the appropriate subclass.
  void Dispatch(Msg message);

  static std::string FormatMessageName(unsigned char msgType,
                                       unsigned char instruction);
};

#endif  // ZILLIQA_SRC_LIBZILLIQA_ZILLIQA_H_
