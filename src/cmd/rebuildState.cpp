/*
 * Copyright (C) 2023 Zilliqa
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

int findMaxTxBlock(const LevelDB& txBlockchainDB) {
  uint32_t left = 0;
  uint32_t right = std::numeric_limits<uint32_t>::max();
  // main code goes as follows
  while (left <= right) {
    uint32_t mid = left + (right - left) / 2;

    // Found!
    if (!txBlockchainDB.Lookup(mid).empty() &&
        txBlockchainDB.Lookup(mid + 1).empty()) {
      return mid;
    }
    if (!txBlockchainDB.Lookup(mid).empty()) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  return left;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " NUM_OF_BLOCKS_TO_KEEP_STATE"
              << std::endl;
    exit(1);
  }
  const auto blocksNum = std::atoi(argv[1]);

  LOOKUP_NODE_MODE = true;

  LevelDB txBlockchainDB{"txBlocks"};

  const auto startTime = std::chrono::system_clock::now();

  auto latestBlockNum = findMaxTxBlock(txBlockchainDB);

  std::cerr << "Max block found: " << latestBlockNum << std::endl;

  std::vector<std::pair<dev::h256, uint32_t>> visitedHashes;
  visitedHashes.reserve(blocksNum);

  {
    dev::OverlayDB fullStateDb{"state"};
    dev::GenericTrieDB fullState{&fullStateDb};

    const auto startBlock =
        (blocksNum > latestBlockNum + 1) ? 0 : (latestBlockNum - blocksNum + 1);

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
      if (idx % 10 == 0) {
        std::cerr << "Processing block num: " << idx << std::endl;
      }
      const auto currStateHash = block.GetHeader().GetStateRootHash();
      try {
        {
          LevelDB level_db{"state_slim"};
          level_db.compact();
        }
        fullState.setRoot(currStateHash);
        visitedHashes.push_back({currStateHash, 0});

        dev::OverlayDB slimStateDb{"state_slim"};
        dev::GenericTrieDB slimState{&slimStateDb};
        slimState.init();

        for (auto iter = fullState.begin(); iter != fullState.end(); ++iter) {
          const auto [key, val] = iter.at();
          slimState.insert(key, val);
          visitedHashes.back().second++;

          if (visitedHashes.back().second % 50000 == 0) {
            std::cerr << "Processed: " << visitedHashes.back().second
                      << " entries from block: " << idx << std::endl;
          }
        }
        slimStateDb.commit();

      } catch (std::exception& e) {
        std::cerr << "Unable to set trie at given hash from blockNum: " << idx
                  << std::endl;
        std::cerr << "Hash saved in txBlock: " << idx
                  << " may not be valid!. Exiting..." << std::endl;
        exit(1);
      }
    }
  }
  {
    std::cerr << "Rebuilding done. Doing validation" << std::endl;
    dev::OverlayDB slimStateDb{"state_slim"};
    dev::GenericTrieDB slimState{&slimStateDb};
    for (const auto& [hash, count] : visitedHashes) {
      try {
        slimState.setRoot(hash);
        uint32_t slimCount = 0;
        for (auto it = slimState.begin(); it != slimState.end(); ++it) {
          slimCount++;
        }
        if (slimCount != count) {
          std::cerr
              << "Invalid number of entries between two states, state has: "
              << count << ", but slim state has: " << slimCount << std::endl;
          std::cerr << "This is inconsistency, exiting...";
          exit(1);
        }
        std::cerr << "Validated one block " << std::endl;
      } catch (std::exception& e) {
        std::cerr << "Unable to verify correctness of slim state trie. Cannot "
                     "set root at hash: "
                  << hash << ", exception: " << e.what() << std::endl;
        std::cerr << "Please revisit correctness of this program or if given "
                     "full state is not corrupted!"
                  << std::endl;
        exit(1);
      }
    }
  }

  const auto stopTime = std::chrono::system_clock::now();
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            stopTime - startTime)
                            .count();
  std::cerr << "All done. It has taken: " << duration << "[ms]. "
            << "Looks we're ready to use the slim version now." << std::endl;

  return 0;
}