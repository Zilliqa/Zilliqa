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

#include "Account.h"
#include "AccountStoreBase.h"
#include "Address.h"
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

class AccountStoreTemp : public AccountStoreBase<MemoryDB>
{
    // shared_ptr<unordered_map<Address, Account>> m_superAddressToAccount;
    AccountStore* m_parent;

public:
    // AccountStoreTemp(
    //     const shared_ptr<unordered_map<Address, Account>>& addressToAccount);
    AccountStoreTemp(AccountStore* parent);

    /// Returns the Account associated with the specified address.
    Account* GetAccount(const Address& address) override;

    const shared_ptr<unordered_map<Address, Account>>& GetAddressToAccount();
};

// template<class KeyType, class DB>
// using SecureTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;
// using StateHash = h256;

class AccountStore : public AccountStoreBase<OverlayDB>, Singleton<AccountStore>
{
    friend class Singleton<AccountStore>;

    shared_ptr<AccountStoreTemp> m_accountStoreTemp;

    AccountStore();
    ~AccountStore();

    /// Store the trie root to leveldb
    void MoveRootToDisk(const h256& root);

public:
    /// Returns the singleton AccountStore instance.
    static AccountStore& GetInstance();

    int Deserialize(const vector<unsigned char>& src,
                    unsigned int offset) override;

    unsigned int SerializeDelta(vector<unsigned char>& dst,
                                unsigned int offset);

    int DeserializeDelta(const vector<unsigned char>& src, unsigned int offset);

    /// Empty the state trie, must be called explicitly otherwise will retrieve the historical data
    void Init() override;

    Account* GetAccount(const Address& address) override;

    void MoveUpdatesToDisk();
    void DiscardUnsavedUpdates();

    bool RetrieveFromDisk();

    bool UpdateAccountsTemp(const uint64_t& blockNum,
                            const Transaction& transaction);
    void CommitTemp();
};

#endif // __ACCOUNTSTORE_H__