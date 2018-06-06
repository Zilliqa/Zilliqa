/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#ifndef __ACCOUNT_H__
#define __ACCOUNT_H__

#include <array>
#include <boost/multiprecision/cpp_int.hpp>
#include <json/json.h>
#include <leveldb/db.h>
#include <vector>

#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"
#include "libCrypto/Schnorr.h"

using namespace std;
using namespace dev;
using namespace boost::multiprecision;

/// DB storing trie storage information for all accounts.
// static OverlayDB contractStatesDB("contractStates");

template<class KeyType, class DB>
using AccountTrieDB = SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

class Account : public Serializable
{
    boost::multiprecision::uint256_t m_balance;
    boost::multiprecision::uint256_t m_nonce;
    h256 m_storageRoot, m_prevRoot;
    h256 m_codeHash;
    // The associated code for this account.
    Json::Value m_initValJson;
    vector<unsigned char> m_codeCache;

    AccountTrieDB<h256, OverlayDB> m_storage;

public:
    Account();

    /// Constructor for loading account information from a byte stream.
    Account(const vector<unsigned char>& src, unsigned int offset);

    /// Constructor for a account.
    Account(const uint256_t& balance, const uint256_t& nonce);

    /// Returns true if account is a contract account
    bool isContract() const { return m_codeHash != h256(); }

    /// Utilization function for trieDB
    void InitStorage();

    /// Parse the Immutable Data at Constract Initialization Stage
    void InitContract(const vector<unsigned char>& data);

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const vector<unsigned char>& src, unsigned int offset);

    /// Increases account balance by the specified delta amount.
    bool IncreaseBalance(const uint256_t& delta);

    /// Decreases account balance by the specified delta amount.
    bool DecreaseBalance(const uint256_t& delta);

    void SetBalance(const uint256_t& balance) { m_balance = balance; }

    /// Returns the account balance.
    const uint256_t& GetBalance() const { return m_balance; }

    /// Increases account nonce by 1.
    bool IncreaseNonce();

    /// Returns the account nonce.
    const uint256_t& GetNonce() const { return m_nonce; }

    void SetStorageRoot(const h256& root);

    /// Returns the storage root.
    const h256& GetStorageRoot() const { return m_storageRoot; }

    /// Set the code
    void SetCode(const std::vector<unsigned char>& code);

    const std::vector<unsigned char>& GetCode() const { return m_codeCache; }

    /// Returns the code hash.
    const h256& GetCodeHash() const { return m_codeHash; }

    void SetStorage(string _k, string _type, string _v, bool _mutable = true);

    /// Return the data for a parameter, type + value
    vector<string> GetStorage(string _k) const;

    Json::Value GetInitJson() const { return m_initValJson; }

    Json::Value GetStorageJson() const;

    void Commit() { m_prevRoot = m_storageRoot; }

    void RollBack();

    /// Computes an account address from a specified PubKey.
    static Address GetAddressFromPublicKey(const PubKey& pubKey);

    /// Computes an account address from a sender and its nonce
    static Address GetAddressForContract(const Address& sender,
                                         const uint256_t& nonce);

    friend inline std::ostream& operator<<(std::ostream& _out,
                                           Account const& account);
};

inline std::ostream& operator<<(std::ostream& _out, Account const& account)
{
    _out << account.m_balance << " " << account.m_nonce << " "
         << account.m_storageRoot << " " << account.m_codeHash;
    return _out;
}

#endif // __ACCOUNT_H__