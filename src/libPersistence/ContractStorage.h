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

#ifndef CONTRACTSTORAGE_H
#define CONTRACTSTORAGE_H

#include <leveldb/db.h>

#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"

class ContractStorage : public Singleton<ContractStorage> {
  dev::OverlayDB m_stateDB;
  LevelDB m_codeDB;

  ContractStorage() : m_stateDB("contractState"), m_codeDB("contractCode"){};

  ~ContractStorage() = default;

 public:
  /// Returns the singleton ContractStorage instance.
  static ContractStorage& GetContractStorage() {
    static ContractStorage cs;
    return cs;
  }

  dev::OverlayDB& GetStateDB() { return m_stateDB; }

  /// Adds a contract code to persistence
  bool PutContractCode(const dev::h160& address, const bytes& code);

  /// Get the desired code from persistence
  const bytes GetContractCode(const dev::h160& address);
};

#endif  // CONTRACTSTORAGE_H
