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

#ifndef __ACCOUNTSTORE_H__
#define __ACCOUNTSTORE_H__

#include <json/json.h>
#include <map>
#include <set>
#include <shared_mutex>
#include <unordered_map>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "Account.h"
#include "AccountStoreSC.h"
#include "AccountStoreTrie.h"
#include "Address.h"
#include "TransactionReceipt.h"
#include "common/Constants.h"
#include "common/Singleton.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

using StateHash = dev::h256;

class AccountStore;

class AccountStoreTemp : public AccountStoreSC<std::map<Address, Account>> {
  // shared_ptr<unordered_map<Address, Account>> m_superAddressToAccount;
  AccountStore& m_parent;

 public:
  // AccountStoreTemp(
  //     const shared_ptr<unordered_map<Address, Account>>& addressToAccount);
  AccountStoreTemp(AccountStore& parent);

  bool DeserializeDelta(const bytes& src, unsigned int offset);

  /// Returns the Account associated with the specified address.
  Account* GetAccount(const Address& address) override;

  const std::shared_ptr<std::map<Address, Account>>& GetAddressToAccount() {
    return this->m_addressToAccount;
  }

  void AddAccountDuringDeserialization(const Address& address,
                                       const Account& account) {
    (*m_addressToAccount)[address] = account;
  }
};

class AccountStore
    : public AccountStoreTrie<dev::OverlayDB,
                              std::unordered_map<Address, Account>>,
      Singleton<AccountStore> {
  std::unique_ptr<AccountStoreTemp> m_accountStoreTemp;

  std::unordered_map<Address, Account> m_addressToAccountRevChanged;
  std::unordered_map<Address, Account> m_addressToAccountRevCreated;

  // primary mutex used by account store for protecting permanent states from
  // external access
  mutable std::shared_timed_mutex m_mutexPrimary;
  // mutex used when manipulating with state delta
  std::mutex m_mutexDelta;
  // mutex related to reversibles
  std::mutex m_mutexReversibles;

  bytes m_stateDeltaSerialized;

  AccountStore();
  ~AccountStore();

  /// Store the trie root to leveldb
  void MoveRootToDisk(const dev::h256& root);

 public:
  /// Returns the singleton AccountStore instance.
  static AccountStore& GetInstance();

  bool Serialize(bytes& src, unsigned int offset) const override;

  bool Deserialize(const bytes& src, unsigned int offset) override;

  bool SerializeDelta();

  void GetSerializedDelta(bytes& dst);

  bool DeserializeDelta(const bytes& src, unsigned int offset,
                        bool reversible = false);

  bool DeserializeDeltaTemp(const bytes& src, unsigned int offset);

  /// Empty the state trie, must be called explicitly otherwise will retrieve
  /// the historical data
  void Init() override;

  void InitSoft();

  void MoveUpdatesToDisk();
  void DiscardUnsavedUpdates();

  bool RetrieveFromDisk();

  bool UpdateAccountsTemp(const uint64_t& blockNum,
                          const unsigned int& numShards, const bool& isDS,
                          const Transaction& transaction,
                          TransactionReceipt& receipt);

  void AddAccountTemp(const Address& address, const Account& account) {
    m_accountStoreTemp->AddAccount(address, account);
  }

  bool IncreaseBalanceTemp(const Address& address,
                           const boost::multiprecision::uint128_t& delta) {
    return m_accountStoreTemp->IncreaseBalance(address, delta);
  }

  void AddAccountDuringDeserialization(const Address& address,
                                       const Account& account,
                                       const bool fullCopy = false,
                                       const bool reversible = false) {
    (*m_addressToAccount)[address] = account;

    if (reversible) {
      if (fullCopy) {
        m_addressToAccountRevCreated[address] = account;
      } else {
        m_addressToAccountRevChanged[address] = account;
      }
    }

    UpdateStateTrie(address, account);
  }

  boost::multiprecision::uint128_t GetNonceTemp(const Address& address);

  bool UpdateCoinbaseTemp(const Address& rewardee,
                          const Address& genesisAddress,
                          const boost::multiprecision::uint128_t& amount);

  StateHash GetStateDeltaHash();

  void CommitTemp();

  void InitTemp();

  void CommitTempReversible();

  void RevertCommitTemp();

  void InitReversibles();
};

#endif  // __ACCOUNTSTORE_H__
