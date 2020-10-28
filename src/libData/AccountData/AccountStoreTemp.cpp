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

#include "AccountStore.h"
#include "libMessage/Messenger.h"

using namespace std;
using namespace boost::multiprecision;

AccountStoreTemp::AccountStoreTemp(AccountStore& parent) : m_parent(parent) {}

std::unique_lock<std::mutex> AccountStoreTemp::GetAccountWMutex(
    const Address& address, std::shared_ptr<Account>& acc) {
  LOG_MARKER();
  std::unique_lock<std::mutex> g1(
      AccountStoreBase<
          map<Address, std::shared_ptr<Account>>>::GetAccountWMutex(address,
                                                                    acc));
  if (acc != nullptr) {
    return g1;
  }

  {
    std::unique_lock<std::mutex> g2(m_parent.GetAccountWMutex(address, acc));
    if (acc) {
      g1.unlock();
      AddAccount(address, std::make_shared<Account>(*acc));
      g2.unlock();
      return AccountStoreBase<
          map<Address, std::shared_ptr<Account>>>::GetAccountWMutex(address,
                                                                    acc);
    }
  }

  return g1;
}

bool AccountStoreTemp::DeserializeDelta(const bytes& src, unsigned int offset) {
  LOG_MARKER();

  if (!Messenger::GetAccountStoreDelta(src, offset, *this, true)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
    return false;
  }

  return true;
}
