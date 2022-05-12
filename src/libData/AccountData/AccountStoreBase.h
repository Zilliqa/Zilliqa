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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTOREBASE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTOREBASE_H_

#include <Schnorr.h>
#include <boost/iterator/iterator_facade.hpp>
#include "Account.h"
#include "Address.h"
#include "Transaction.h"
#include "TransactionReceipt.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "common/TxnStatus.h"
#include "depends/common/FixedHash.h"

class AccountStoreBase : public SerializableDataBlock {
 protected:
  std::unordered_map<Address, Account> m_addressToAccount;

 public:
  virtual void Init();

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

  Account* GetAccount(const Address& address);
  const Account* GetAccount(const Address& address) const;

  /// Verifies existence of Account in the list.
  bool IsAccountExist(const Address& address);

  /// Adds an Account to the list.
  bool AddAccount(const Address& address, const Account& account,
                  bool toReplace = false);
  bool AddAccount(const PubKey& pubKey, const Account& account);

  /// Increase balance of address by delta
  bool IncreaseBalance(const Address& address, const uint128_t& delta);

  /// Decrease balance of address by delta
  bool DecreaseBalance(const Address& address, const uint128_t& delta);

  /// Updates the source and destination accounts included in the specified
  /// Transaction.
  bool TransferBalance(const Address& from, const Address& to,
                       const uint128_t& delta);

  void RemoveAccount(const Address& address);

  size_t GetNumOfAccounts() const { return m_addressToAccount.size(); }

  // Implement a custom iterator: make range for loop work with AccountBase
  class Iterator
      : public boost::iterator_facade<Iterator,
                                      const std::pair<Address, Account>&,
                                      boost::forward_traversal_tag> {
    using base = std::unordered_map<Address, Account>::iterator;
    base m_iter;

   public:
    Iterator(base iter) : m_iter(iter) {}
    bool equal(Iterator const& other) const { return m_iter == other.m_iter; }
    void increment() { m_iter = ++m_iter; }
    const std::pair<Address, Account> dereference() const { return *m_iter; }
  };

  Iterator begin() { return Iterator(m_addressToAccount.begin()); }
  Iterator end() { return Iterator(m_addressToAccount.end()); }

  void PrintAccountState();
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTOREBASE_H_
