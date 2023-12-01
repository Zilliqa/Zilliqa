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

#include <common/Constants.h>
#include <depends/libDatabase/LevelDB.h>
#include <libBlockchain/TxBlock.h>

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " PERSISTENCE_PATH" << " DB_NAME" << std::endl;
    exit(1);
  }
  const std::string persistencePath = argv[1];
  const std::string dbName = argv[2];
  const std::string EMPTY_SUBDIR{};

  // Pass explicitly subdir as an empty std::string type to invoke proper ctor
  LevelDB stateDelta{dbName, persistencePath, EMPTY_SUBDIR};

  if(stateDelta.GetDB() == nullptr) {
    std::cerr << "Unable to open provided db..." << std::endl;
    exit(1);
  }
  std::cerr << "Starting compact action on db: " <<  dbName << std::endl;

  stateDelta.GetDB()->CompactRange(nullptr, nullptr);

  return 0;
}
