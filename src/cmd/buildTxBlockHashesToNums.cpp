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

#include <libData/AccountData/AccountStore.h>
#include <libData/BlockData/Block/TxBlock.h>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " PERSISTENCE_PATH" << std::endl;
    // For some reason a few symbols are missing without the invocation below,
    // leading to unresolved externals during linking stage.
    AccountStore::GetInstance();
    exit(1);
  }
  const std::string persistencePath = argv[1];
  const std::string EMPTY_SUBDIR{};

  const std::string dbCommonSubPath = persistencePath + "/";

  // Pass explicitly subdir as an empty std::string type to invoke proper ctor
  LevelDB txBlockchainDB{"txBlocks", persistencePath, EMPTY_SUBDIR};
  LevelDB txBlockchainAuxDB{"txBlocksAux", persistencePath, EMPTY_SUBDIR};
  LevelDB txBlockHashToNumDB{"txBlockHashToNum", persistencePath, EMPTY_SUBDIR};

  // Algo is as follows:
  // MaxBlockNum = 0
  // For each (blockNum, block) in txBlocks:
  //   Insert (block->GetBlockHash(), blockNum) into TxBlockHashToNum
  //   MaxBlockNum = max(MaxBlockNum, blockNum)
  // Insert ('MaxTxBlockNumber', MaxBlockNum) into txBlocksAux
  //

  uint64_t maxKnownBlockNum = 0;

  std::cerr << "Starting main loop" << std::endl;

  const auto it = std::unique_ptr<leveldb::Iterator>(
      txBlockchainDB.GetDB()->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    const uint64_t blockNum = std::stoull(it->key().ToString());
    const auto blockString = txBlockchainDB.Lookup(blockNum);
    const TxBlock block{bytes(blockString.begin(), blockString.end()), 0};
    txBlockHashToNumDB.Insert(block.GetBlockHash(), std::to_string(blockNum));
    maxKnownBlockNum = std::max(maxKnownBlockNum, blockNum);
  }

  std::cerr << "Greates block number found: " << maxKnownBlockNum << std::endl;

  constexpr auto MAX_KNOWN_BLOCK_NUM_KEY = "MaxTxBlockNumber";
  txBlockchainAuxDB.Insert(leveldb::Slice(MAX_KNOWN_BLOCK_NUM_KEY),
                           leveldb::Slice(std::to_string(maxKnownBlockNum)));

  return 0;
}
