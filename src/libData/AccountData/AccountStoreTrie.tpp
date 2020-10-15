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

template <class DB, class MAP>
AccountStoreTrie<DB, MAP>::AccountStoreTrie()
    : m_db(std::is_same<DB, dev::OverlayDB>::value ? "state" : "") {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address>(&m_db);
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::Init() {
  AccountStoreSC<MAP>::Init();
  InitTrie();
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::InitTrie() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state.init();
  m_prevRoot = m_state.root();
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::Serialize(bytes& dst,
                                          unsigned int offset) const {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  if (!MessengerAccountStoreTrie::SetAccountStoreTrie(
          dst, offset, m_state, this->m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreTrie failed.");
    return false;
  }

  return true;
}

template <class DB, class MAP>
Account* AccountStoreTrie<DB, MAP>::GetAccount(const Address& address) {
  // LOG_MARKER();
  using namespace boost::multiprecision;

  Account* account = AccountStoreBase<MAP>::GetAccount(address);
  if (account != nullptr) {
    return account;
  }

  std::string rawAccountBase;
  {
    std::lock(m_mutexTrie, m_mutexDB);
    std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);

    rawAccountBase = m_state.at(address);
  }
  if (rawAccountBase.empty()) {
    return nullptr;
  }

  account = new Account();
  if (!account->DeserializeBase(
          bytes(rawAccountBase.begin(), rawAccountBase.end()), 0)) {
    LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
    delete account;
    return nullptr;
  }

  if (account->isContract()) {
    account->SetAddress(address);
  }

  auto it2 = this->m_addressToAccount->emplace(address, *account);

  delete account;

  return &it2.first->second;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrie(const Address& address,
                                                const Account& account) {
  // LOG_MARKER();
  bytes rawBytes;
  if (!account.SerializeBase(rawBytes, 0)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
    return false;
  }

  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state.insert(address, rawBytes);

  return true;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::RemoveFromTrie(const Address& address) {
  // LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexTrie);

  m_state.remove(address);

  return true;
}

template <class DB, class MAP>
dev::h256 AccountStoreTrie<DB, MAP>::GetStateRootHash() const {
  LOG_MARKER();

  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_state.root();
}

template <class DB, class MAP>
dev::h256 AccountStoreTrie<DB, MAP>::GetPrevRootHash() const {
  LOG_MARKER();

  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_prevRoot;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrieAll() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  for (auto const& entry : *(this->m_addressToAccount)) {
    bytes rawBytes;
    if (!entry.second.SerializeBase(rawBytes, 0)) {
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
