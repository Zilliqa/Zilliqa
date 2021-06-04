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

template <class MAP>
AccountStoreTrie<MAP>::AccountStoreTrie() : m_db("state"), m_state(&m_db) {}

template <class MAP>
void AccountStoreTrie<MAP>::Init() {
  AccountStoreSC<MAP>::Init();
  InitTrie();
}

template <class MAP>
void AccountStoreTrie<MAP>::InitTrie() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state.init();
  m_prevRoot = m_state.root();
}

template <class MAP>
bool AccountStoreTrie<MAP>::Serialize(bytes& dst, unsigned int offset) {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  if (LOOKUP_NODE_MODE) {
    if (m_prevRoot != dev::h256()) {
      try {
        m_state.setRoot(m_prevRoot);
      } catch (...) {
        return false;
      }
    }
  }
  if (!MessengerAccountStoreTrie::SetAccountStoreTrie(
          dst, offset, m_state, this->m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreTrie failed.");
    return false;
  }

  return true;
}

template <class MAP>
Account* AccountStoreTrie<MAP>::GetAccount(const Address& address,
                                           const dev::h256& rootHash,
                                           bool resetRoot) {
  // LOG_MARKER();
  using namespace boost::multiprecision;

  Account* account = AccountStoreBase<MAP>::GetAccount(address);
  if (account != nullptr) {
    return account;
  }

  std::string rawAccountBase;

  dev::h256 t_rootHash = rootHash;

  if (LOOKUP_NODE_MODE && rootHash == dev::h256()) {
    t_rootHash = m_prevRoot;
  }

  {
    std::lock(m_mutexTrie, m_mutexDB);
    std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);

    if (LOOKUP_NODE_MODE && resetRoot) {
      if (t_rootHash != dev::h256()) {
        try {
          m_state.setRoot(t_rootHash);
        } catch (...) {
          LOG_GENERAL(WARNING, "setRoot for " << t_rootHash.hex() << " failed");
          return nullptr;
        }
      }
    }

    rawAccountBase =
        m_state.at(DataConversion::StringToCharArray(address.hex()));
  }
  if (rawAccountBase.empty()) {
    LOG_GENERAL(WARNING, "rawAccountBase is empty");
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

template <class MAP>
bool AccountStoreTrie<MAP>::GetProof(const Address& address,
                                     const dev::h256& rootHash,
                                     Account& account,
                                     std::set<std::string>& nodes) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "not lookup node");
    return false;
  }

  std::string rawAccountBase;

  dev::h256 t_rootHash = (rootHash == dev::h256()) ? m_prevRoot : rootHash;

  {
    std::lock(m_mutexTrie, m_mutexDB);
    std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);

    if (t_rootHash != dev::h256()) {
      try {
        m_state.setRoot(t_rootHash);
      } catch (...) {
        LOG_GENERAL(WARNING, "setRoot for " << t_rootHash.hex() << " failed");
        return false;
      }
    }

    rawAccountBase = m_state.getProof(
        DataConversion::StringToCharArray(address.hex()), nodes);
  }

  if (rawAccountBase.empty()) {
    return false;
  }

  Account t_account;
  if (!t_account.DeserializeBase(
          bytes(rawAccountBase.begin(), rawAccountBase.end()), 0)) {
    LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
    return false;
  }

  if (t_account.isContract()) {
    t_account.SetAddress(address);
  }

  account = std::move(t_account);

  return true;
}

template <class MAP>
bool AccountStoreTrie<MAP>::UpdateStateTrie(const Address& address,
                                            const Account& account) {
  bytes rawBytes;
  if (!account.SerializeBase(rawBytes, 0)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
    return false;
  }

  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state.insert(DataConversion::StringToCharArray(address.hex()), rawBytes);

  return true;
}

template <class MAP>
bool AccountStoreTrie<MAP>::RemoveFromTrie(const Address& address) {
  // LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexTrie);

  m_state.remove(DataConversion::StringToCharArray(address.hex()));

  return true;
}

template <class MAP>
dev::h256 AccountStoreTrie<MAP>::GetStateRootHash() const {
  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_state.root();
}

template <class MAP>
dev::h256 AccountStoreTrie<MAP>::GetPrevRootHash() const {
  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_prevRoot;
}

template <class MAP>
bool AccountStoreTrie<MAP>::UpdateStateTrieAll() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  if (m_prevRoot != dev::h256()) {
    try {
      m_state.setRoot(m_prevRoot);
    } catch (...) {
      LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
      return false;
    }
  }
  for (auto const& entry : *(this->m_addressToAccount)) {
    bytes rawBytes;
    if (!entry.second.SerializeBase(rawBytes, 0)) {
      LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
      return false;
    }
    m_state.insert(DataConversion::StringToCharArray(entry.first.hex()),
                   rawBytes);
  }

  m_prevRoot = m_state.root();

  return true;
}

template <class MAP>
void AccountStoreTrie<MAP>::PrintAccountState() {
  AccountStoreBase<MAP>::PrintAccountState();
  LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}

template <class MAP>
void AccountStoreTrie<MAP>::PrintTrie() {
  if (LOOKUP_NODE_MODE) {
    std::lock_guard<std::mutex> g(m_mutexTrie);
    if (m_prevRoot != dev::h256()) {
      try {
        LOG_GENERAL(INFO, "prevRoot: " << m_prevRoot.hex());
        m_state.setRoot(m_prevRoot);
      } catch (...) {
        LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
        return;
      }
    }
  }

  LOG_GENERAL(INFO, "setRoot finished");

  for (const auto& i : m_state) {
    Address address(i.first);

    LOG_GENERAL(INFO, "Address: " << address.hex());

    AccountBase ab;
    if (!ab.Deserialize(bytes(i.second.begin(), i.second.end()), 0)) {
      LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
      return;
    }

    LOG_GENERAL(INFO, "Address: " << address.hex() << " AccountBase: " << ab);
  }
}