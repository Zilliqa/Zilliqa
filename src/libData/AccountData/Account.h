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
using SecureTrieDB = SpecificTrieDB<dev::HashedGenericTrieDB<DB>, KeyType>;

class Account : public Serializable
{
    boost::multiprecision::uint256_t m_balance;
    boost::multiprecision::uint256_t m_nonce;
    h256 m_storageRoot = h256();
    h256 m_prevRoot;
    h256 m_codeHash;

    // The associated code for this account.
    vector<unsigned char> m_codeCache;

    SecureTrieDB<bytesConstRef, OverlayDB> m_storage;

public:
    Account();

    ~Account() { m_storage.init(); }

    /// Constructor for loading account information from a byte stream.
    Account(const vector<unsigned char>& src, unsigned int offset);

    /// Constructor with account balance, and nonce.
    Account(const uint256_t& balance, const uint256_t& nonce,
            const h256& storageRoot, const h256& codeHash);

    void InitStorage();

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const vector<unsigned char>& src, unsigned int offset);

    /// Increases account balance by the specified delta amount.
    bool IncreaseBalance(const uint256_t& delta);

    /// Decreases account balance by the specified delta amount.
    bool DecreaseBalance(const uint256_t& delta);

    /// Returns the account balance.
    const uint256_t& GetBalance() const { return m_balance; }

    /// Increases account nonce by 1.
    bool IncreaseNonce();

    /// Returns the account nonce.
    const uint256_t& GetNonce() const { return m_nonce; }

    /// Returns the storage root.
    const h256& GetStorageRoot() const { return m_storageRoot; }

    /// Returns the code hash.
    const h256& GetCodeHash() const { return m_codeHash; }

    /// Set the code of the account. Used by "create" messages
    void SetCode(vector<unsigned char>&& code);

    const vector<unsigned char>& GetCode() const { return m_codeCache; }

    void SetStorage(string _k, string _mutable, string _type, string _v);

    vector<string> GetKeys();

    vector<string> GetStorage(string _k);

    string GetStorageValue(string _k);

    void Commit() { m_prevRoot = m_storageRoot; }

    void RollBack();

    /// Computes an account address from a specified PubKey.
    static Address GetAddressFromPublicKey(const PubKey& pubKey);

    friend inline std::ostream& operator<<(std::ostream& _out,
                                           Account const& account);
};

inline std::ostream& operator<<(std::ostream& _out, Account const& account)
{
    _out << account.m_balance << " " << account.m_nonce;
    return _out;
}

#endif // __ACCOUNT_H__