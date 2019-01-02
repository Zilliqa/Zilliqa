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

#ifndef __ARCHIVAL_H__
#define __ARCHIVAL_H__

#include "common/Broadcastable.h"
#include "common/Executable.h"
#include "libArchival/BaseDB.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libLookup/Synchronizer.h"
#include "libNetwork/Peer.h"
#include "libUtils/Logger.h"

class Mediator;
class Synchronizer;

class Archival : public Executable, public Broadcastable {
  Mediator& m_mediator;
  Synchronizer m_synchronizer;

  std::mutex m_mutexMicroBlockInfo;
  std::vector<BlockHash> m_fetchMicroBlockInfo;

  std::mutex m_mutexUnfetchedTxns;
  std::set<TxnHash> m_unfetchedTxns;

 public:
  Archival(Mediator& mediator);
  ~Archival();

  void Init();
  void InitSync();
  bool Execute(const bytes& message, unsigned int offset, const Peer& from);

  bool AddToFetchMicroBlockInfo(const BlockHash& microBlockHash);
  bool RemoveFromFetchMicroBlockInfo(const BlockHash& microBlockHash);
  void SendFetchMicroBlockInfo();
  void AddToUnFetchedTxn(const std::vector<TxnHash>& txnhashes);
  void AddTxnToDB(const std::vector<TransactionWithReceipt>& txns, BaseDB& db);
  void SendFetchTxn();
};

#endif  //__ARCHIVAL_H__
