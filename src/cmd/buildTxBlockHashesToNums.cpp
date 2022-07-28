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
#include <memory>

#include <libData/AccountData/AccountStore.h>
#include <libData/BlockData/Block/TxBlock.h>

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " PERSISTENCE_PATH" << std::endl;
    // For some reason a few symbols are missing without the below invocation,
    // leading to unresolved externals during linking stage.
    AccountStore::GetInstance();
    exit(1);
  }
  const std::string persistencePath = argv[1];
  const std::string EMPTY_SUBDIR{};

  const std::string dbCommonSubPath = persistencePath + "/";

  // Pass explicitly subdir as an empty std::string type to invoke proper ctor
  LevelDB txBlockchainDB{"txBlocks", persistencePath, EMPTY_SUBDIR};

  const auto it = std::unique_ptr<leveldb::Iterator>(
      txBlockchainDB.GetDB()->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    uint64_t blockNum = std::stoull(it->key().ToString());
    std::cerr << "Got blockNum: " << blockNum << std::endl;
    const auto blockString = txBlockchainDB.Lookup(blockNum);
    const TxBlock block{bytes(blockString.begin(), blockString.end()), 0};
  }

  // std::shared_ptr<LevelDB> txBlockchainAuxDB;
  // std::shared_ptr<LevelDB> txBlockHashToNumDB;

  return 0;
}