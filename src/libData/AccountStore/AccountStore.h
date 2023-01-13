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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORE_H_

#include <json/json.h>
#include <map>
#include <set>
#include <shared_mutex>
#include <unordered_map>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Hashes.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/AccountStoreSC.h"
#include "libData/AccountStore/AccountStoreTemp.h"
#include "libData/AccountStore/AccountStoreTrie.h"
#include "libScilla/UnixDomainSocketServer.h"
#include "libUtils/TxnExtras.h"

class ScillaIPCServer;


class AccountStore  : public AccountStoreTrie {
  /// instantiate of AccountStoreTemp, which is serving for the StateDelta
  /// generation
  AccountStoreTemp m_accountStoreTemp;

  /// used for states reverting
  std::unordered_map<Address, Account> m_addressToAccountRevChanged;
  std::unordered_map<Address, Account> m_addressToAccountRevCreated;

  /// primary mutex used by account store for protecting permanent states from
  /// external access
  mutable std::shared_timed_mutex m_mutexPrimary;
  /// mutex used when manipulating with state delta
  std::mutex m_mutexDelta;
  /// mutex related to revertibles
  std::mutex m_mutexRevertibles;
  /// buffer for the raw bytes of state delta serialized
  zbytes m_stateDeltaSerialized;
  // for external write access prioritization
  std::atomic<int> m_externalWriters;
  std::condition_variable_any m_writeCond;
  static constexpr int NUM_OF_WRITERS_IN_QUEUE = 1;

  /// Scilla IPC server related
  std::shared_ptr<ScillaIPCServer> m_scillaIPCServer;

  rpc::UnixDomainSocketServer m_scillaIPCServerConnector;

  AccountStore();
  ~AccountStore();

  /// Store the trie root to leveldb
  bool MoveRootToDisk(const dev::h256& root);

 public:
  /// Returns the singleton AccountStore instance.
  static AccountStore& GetInstance();

  bool Serialize(zbytes& src, unsigned int offset) const override;

  bool Deserialize(const zbytes& src, unsigned int offset) override;

  bool Deserialize(const std::string& src, unsigned int offset) override;

  /// generate serialized raw bytes for StateDelta
  bool SerializeDelta();

  /// get raw bytes of StateDelta
  void GetSerializedDelta(zbytes& dst);

  /// update this account states with the raw bytes of StateDelta
  bool DeserializeDelta(const zbytes& src, unsigned int offset,
                        bool revertible = false);

  /// update account states in AccountStoreTemp with the raw bytes of StateDelta
  bool DeserializeDeltaTemp(const zbytes& src, unsigned int offset);

  /// empty everything including the persistent storage for account states
  void Init() override;

  /// empty states data in memory
  void InitSoft();

  /// Reset the reference to underlying leveldb
  bool RefreshDB();

  /// Use the states in Temp State DB to refresh the state merkle trie
  bool UpdateStateTrieFromTempStateDB();

  /// commit the in-memory states into persistent storage
  bool MoveUpdatesToDisk(uint64_t dsBlockNum = 0);

  /// repopulate the in-memory data structures from persistent storage
  bool RetrieveFromDisk();

  bool RetrieveFromDiskOld();

  /// Get the instance of an account from AccountStoreTemp
  /// [[[WARNING]]] Test utility function, don't use in core protocol
  Account* GetAccountTemp(const Address& address);

  Account* GetAccountTempAtomic(const Address& address);

  /// update account states in AccountStoreTemp
  bool UpdateAccountsTemp(const uint64_t& blockNum,
                          const unsigned int& numShards, const bool& isDS,
                          const Transaction& transaction,
                          const TxnExtras& txnExtras,
                          TransactionReceipt& receipt, TxnStatus& error_code);

  /// add account in AccountStoreTemp
  void AddAccountTemp(const Address& address, const Account& account) {
    std::lock_guard<std::mutex> g(m_mutexDelta);
    m_accountStoreTemp.AddAccount(address, account);
  }

  /// increase balance for account in AccountStoreTemp
  bool IncreaseBalanceTemp(const Address& address, const uint128_t& delta) {
    std::lock_guard<std::mutex> g(m_mutexDelta);
    return m_accountStoreTemp.IncreaseBalance(address, delta);
  }

  /// get the nonce of an account in AccountStoreTemp
  uint128_t GetNonceTemp(const Address& address);

  /// Update the states balance due to coinbase changes to the AccountStoreTemp
  bool UpdateCoinbaseTemp(const Address& rewardee,
                          const Address& genesisAddress,
                          const uint128_t& amount);

  /// Call ProcessStorageRootUpdateBuffer in AccountStoreTemp
  void ProcessStorageRootUpdateBufferTemp() {
    std::lock_guard<std::mutex> g(m_mutexDelta);
    m_accountStoreTemp.ProcessStorageRootUpdateBuffer();
  }

  /// Call ProcessStorageRootUpdateBuffer in AccountStoreTemp
  void CleanStorageRootUpdateBufferTemp() {
    std::lock_guard<std::mutex> g(m_mutexDelta);
    m_accountStoreTemp.CleanStorageRootUpdateBuffer();
  }

  void CleanNewLibrariesCacheTemp() {
    std::lock_guard<std::mutex> g(m_mutexDelta);
    m_accountStoreTemp.CleanNewLibrariesCache();
  }

  /// used in deserialization
  void AddAccountDuringDeserialization(const Address& address,
                                       const Account& account,
                                       const Account& oriAccount,
                                       const bool fullCopy = false,
                                       const bool revertible = false) {
    m_addressToAccount->insert_or_assign(address, account);

    if (revertible) {
      if (fullCopy) {
        m_addressToAccountRevCreated.insert_or_assign(address, account);
      } else {
        m_addressToAccountRevChanged.insert_or_assign(address, oriAccount);
      }
    }

    UpdateStateTrie(address, account);
  }

  /// return the hash of the raw bytes of StateDelta
  StateHash GetStateDeltaHash();

  /// commit the StateDelta to update the AccountStore in an irrevertible way
  void CommitTemp();

  /// clean the AccountStoreTemp and the serialized StateDelta raw bytes
  void InitTemp();

  /// commit the StateDelta to update the AccountStore in a revertible way
  void CommitTempRevertible();

  /// revert the AccountStore if previously called CommitTempRevertible
  bool RevertCommitTemp();

  /// NotifyTimeout for AccountStoreTemp
  void NotifyTimeoutTemp();

  /// clean the data for revert the AccountStore
  void InitRevertibles();

  void PurgeUnnecessary();

  void SetPurgeStopSignal();

  bool IsPurgeRunning();

  std::shared_timed_mutex& GetPrimaryMutex() { return m_mutexPrimary; }

  void IncrementPrimaryWriteAccessCount() { ++m_externalWriters; }
  void DecrementPrimaryWriteAccessCount() { --m_externalWriters; }
  bool GetPrimaryWriteAccess() {
    return m_externalWriters.load() < NUM_OF_WRITERS_IN_QUEUE;
  }
  std::condition_variable_any& GetPrimaryWriteAccessCond() {
    return m_writeCond;
  }
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORE_H_
