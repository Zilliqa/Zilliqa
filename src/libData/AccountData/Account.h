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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNT_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNT_H_

#include <json/json.h>
#include <leveldb/db.h>
#include <array>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include <Schnorr.h>
#include "depends/libTrie/TrieDB.h"

/// DB storing trie storage information for all accounts.
// static OverlayDB contractStatesDB("contractStates");

template <class KeyType, class DB>
using AccountTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

class AccountBase : public SerializableDataBlock {
 protected:
  uint32_t m_version{};
  uint128_t m_balance;
  uint64_t m_nonce{};
  dev::h256 m_storageRoot;
  dev::h256 m_codeHash;

 public:
  AccountBase() {}

  AccountBase(const uint128_t& balance, const uint64_t& nonce,
              const uint32_t& version);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset);

  void SetVersion(const uint32_t& version);

  const uint32_t& GetVersion() const;

  /// Increases account balance by the specified delta amount.
  bool IncreaseBalance(const uint128_t& delta);

  /// Decreases account balance by the specified delta amount.
  bool DecreaseBalance(const uint128_t& delta);

  bool ChangeBalance(const boost::multiprecision::int256_t& delta);

  void SetBalance(const uint128_t& balance);

  /// Returns the account balance.
  const uint128_t& GetBalance() const;

  void SetNonce(const uint64_t& nonce);

  /// Returns the account nonce.
  const uint64_t& GetNonce() const;

  /// Increases account nonce by 1.
  bool IncreaseNonce();

  bool IncreaseNonceBy(const uint64_t& nonceDelta);

  void SetStorageRoot(const dev::h256& root);

  /// Returns the storage root.
  const dev::h256& GetStorageRoot() const;

  void SetCodeHash(const dev::h256& codeHash);

  /// Returns the code hash.
  const dev::h256& GetCodeHash() const;

  /// Returns true if account is a contract account
  bool isContract() const;

  friend inline std::ostream& operator<<(std::ostream& out,
                                         AccountBase const& account);
};

inline std::ostream& operator<<(std::ostream& out,
                                AccountBase const& accountbase) {
  out << accountbase.GetBalance() << " " << accountbase.GetNonce() << " "
      << accountbase.GetStorageRoot() << " " << accountbase.GetCodeHash();
  return out;
}

class Account : public AccountBase {
  // The associated code for this account.
  bytes m_codeCache;
  bytes m_initDataCache;
  Address m_address;  // used by contract account only
  Json::Value m_initDataJson = Json::nullValue;
  uint32_t m_scilla_version = std::numeric_limits<uint32_t>::max();
  bool m_is_library = false;
  std::vector<Address> m_extlibs;

  bool PrepareInitDataJson(const bytes& initData, const Address& addr,
                           const uint64_t& blockNum, Json::Value& root,
                           uint32_t& scilla_version, bool& is_library,
                           std::vector<Address>& extlibs);

  bool ParseInitData(const Json::Value& root, uint32_t& scilla_version,
                     bool& is_library, std::vector<Address>& extlibs);

  AccountTrieDB<dev::h256, dev::OverlayDB> m_storage;

 public:
  Account() {}

  /// Constructor for loading account information from a byte stream.
  Account(const bytes& src, unsigned int offset);

  /// Constructor for a account.
  Account(const uint128_t& balance, const uint64_t& nonce,
          const uint32_t& version = ACCOUNT_VERSION);

  /// Parse the Immutable Data at Constract Initialization Stage
  bool InitContract(const bytes& code, const bytes& initData,
                    const Address& addr, const uint64_t& blockNum);

  bool SetImmutable(const bytes& code, const bytes& initData);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Implements the Serialize function inherited from Serializable.
  bool SerializeBase(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool DeserializeBase(const bytes& src, unsigned int offset);

  void SetAddress(const Address& addr);

  const Address& GetAddress() const;

  bool SetCode(const bytes& code);

  const bytes GetCode() const;

  bool SetInitData(const bytes& initData);

  const bytes GetInitData() const;

  bool GetContractAuxiliaries(bool& is_library, uint32_t& scilla_version,
                              std::vector<Address>& extlibs);

  // includes scilla_version, is_library, and extlibs
  bool RetrieveContractAuxiliaries();

  /// !temp represents getting whole states
  bool GetUpdatedStates(std::map<std::string, bytes>& t_states,
                        std::set<std::string>& toDeleteIndices,
                        bool temp) const;

  bool UpdateStates(const Address& addr,
                    const std::map<std::string, bytes>& t_states,
                    const std::vector<std::string>& toDeleteIndices, bool temp,
                    bool revertible = false);

  bool FetchStateJson(Json::Value& root, const std::string& vname = "",
                      const std::vector<std::string>& indices = {},
                      bool temp = false) const;

  /// Computes an account address from a specified PubKey.
  static Address GetAddressFromPublicKey(const PubKey& pubKey);

  /// Computes an account address from a sender and its nonce
  static Address GetAddressForContract(const Address& sender,
                                       const uint64_t& nonce);
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNT_H_
