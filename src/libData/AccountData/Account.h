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

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <json/json.h>
#include <leveldb/db.h>
#include <array>
#include <boost/multiprecision/cpp_int.hpp>
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
  boost::multiprecision::uint256_t m_balance;
  boost::multiprecision::uint256_t m_nonce;
  dev::h256 m_storageRoot, m_prevRoot;
  dev::h256 m_codeHash;
  // The associated code for this account.
  uint64_t m_createBlockNum = 0;
  Json::Value m_initValJson;
  std::vector<unsigned char> m_initData;
  std::vector<unsigned char> m_codeCache;

  const dev::h256 GetKeyHash(const std::string& key) const;

  AccountTrieDB<dev::h256, dev::OverlayDB> m_storage;

 public:
  Account();

  /// Constructor for loading account information from a byte stream.
  Account(const std::vector<unsigned char>& src, unsigned int offset);

  /// Constructor for a account.
  Account(const boost::multiprecision::uint256_t& balance,
          const boost::multiprecision::uint256_t& nonce);

  /// Returns true if account is a contract account
  bool isContract() const { return m_codeHash != dev::h256(); }

  /// Utilization function for trieDB
  void InitStorage();

  /// Parse the Immutable Data at Constract Initialization Stage
  void InitContract(const std::vector<unsigned char>& data);

  /// Set the block number when this account was created.
  void SetCreateBlockNum(const uint64_t& blockNum) {
    m_createBlockNum = blockNum;
  }

  /// Get the block number when this account was created.
  const uint64_t& GetCreateBlockNum() const { return m_createBlockNum; }

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

  /// Increases account balance by the specified delta amount.
  bool IncreaseBalance(const boost::multiprecision::uint256_t& delta);

  /// Decreases account balance by the specified delta amount.
  bool DecreaseBalance(const boost::multiprecision::uint256_t& delta);

  bool ChangeBalance(const boost::multiprecision::int256_t& delta);

  void SetBalance(const boost::multiprecision::uint256_t& balance) {
    m_balance = balance;
  }

  /// Returns the account balance.
  const boost::multiprecision::uint256_t& GetBalance() const {
    return m_balance;
  }

  /// Increases account nonce by 1.
  bool IncreaseNonce();

  bool IncreaseNonceBy(const boost::multiprecision::uint256_t& nonceDelta);

  void SetNonce(const boost::multiprecision::uint256_t& nonce) {
    m_nonce = nonce;
  }

  /// Returns the account nonce.
  const boost::multiprecision::uint256_t& GetNonce() const { return m_nonce; }

  void SetStorageRoot(const dev::h256& root);

  /// Returns the storage root.
  const dev::h256& GetStorageRoot() const { return m_storageRoot; }

  /// Set the code
  void SetCode(const std::vector<unsigned char>& code);

  const std::vector<unsigned char>& GetCode() const { return m_codeCache; }

  /// Returns the code hash.
  const dev::h256& GetCodeHash() const { return m_codeHash; }

  void SetStorage(const dev::h256& k_hash, const std::string& rlpStr);

  void SetStorage(std::string k, std::string type, std::string v,
                  bool is_mutable = true);

  /// Return the data for a parameter, type + value
  std::vector<std::string> GetStorage(const std::string& _k) const;

  std::string GetRawStorage(const dev::h256& k_hash) const;

  Json::Value GetInitJson() const { return m_initValJson; }

  const std::vector<unsigned char>& GetInitData() const { return m_initData; }

  void SetInitData(const std::vector<unsigned char>& initData) {
    m_initData = initData;
  }

  void InitContract();

  std::vector<dev::h256> GetStorageKeyHashes() const;

  Json::Value GetStorageJson() const;

  void Commit() { m_prevRoot = m_storageRoot; }

  void RollBack();

  /// Computes an account address from a specified PubKey.
  static Address GetAddressFromPublicKey(const PubKey& pubKey);

  /// Computes an account address from a sender and its nonce
  static Address GetAddressForContract(
      const Address& sender, const boost::multiprecision::uint256_t& nonce);

  friend inline std::ostream& operator<<(std::ostream& out,
                                         Account const& account);

  static bool SerializeDelta(std::vector<unsigned char>& dst,
                             unsigned int offset, Account* oldAccount,
                             const Account& newAccount);

  static bool DeserializeDelta(const std::vector<unsigned char>& src,
                               unsigned int offset, Account& account,
                               bool fullCopy);
};

inline std::ostream& operator<<(std::ostream& out, Account const& account) {
  out << account.m_balance << " " << account.m_nonce << " "
      << account.m_storageRoot << " " << account.m_codeHash;
  return out;
}

#endif  // __ACCOUNT_H__
