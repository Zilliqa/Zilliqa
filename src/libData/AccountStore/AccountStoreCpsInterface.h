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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORECPSINTERFACE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORECPSINTERFACE_H_

#include "libData/AccountStore/AccountStoreSC.h"

#include "libCps/CpsAccountStoreInterface.h"

#include "libPersistence/ContractStorage.h"

struct AccountStoreCpsInterface : public libCps::CpsAccountStoreInterface {
 public:
  explicit AccountStoreCpsInterface(AccountStoreSC& accStore)
      : mAccountStore(accStore) {}
  virtual libCps::Amount GetBalanceForAccountAtomic(
      const Address& address) override {
    const Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      return libCps::Amount::fromQa(account->GetBalance());
    }
    return libCps::Amount{};
  }

  virtual uint64_t GetNonceForAccount(const Address& account) override {
    return mAccountStore.GetNonce(account);
  }

  virtual bool AccountExistsAtomic(const Address& address) override {
    return mAccountStore.GetAccountAtomic(address) != nullptr;
  }
  virtual bool AddAccountAtomic(const Address& address) override {
    if (!mAccountStore.AddAccountAtomic(address, {0, 0})) {
      return false;
    }
    return true;
  }
  virtual Address GetAddressForContract(const Address& account,
                                        uint32_t transaction_version) override {
    return Account::GetAddressForContract(
        account, GetNonceForAccountAtomic(account), transaction_version);
  }
  virtual bool IncreaseBalance(const Address& account,
                               libCps::Amount amount) override {
    return mAccountStore.IncreaseBalance(account, amount.toQa());
  }
  virtual bool DecreaseBalance(const Address& account,
                               libCps::Amount amount) override {
    return mAccountStore.DecreaseBalance(account, amount.toQa());
  }
  virtual void SetBalanceAtomic(const Address& address,
                                libCps::Amount amount) override {
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      account->SetBalance(amount.toQa());
    }
  }

  virtual bool TransferBalanceAtomic(const Address& from, const Address& to,
                                     libCps::Amount amount) override {
    LOG_GENERAL(WARNING,
                "TRANSFERRING FROM: " << from.hex() << ", TO: " << to.hex()
                                      << ", AMOUNT: " << amount.toQa());
    return mAccountStore.TransferBalanceAtomic(from, to, amount.toQa());
  }

  virtual void DiscardAtomics() override {
    mAccountStore.m_storageRootUpdateBufferAtomic.clear();
    mAccountStore.DiscardAtomics();
  }
  virtual void CommitAtomics() override {
    mAccountStore.CommitAtomics();
    mAccountStore.m_storageRootUpdateBuffer.insert(
        mAccountStore.m_storageRootUpdateBufferAtomic.begin(),
        mAccountStore.m_storageRootUpdateBufferAtomic.end());
  }

  virtual bool UpdateStates(const Address& address,
                            const std::map<std::string, zbytes>& states,
                            const std::vector<std::string>& toDeleteIndices,
                            bool temp, bool revertible = false) override {
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      return account->UpdateStates(address, states, toDeleteIndices, temp,
                                   revertible);
    }
    return false;
  }

  virtual bool UpdateStateValue(const Address& address, const zbytes& q,
                                unsigned int q_offset, const zbytes& v,
                                unsigned int v_offset) override {
    return Contract::ContractStorage::GetContractStorage().UpdateStateValue(
        address, q, q_offset, v, v_offset);
  }

  virtual std::string GenerateContractStorageKey(const Address& addr) override {
    return Contract::ContractStorage::GenerateStorageKey(
        addr, CONTRACT_ADDR_INDICATOR, {});
  };

  virtual void AddAddressToUpdateBufferAtomic(const Address& addr) override {
    mAccountStore.m_storageRootUpdateBufferAtomic.emplace(addr);
  }

  virtual void SetImmutableAtomic(const Address& address, const zbytes& code,
                                  const zbytes& initData) override {
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      account->SetImmutable(code, initData);
    }
  }

  virtual uint64_t GetNonceForAccountAtomic(const Address& address) override {
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      return account->GetNonce();
    }
    return 0;
  }

  virtual void IncreaseNonceForAccountAtomic(const Address& address) override {
    const auto nonce = GetNonceForAccountAtomic(address);
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      account->SetNonce(nonce + 1);
    }
  };

  virtual void FetchStateDataForContract(
      std::map<std::string, zbytes>& states, const dev::h160& address,
      const std::string& vname, const std::vector<std::string>& indices,
      bool temp) override {
    Contract::ContractStorage::GetContractStorage().FetchStateDataForContract(
        states, address, vname, indices, temp);
  }

  virtual void BufferCurrentContractStorageState() override {
    Contract::ContractStorage::GetContractStorage().BufferCurrentState();
  }

  virtual void RevertContractStorageState() override {
    Contract::ContractStorage::GetContractStorage().RevertPrevState();
  }

  virtual zbytes GetContractCode(const Address& address) override {
    Account* account = mAccountStore.GetAccountAtomic(address);
    if (account != nullptr) {
      return account->GetCode();
    }
    return {};
  }

 private:
  AccountStoreSC& mAccountStore;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORECPSINTERFACE_H_