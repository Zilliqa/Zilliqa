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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORETRIE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORETRIE_H_

#include "AccountStoreSC.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libDatabase/OverlayDB.h"

template <class DB, class MAP>
class AccountStoreTrie : public AccountStoreSC<MAP> {
  /// Store the trie root to leveldb
  bool MoveRootToDisk(const dev::h256& root);

 protected:
  DB m_db;
  dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address> m_state;
  dev::h256 m_prevRoot{dev::h256()};

  // mutex for Trie related operations
  mutable std::shared_timed_mutex m_mutexTrie;

  AccountStoreTrie();

  bool UpdateStateTrie(const Address& address,
                       const std::shared_ptr<Account>& account);
  bool RemoveFromTrie(const Address& address);

  /// repopulate the in-memory data structures from persistent storage
  bool RetrieveFromDisk();

  /// commit the in-memory states into persistent storage
  bool MoveUpdatesToDisk();

 public:
  virtual void Init() override;

  void InitTrie();

  bool Serialize(bytes& dst, unsigned int offset) const override;

  std::unique_lock<std::mutex> GetAccountWMutex(
      const Address& address, std::shared_ptr<Account>& acc) override;

  dev::h256 GetStateRootHash() const;
  dev::h256 GetPrevRootHash() const;
  bool UpdateStateTrieAll();

  void PrintAccountState() override;

  bool RefreshDB();

  void ResetDB();

  /// discard all the changes in memory and reset the states from last
  /// checkpoint in persistent storage
  void DiscardUnsavedUpdates();
};

#include "AccountStoreTrie.tpp"

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORETRIE_H_
