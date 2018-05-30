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
#include <set>
#include <unordered_map>

#include <boost/multiprecision/cpp_int.hpp>

#include "AccountStoreBase.h"
#include "Account.h"
#include "Address.h"
#include "common/Constants.h"
#include "common/Singleton.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

template<class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

using StateHash = dev::h256;

class AccountStore : public AccountStoreBase, Singleton<AccountStore>
{
    friend class Singleton<AccountStore>;

    dev::OverlayDB m_db; // Our overlay for the state tree.
    SecureTrieDB<Address, dev::OverlayDB>
        m_state; // Our state tree, as an OverlayDB DB.
    dev::h256 prevRoot;

    AccountStoreTemp m_tempAccountStore;

    AccountStore();
    ~AccountStore();

    bool UpdateStateTrie(const Address& address, const Account& account);

    /// Store the trie root to leveldb
    void MoveRootToDisk(const dev::h256& root);

    bool ParseCreateContractJsonOutput(const Json::Value& _json) override;

    bool ParseCallContractJsonOutput(const Json::Value& _json) override;

public:
    /// Returns the singleton AccountStore instance.
    static AccountStore& GetInstance();

    /// Empty the state trie, must be called explicitly otherwise will retrieve the historical data
    void Init() override;

    /// Returns the Account associated with the specified address.
    Account* GetAccount(const Address& address) override;

    dev::h256 GetStateRootHash() const;

    bool UpdateStateTrieAll();
    void MoveUpdatesToDisk();
    void DiscardUnsavedUpdates();

    bool RetrieveFromDisk();
    void RepopulateStateTrie();

    bool UpdateAccountsTemp();
    void CommitTemp();
};

#endif // __ACCOUNTSTORE_H__