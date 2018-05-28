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

#ifndef __ACCOUNTSTORE_H__
#define __ACCOUNTSTORE_H__

#include <json/json.h>
#include <mutex>
#include <set>
#include <unordered_map>

#include <boost/multiprecision/cpp_int.hpp>

#include "Account.h"
#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

template<class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

using StateHash = dev::h256;

/// Maintains the list of accounts.
class AccountStore : public Serializable
{
    std::unordered_map<Address, Account> m_addressToAccount;

    dev::OverlayDB m_db; // Our overlay for the state tree.
    SecureTrieDB<Address, dev::OverlayDB>
        m_state; // Our state tree, as an OverlayDB DB.
    dev::h256 prevRoot;

    uint64_t m_curBlockNum;
    Address m_curContractAddr;

    AccountStore();
    ~AccountStore();

    static bool Compare(const Account& l, const Account& r);

    bool UpdateStateTrie(const Address& address, const Account& account);

    /// Store the trie root to leveldb
    void MoveRootToDisk(const dev::h256& root);

    bool ParseCreateContractOutput();

    bool ParseCreateContractJsonOutput(const Json::Value& _json);

    bool ParseCallContractOutput();

    bool ParseCallContractJsonOutput(const Json::Value& _json);

    Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;

    std::string GetCreateContractCmdStr();

    std::string GetCallContractCmdStr();

    // Generate input for interpreter to check the correctness of contract
    bool ExportCreateContractFiles(Account* contract);

    bool
    ExportCallContractFiles(Account* contract,
                            const std::vector<unsigned char>& contractData);

    const std::vector<unsigned char>
    CompositeContractData(const std::string& funcName,
                          const std::string& amount, const Json::Value& params);

public:
    /// Returns the singleton AccountStore instance.
    static AccountStore& GetInstance();
    /// Empty the state trie, must be called explicitly otherwise will retrieve the historical data
    void Init();
    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(std::vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

    /// Verifies existence of Account in the list.
    bool DoesAccountExist(const Address& address);

    /// Adds an Account to the list.
    void AddAccount(const Address& address, const Account& account);
    void AddAccount(const PubKey& pubKey, const Account& account);

    bool UpdateAccounts(const uint64_t& blockNum,
                        const Transaction& transaction);

    /// Returns the Account associated with the specified address.
    Account* GetAccount(const Address& address);
    boost::multiprecision::uint256_t GetNumOfAccounts() const;

    bool IncreaseBalance(const Address& address,
                         const boost::multiprecision::uint256_t& delta);
    bool DecreaseBalance(const Address& address,
                         const boost::multiprecision::uint256_t& delta);

    /// Updates the source and destination accounts included in the specified Transaction.
    bool TransferBalance(const Address& from, const Address& to,
                         const boost::multiprecision::uint256_t& delta);
    boost::multiprecision::uint256_t GetBalance(const Address& address);

    bool IncreaseNonce(const Address& address);
    boost::multiprecision::uint256_t GetNonce(const Address& address);

    dev::h256 GetStateRootHash() const;

    bool UpdateStateTrieAll();
    void MoveUpdatesToDisk();
    void DiscardUnsavedUpdates();

    void PrintAccountState();

    bool RetrieveFromDisk();
    void RepopulateStateTrie();
};

#endif // __ACCOUNTSTORE_H__