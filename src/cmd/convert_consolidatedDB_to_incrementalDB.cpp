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

#include "libMediator/Mediator.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/IncrementalDB.h"
#include "libPersistence/Retriever.h"

using namespace std;

int main() {
  IncrementalDB::GetInstance().Init();

  cout << "This file needs a valid persistence folder in the working directory "
          "and also a constant file corresponding to it"
       << endl;

  PairOfKey key;  // Dummy to initate mediator
  Peer peer;

  Mediator mediator(key, peer);
  Retriever retriever(mediator);
  Node node(mediator, 0, false);

  mediator.RegisterColleagues(nullptr, &node, nullptr, nullptr);

  retriever.RetrieveBlockLink(false, true);
  retriever.RetrieveTxBlocks(false, true);
  // Put MicroBlock and Txn Hashes
  const auto& latestTxBlockNum =
      mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  // Expects every tx block to be present in level DB
  for (uint i = 1; i <= latestTxBlockNum; i++) {
    const auto& txblock = mediator.m_txBlockChain.GetBlock(i);
    if (INIT_BLOCK_NUMBER == txblock.GetHeader().GetBlockNum()) {
      cout << "Failed to get tx block " << i;
      return 1;
    }
    const auto& dsEpoch = txblock.GetHeader().GetDSBlockNum();
    for (const auto& mbInfo : txblock.GetMicroBlockInfos()) {
      if (mbInfo.m_txnRootHash == TxnHash()) {
        continue;
      }
      MicroBlockSharedPtr microblock;
      if (!BlockStorage::GetBlockStorage().GetMicroBlock(
              mbInfo.m_microBlockHash, microblock)) {
        cout << "Could not get " << mbInfo.m_microBlockHash;
        return 1;
      }
      bytes tmpMB;
      microblock->Serialize(tmpMB, 0);
      IncrementalDB::GetInstance().PutMicroBlock(mbInfo.m_microBlockHash, tmpMB,
                                                 dsEpoch);

      for (const auto& txnHash : microblock->GetTranHashes()) {
        TxBodySharedPtr tptr;
        if (BlockStorage::GetBlockStorage().GetTxBody(txnHash, tptr)) {
          cout << "Could not get " << txnHash;
          return 1;
        }
        bytes tmpTxn;
        tptr->Serialize(tmpTxn, 0);
        IncrementalDB::GetInstance().PutTxBody(txnHash, tmpTxn, dsEpoch);
      }
    }
  }

  // Assuming the state in storage is latest
  retriever.RetrieveStates();
  if (!retriever.ValidateStates()) {
    cout << "Failed to ValidateStates" << endl;
    return 1;
  }

  bytes tmpState;
  AccountStore::GetInstance().Serialize(tmpState, 0);

  IncrementalDB::GetInstance().PutBaseState(latestTxBlockNum, tmpState);

  cout << "Conversion from consildated DB to IncrementalDB success";
  return 0;
}
