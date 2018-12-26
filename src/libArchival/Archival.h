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