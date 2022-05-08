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

#include "libData/AccountData/AccountStoreSC.h"

AccountStoreAtomic::AccountStoreAtomic(AccountStoreSC& parent)
    : m_parent(parent) {}

Account* AccountStoreAtomic::GetAccount(const Address& address) {
  Account* account = AccountStoreBase::GetAccount(address);
  if (account != nullptr) {
    // LOG_GENERAL(INFO, "Got From Temp");
    return account;
  }

  account = m_parent.GetAccount(address);
  if (account) {
    // LOG_GENERAL(INFO, "Got From Parent");
    m_addressToAccount->insert(std::make_pair(address, *account));
    return &(m_addressToAccount->find(address))->second;
  }

  // LOG_GENERAL(INFO, "Got Nullptr");

  return nullptr;
}

const std::shared_ptr<std::unordered_map<Address, Account>>&
AccountStoreAtomic::GetAddressToAccount() {
  return this->m_addressToAccount;
}
