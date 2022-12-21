/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORECPSINTERFACE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORECPSINTERFACE_H_

#include "libData/AccountData/AccountStoreSC.h"

#include "libCps/CpsAccountStoreInterface.h"

template <typename T>
struct AccountStoreCpsInterface : public libCps::CpsAccountStoreInterface {
 public:
  explicit AccountStoreCpsInterface(AccountStoreSC<T>& acc_store)
      : m_account_store(acc_store) {}
  virtual libCps::Amount GetBalanceForAccount(const Address& account) override {
    return libCps::Amount::fromQa(m_account_store.GetBalance(account));
  }
  virtual uint64_t GetNonceForAccount(const Address& account) override {
    return m_account_store.GetNonce(account);
  }

  virtual bool AccountExists(const Address& account) override {
    return m_account_store.GetAccount(account) != nullptr;
  }
  virtual bool AddAccountAtomic(const Address& address) override {
    if (m_account_store.AddAccountAtomic(address, {0, 0})) {
      return false;
    }
    return true;
  }
  virtual Address GetAddressForContract(const Address& account,
                                        uint32_t transaction_version) override {
    return Account::GetAddressForContract(account, GetNonceForAccount(account),
                                          transaction_version);
  }
  virtual bool IncreaseBalance(const Address& account,
                               libCps::Amount amount) override {
    return m_account_store.IncreaseBalance(account, amount.toQa());
  }
  virtual bool DecreaseBalance(const Address& account,
                               libCps::Amount amount) override {
    return m_account_store.DecreaseBalance(account, amount.toQa());
  }
  virtual bool TransferBalanceAtomic(const Address& from, const Address& to,
                                     libCps::Amount amount) override {
    return m_account_store.TransferBalanceAtomic(from, to, amount.toQa());
  }

  virtual void DiscardAtomics() override { m_account_store.DiscardAtomics(); }
  virtual void CommitAtomics() override { m_account_store.CommitAtomics(); }

  virtual void UpdateStates(const Address& address,
                            const std::map<std::string, zbytes>& states,
                            const std::vector<std::string>& toDeleteIndices,
                            bool temp, bool revertible) override {
    Account* account = m_account_store.GetAccountAtomic(address);
    account->UpdateStates(address, states, toDeleteIndices, temp, revertible);
  }

 private:
  AccountStoreSC<T>& m_account_store;
};

#endif /* ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORECPSINTERFACE_H_ */