/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORETEMP_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORETEMP_H_

#include "AccountStore.h"
#include "AccountStoreSC.h"
#include "AccountStoreTrie.h"

class AccountStore;

class AccountStoreTemp : public AccountStoreSC {
  AccountStoreTrie& m_parent;

  friend class AccountStore;

 public:
  AccountStoreTemp(AccountStoreTrie& parent);

  bool DeserializeDelta(const zbytes& src, unsigned int offset);

  // Returns the Account associated with the specified address.
  Account* GetAccount(const Address& address) override;

  const std::shared_ptr<std::unordered_map<Address, Account>>& GetAddressToAccount() {
    return this->m_addressToAccount;
  }

  void AddAccountDuringDeserialization(const Address& address,
                                       const Account& account) {
    m_addressToAccount->insert_or_assign(address, account);
  }
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORETEMP_H_
