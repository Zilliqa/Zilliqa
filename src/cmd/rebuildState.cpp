/*
 * Copyright (C) 2022 Zilliqa
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

#include <iostream>

#include <depends/libDatabase/LevelDB.h>
#include <libBlockchain/TxBlock.h>
#include <libData/AccountStore/AccountStore.h>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " LATEST_TX_BLOCK_NUM"
              << " NUM_OF_BLOCKS_TO_KEEP_STATE" << std::endl;
    exit(1);
  }
  auto latestBlockNum = std::atoi(argv[1]);
  const auto blocksNum = std::atoi(argv[2]);

  LevelDB txBlockchainDB{"txBlocks"};

  // Find if given block num is actually the highest

  for (;;) {
    const auto blockString = txBlockchainDB.Lookup(latestBlockNum);
    if (std::empty(blockString)) {
      latestBlockNum--;
      break;
    }
    latestBlockNum++;
  }

  const auto startBlock =
      (blocksNum > latestBlockNum + 1) ? 0 : (latestBlockNum - blocksNum + 1);

  {
    dev::OverlayDB fullStateDb{"state"};
    dev::GenericTrieDB fullState{&fullStateDb};

    dev::OverlayDB slimStateDb{"slim_state"};
    dev::GenericTrieDB slimState{&slimStateDb};
    slimState.init();

    for (auto idx = startBlock; idx <= latestBlockNum; ++idx) {
      const auto blockString = txBlockchainDB.Lookup(idx);
      if (std::empty(blockString)) {
        std::cerr << "Unable to find txBlick with number: " << idx << std::endl;
        continue;
      }
      TxBlock block;
      if (!block.Deserialize(blockString, 0)) {
        std::cerr << "Unable to deserialize block with number: " << idx
                  << std::endl;
        continue;
      }

      std::cerr << "Processing block num: " << idx << std::endl;
      const auto currStateHash = block.GetHeader().GetStateRootHash();
      fullState.setRoot(currStateHash);

      for (auto iter = fullState.begin(); iter != fullState.end(); ++iter) {
        const auto [key, val] = iter.at();
        slimState.insert(key, val);
      }
    }

    slimStateDb.commit();
  }

  return 0;
}