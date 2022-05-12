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

#include <type_traits>

#include "libData/AccountData/AccountStoreBase.h"
#include "libMessage/MessengerAccountStoreBase.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

void AccountStoreBase::Init() { m_addressToAccount.clear(); }

bool AccountStoreBase::Serialize(bytes& dst, unsigned int offset) const {
  if (!MessengerAccountStoreBase::SetAccountStore(dst, offset,
                                                  m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::Deserialize(const bytes& src, unsigned int offset) {
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::Deserialize(const std::string& src,
                                   unsigned int offset) {
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::IsAccountExist(const Address& address) {
  // LOG_MARKER();
  return (nullptr != GetAccount(address));
}

bool AccountStoreBase::AddAccount(const Address& address,
                                  const Account& account, bool toReplace) {
  // LOG_MARKER();
  if (toReplace || !IsAccountExist(address)) {
    m_addressToAccount[address] = account;

    return true;
  }
  LOG_GENERAL(WARNING, "Address "
                           << address
                           << " could not be added because already present");
  return false;
}

bool AccountStoreBase::AddAccount(const PubKey& pubKey,
                                  const Account& account) {
  return AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

void AccountStoreBase::RemoveAccount(const Address& address) {
  m_addressToAccount.erase(address);
}

Account* AccountStoreBase::GetAccount(const Address& address) {
  auto it = m_addressToAccount.find(address);
  if (it != m_addressToAccount.end()) {
    return &it->second;
  }
  return nullptr;
}

const Account* AccountStoreBase::GetAccount(const Address& address) const {
  auto it = m_addressToAccount.find(address);
  if (it != m_addressToAccount.end()) {
    return &it->second;
  }
  return nullptr;
}

bool AccountStoreBase::IncreaseBalance(const Address& address,
                                       const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

  if (account != nullptr && account->IncreaseBalance(delta)) {
    return true;
  }

  else if (account == nullptr) {
    return AddAccount(address, {delta, 0});
  }

  return false;
}

bool AccountStoreBase::DecreaseBalance(const Address& address,
                                       const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

  if (nullptr == account) {
    LOG_GENERAL(WARNING, "Account " << address.hex() << " not exist");
    return false;
  }

  if (!account->DecreaseBalance(delta)) {
    LOG_GENERAL(WARNING, "Failed to decrease " << delta << " for account "
                                               << address.hex());
    return false;
  }
  return true;
}

bool AccountStoreBase::TransferBalance(const Address& from, const Address& to,
                                       const uint128_t& delta) {
  // LOG_MARKER();
  // FIXME: Is there any elegent way to implement this atomic change on balance?
  if (DecreaseBalance(from, delta)) {
    if (IncreaseBalance(to, delta)) {
      return true;
    } else {
      if (!IncreaseBalance(from, delta)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for delta");
      }
    }
  }

  return false;
}

void AccountStoreBase::PrintAccountState() {
  LOG_MARKER();
  for (const auto& entry : m_addressToAccount) {
    LOG_GENERAL(INFO, entry.first << " " << entry.second);
  }
}
