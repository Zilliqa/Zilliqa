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

#include "libMessage/MessengerAccountStoreTrie.h"

#include "libPersistence/BlockStorage.h"

template <class DB, class MAP>
AccountStoreTrie<DB, MAP>::AccountStoreTrie()
    : m_db(std::is_same<DB, dev::OverlayDB>::value ? "state" : "") {
  m_state = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address>(&m_db);
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::Init() {
  AccountStoreSC<MAP>::Init();
  InitTrie();
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::InitTrie() {
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  m_state.init();
  m_prevRoot = m_state.root();
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::Serialize(bytes& dst,
                                          unsigned int offset) const {
  std::shared_ptr<MAP> accs;
  std::unique_lock<std::mutex> g2(this->GetAccounts(accs));
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  if (!MessengerAccountStoreTrie::SetAccountStoreTrie(dst, offset, m_state,
                                                      *accs)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreTrie failed.");
    return false;
  }

  return true;
}

template <class DB, class MAP>
std::unique_lock<std::mutex> AccountStoreTrie<DB, MAP>::GetAccountWMutex(
    const Address& address, std::shared_ptr<Account>& acc) {
  std::unique_lock<std::mutex> g1(
      AccountStoreBase<MAP>::GetAccountWMutex(address, acc));
  if (acc != nullptr) {
    return g1;
  }
  g1.unlock();

  std::string rawAccountBase;
  {
    std::shared_lock<std::shared_timed_mutex> g2(m_mutexTrie);
    rawAccountBase = m_state.at(address);
  }
  if (rawAccountBase.empty()) {
    return g1;
  }

  std::shared_ptr<Account> account = std::make_shared<Account>();
  if (!account->DeserializeBase(
          bytes(rawAccountBase.begin(), rawAccountBase.end()), 0)) {
    LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
    return g1;
  }

  if (account->isContract()) {
    account->SetAddress(address);
  }

  this->AddAccount(address, account);

  return AccountStoreBase<MAP>::GetAccountWMutex(address, acc);
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrie(
    const Address& address, const std::shared_ptr<Account>& account) {
  // LOG_MARKER();
  bytes rawBytes;
  if (!account->SerializeBase(rawBytes, 0)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
    return false;
  }

  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  m_state.insert(address, rawBytes);

  return true;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::RemoveFromTrie(const Address& address) {
  // LOG_MARKER();
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);

  m_state.remove(address);

  return true;
}

template <class DB, class MAP>
dev::h256 AccountStoreTrie<DB, MAP>::GetStateRootHash() const {
  LOG_MARKER();

  std::shared_lock<std::shared_timed_mutex> g(m_mutexTrie);

  return m_state.root();
}

template <class DB, class MAP>
dev::h256 AccountStoreTrie<DB, MAP>::GetPrevRootHash() const {
  LOG_MARKER();

  std::shared_lock<std::shared_timed_mutex> g(m_mutexTrie);

  return m_prevRoot;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrieAll() {
  std::shared_ptr<MAP> accs;
  std::unique_lock<std::mutex> g2(this->GetAccounts(accs));
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  for (auto const& entry : *accs) {
    bytes rawBytes;
    if (!entry.second->SerializeBase(rawBytes, 0)) {
      LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
      return false;
    }
    m_state.insert(entry.first, rawBytes);
  }

  return true;
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::PrintAccountState() {
  AccountStoreBase<MAP>::PrintAccountState();
  LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::RefreshDB() {
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  return m_db.RefreshDB();
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::ResetDB() {
  std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
  m_db.ResetDB();
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::MoveUpdatesToDisk() {
  LOG_MARKER();

  try {
    std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
    if (!m_state.db()->commit()) {
      LOG_GENERAL(WARNING, "LevelDB commit failed");
    }
    if (!MoveRootToDisk(m_state.root())) {
      LOG_GENERAL(WARNING, "MoveRootToDisk failed " << m_state.root().hex());
      return false;
    }
    m_prevRoot = m_state.root();
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::MoveUpdatesToDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }

  AccountStoreSC<MAP>::Init();

  return true;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::MoveRootToDisk([
    [gnu::unused]] const dev::h256& root) {
  // convert h256 to bytes
  if (!BlockStorage::GetBlockStorage().PutStateRoot(root.asBytes())) {
    LOG_GENERAL(INFO, "FAIL: Put state root failed " << root.hex());
    return false;
  }
  return true;
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::DiscardUnsavedUpdates() {
  LOG_MARKER();

  try {
    {
      std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
      m_state.db()->rollback();
      m_state.setRoot(m_prevRoot);
    }
    AccountStoreSC<MAP>::Init();
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::DiscardUnsavedUpdates. "
                             << boost::diagnostic_information(e));
  }
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::RetrieveFromDisk() {
  bytes rootBytes;
  if (!BlockStorage::GetBlockStorage().GetStateRoot(rootBytes)) {
    // To support backward compatibilty - lookup with new binary trying to
    // recover from old database
    if (BlockStorage::GetBlockStorage().GetMetadata(STATEROOT, rootBytes)) {
      if (!BlockStorage::GetBlockStorage().PutStateRoot(rootBytes)) {
        LOG_GENERAL(WARNING,
                    "BlockStorage::PutStateRoot failed "
                        << DataConversion::CharArrayToString(rootBytes));
        return false;
      }
    } else {
      LOG_GENERAL(WARNING, "Failed to retrieve StateRoot from disk");
      return false;
    }
  }

  try {
    dev::h256 root(rootBytes);
    LOG_GENERAL(INFO, "StateRootHash:" << root.hex());
    std::unique_lock<std::shared_timed_mutex> g(m_mutexTrie);
    m_state.setRoot(root);
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::RetrieveFromDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }
  return true;
}