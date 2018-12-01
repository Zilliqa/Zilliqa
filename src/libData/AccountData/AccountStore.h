/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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

  bool DeserializeDelta(const std::vector<unsigned char>& src,
                        unsigned int offset);

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

  std::vector<unsigned char> m_stateDeltaSerialized;

  AccountStore();
  ~AccountStore();

  /// Store the trie root to leveldb
  void MoveRootToDisk(const dev::h256& root);

 public:
  /// Returns the singleton AccountStore instance.
  static AccountStore& GetInstance();

  bool Serialize(std::vector<unsigned char>& src,
                 unsigned int offset) const override;

  bool Deserialize(const std::vector<unsigned char>& src,
                   unsigned int offset) override;

  bool SerializeDelta();

  void GetSerializedDelta(std::vector<unsigned char>& dst);

  bool DeserializeDelta(const std::vector<unsigned char>& src,
                        unsigned int offset, bool reversible = false);

  bool DeserializeDeltaTemp(const std::vector<unsigned char>& src,
                            unsigned int offset);

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
