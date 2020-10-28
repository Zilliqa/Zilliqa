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

template <class MAP>
AccountStoreAtomic<MAP>::AccountStoreAtomic(AccountStoreSC<MAP>& parent)
    : m_parent(parent) {}

template <class MAP>
std::unique_lock<std::mutex> AccountStoreAtomic<MAP>::GetAccountWMutex(
    const Address& address, std::shared_ptr<Account>& acc) {
  LOG_MARKER();

  std::unique_lock<std::mutex> g1(
      AccountStoreBase<std::unordered_map<Address, std::shared_ptr<Account>>>::
          GetAccountWMutex(address, acc));
  if (acc != nullptr) {
    return g1;
  }

  {
    std::unique_lock<std::mutex> g2(m_parent.GetAccountWMutex(address, acc));
    if (acc) {
      g1.unlock();
      this->AddAccount(address, std::make_shared<Account>(*acc));
      g2.unlock();
      return AccountStoreBase<std::unordered_map<
          Address, std::shared_ptr<Account>>>::GetAccountWMutex(address, acc);
    }
  }

  return g1;
}

// template <class MAP>
// const std::shared_ptr<std::unordered_map<Address, Account>>&
// AccountStoreAtomic<MAP>::GetAddressToAccount() {
//   return this->m_addressToAccount;
// }
