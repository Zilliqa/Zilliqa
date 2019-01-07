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

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <json/json.h>
#include <leveldb/db.h>
#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <vector>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"
#include "libCrypto/Schnorr.h"

/// DB storing trie storage information for all accounts.
// static OverlayDB contractStatesDB("contractStates");

template <class KeyType, class DB>
using AccountTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

class Account : public SerializableDataBlock {
  boost::multiprecision::uint128_t m_balance;
  uint64_t m_nonce;
  dev::h256 m_storageRoot, m_prevRoot;
  dev::h256 m_codeHash;
  // The associated code for this account.
  uint64_t m_createBlockNum = 0;
  Json::Value m_initValJson;
  bytes m_initData;
  bytes m_codeCache;

  const dev::h256 GetKeyHash(const std::string& key) const;

  AccountTrieDB<dev::h256, dev::OverlayDB> m_storage;

 public:
  Account();

  /// Constructor for loading account information from a byte stream.
  Account(const bytes& src, unsigned int offset);

  /// Constructor for a account.
  Account(const boost::multiprecision::uint128_t& balance,
          const uint64_t& nonce);

  /// Returns true if account is a contract account
  bool isContract() const;

  /// Utilization function for trieDB
  void InitStorage();

  /// Parse the Immutable Data at Constract Initialization Stage
  void InitContract(const bytes& data, const Address& addr);

  /// Set the block number when this account was created.
  void SetCreateBlockNum(const uint64_t& blockNum);

  /// Get the block number when this account was created.
  const uint64_t& GetCreateBlockNum() const;

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Increases account balance by the specified delta amount.
  bool IncreaseBalance(const boost::multiprecision::uint128_t& delta);

  /// Decreases account balance by the specified delta amount.
  bool DecreaseBalance(const boost::multiprecision::uint128_t& delta);

  bool ChangeBalance(const boost::multiprecision::int256_t& delta);

  void SetBalance(const boost::multiprecision::uint128_t& balance);

  /// Returns the account balance.
  const boost::multiprecision::uint128_t& GetBalance() const;

  /// Increases account nonce by 1.
  bool IncreaseNonce();

  bool IncreaseNonceBy(const uint64_t& nonceDelta);

  void SetNonce(const uint64_t& nonce);

  /// Returns the account nonce.
  const uint64_t& GetNonce() const;

  void SetStorageRoot(const dev::h256& root);

  /// Returns the storage root.
  const dev::h256& GetStorageRoot() const;

  /// Set the code
  void SetCode(const bytes& code);

  const bytes& GetCode() const;

  /// Returns the code hash.
  const dev::h256& GetCodeHash() const;

  void SetStorage(const dev::h256& k_hash, const std::string& rlpStr);

  void SetStorage(std::string k, std::string type, std::string v,
                  bool is_mutable = true);

  /// Return the data for a parameter, type + value
  std::vector<std::string> GetStorage(const std::string& _k) const;

  std::string GetRawStorage(const dev::h256& k_hash) const;

  Json::Value GetInitJson() const;

  const bytes& GetInitData() const;

  void SetInitData(const bytes& initData);

  void InitContract(const Address& addr);

  std::vector<dev::h256> GetStorageKeyHashes() const;

  Json::Value GetStorageJson() const;

  void Commit();

  void RollBack();

  /// Computes an account address from a specified PubKey.
  static Address GetAddressFromPublicKey(const PubKey& pubKey);

  /// Computes an account address from a sender and its nonce
  static Address GetAddressForContract(const Address& sender,
                                       const uint64_t& nonce);

  friend inline std::ostream& operator<<(std::ostream& out,
                                         Account const& account);

  // static bool SerializeDelta(bytes& dst, unsigned int offset,
  //                            Account* oldAccount, const Account& newAccount);

  // static bool DeserializeDelta(const bytes& src, unsigned int offset,
  //                              Account& account, bool fullCopy);
};

inline std::ostream& operator<<(std::ostream& out, Account const& account) {
  out << account.m_balance << " " << account.m_nonce << " "
      << account.m_storageRoot << " " << account.m_codeHash;
  return out;
}

#endif  // __ACCOUNT_H__
