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

#include "libPersistence/ContractStorage.h"

template <class DB, class MAP>
AccountStoreTrie<DB, MAP>::AccountStoreTrie()
    : m_db(std::is_same<DB, dev::OverlayDB>::value ? "state" : "") {
  m_state = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address>(&m_db);
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::Init() {
  AccountStoreSC<MAP>::Init();
  m_state.init();
  m_prevRoot = m_state.root();
}

template <class DB, class MAP>
Account* AccountStoreTrie<DB, MAP>::GetAccount(const Address& address) {
  using namespace boost::multiprecision;

  Account* account = AccountStoreBase<MAP>::GetAccount(address);
  if (account != nullptr) {
    return account;
  }

  std::string rawAccountBase = m_state.at(address);
  if (rawAccountBase.empty()) {
    return nullptr;
  }

  if (!((AccountBase)(*account))
           .Deserialize(bytes(rawAccountBase.begin(), rawAccountBase.end()),
                        0) == false) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountBase failed");
    return nullptr;
  }

  auto it2 = this->m_addressToAccount->emplace(address, *account);

  // Code Hash
  if (account->GetCodeHash() != dev::h256()) {
    dev::h256 tmpCodeHash = it2.first->second.GetCodeHash();
    // Extract Code Content
    it2.first->second.SetCode(
        Contract::ContractStorage::GetContractStorage().GetContractCode(
            address));
    if (tmpCodeHash != it2.first->second.GetCodeHash()) {
      LOG_GENERAL(WARNING, "Account Code Content doesn't match Code Hash")
      this->m_addressToAccount->erase(it2.first);
      return nullptr;
    }
    // Address
    it2.first->second.SetAddress(address);
  }

  return &it2.first->second;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrie(const Address& address,
                                                const Account& account) {
  // LOG_MARKER();
  bytes rawBytes;
  if (!((AccountBase)account).Serialize(rawBytes, 0)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
    return false;
  }

  m_state.insert(address, rawBytes);

  return true;
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::RemoveFromTrie(const Address& address) {
  // LOG_MARKER();
  m_state.remove(address);

  return true;
}

template <class DB, class MAP>
dev::h256 AccountStoreTrie<DB, MAP>::GetStateRootHash() const {
  LOG_MARKER();

  return m_state.root();
}

template <class DB, class MAP>
bool AccountStoreTrie<DB, MAP>::UpdateStateTrieAll() {
  for (auto const& entry : *(this->m_addressToAccount)) {
    if (!UpdateStateTrie(entry.first, entry.second)) {
      return false;
    }
  }

  return true;
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::RepopulateStateTrie() {
  LOG_MARKER();
  m_state.init();
  m_prevRoot = m_state.root();
  UpdateStateTrieAll();
}

template <class DB, class MAP>
void AccountStoreTrie<DB, MAP>::PrintAccountState() {
  AccountStoreBase<MAP>::PrintAccountState();
  LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}
