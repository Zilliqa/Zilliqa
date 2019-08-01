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
#include "libNetwork/Guard.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/Retriever.h"
#include "libUtils/UpgradeManager.h"

/// Should be run from a folder with dsnodes.xml and constants.xml and a folder
/// named "persistence" consisting of the persistence

using namespace std;
int main() {
  PairOfKey key;  // Dummy to initate mediator
  Peer peer;

  Mediator mediator(key, peer);
  Node node(mediator, 0, false);
  shared_ptr<Validator> vd;
  vd = make_shared<Validator>(mediator);
  Synchronizer sync;

  mediator.m_dsBlockChain.Reset();
  mediator.m_txBlockChain.Reset();

  sync.InitializeGenesisBlocks(mediator.m_dsBlockChain,
                               mediator.m_txBlockChain);
  const auto& dsBlock = mediator.m_dsBlockChain.GetBlock(0);
  // cout << dsBlock.GetHeader().GetBlockNum() << endl;
  {
    lock_guard<mutex> lock(mediator.m_mutexInitialDSCommittee);
    if (!UpgradeManager::GetInstance().LoadInitialDS(
            *mediator.m_initialDSCommittee)) {
      LOG_GENERAL(WARNING, "Unable to load initial DS comm");
    }
  }
  mediator.m_blocklinkchain.AddBlockLink(0, 0, BlockType::DS,
                                         dsBlock.GetBlockHash());

  if (GUARD_MODE) {
    Guard::GetInstance().Init();
  }
  mediator.RegisterColleagues(nullptr, &node, nullptr, vd.get());

  if (node.CheckIntegrity(true)) {
    cout << "Validation Success";
  } else {
    cout << "Validation Failure";
  }

  return 0;
}
