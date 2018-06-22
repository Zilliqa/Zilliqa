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

#ifndef __ACCOUNTSTOREBASE_H__
#define __ACCOUNTSTOREBASE_H__

#include <json/json.h>
#include <set>
#include <unordered_map>

#include <boost/multiprecision/cpp_int.hpp>

#include "Account.h"
#include "Address.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libDatabase/OverlayDB.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

using namespace std;
using namespace boost::multiprecision;

template<class DB, class MAP> class AccountStoreBase : public Serializable
{
protected:
    shared_ptr<MAP> m_addressToAccount;

    DB m_db;
    dev::SpecificTrieDB<dev::GenericTrieDB<DB>, Address> m_state;
    h256 prevRoot;

    uint64_t m_curBlockNum;
    Address m_curContractAddr;

    AccountStoreBase();

    bool ParseCreateContractOutput();

    bool ParseCreateContractJsonOutput(const Json::Value& _json);

    bool ParseCallContractOutput();

    bool ParseCallContractJsonOutput(const Json::Value& _json);

    Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;

    string GetCreateContractCmdStr();

    string GetCallContractCmdStr();

    // Generate input for interpreter to check the correctness of contract
    bool ExportCreateContractFiles(Account* contract);

    bool ExportCallContractFiles(Account* contract,
                                 const vector<unsigned char>& contractData);

    const vector<unsigned char>
    CompositeContractData(const string& funcName, const string& amount,
                          const Json::Value& params);

    bool UpdateStateTrie(const Address& address, const Account& account);

public:
    virtual void Init();

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    virtual int Deserialize(const vector<unsigned char>& src,
                            unsigned int offset);

    /// Verifies existence of Account in the list.
    bool DoesAccountExist(const Address& address);

    /// Adds an Account to the list.
    void AddAccount(const Address& address, const Account& account);
    void AddAccount(const PubKey& pubKey, const Account& account);

    bool UpdateAccounts(const uint64_t& blockNum,
                        const Transaction& transaction);

    /// Returns the Account associated with the specified address.
    virtual Account* GetAccount(const Address& address) = 0;
    uint256_t GetNumOfAccounts() const;

    bool IncreaseBalance(const Address& address, const uint256_t& delta);
    bool DecreaseBalance(const Address& address, const uint256_t& delta);

    /// Updates the source and destination accounts included in the specified Transaction.
    bool TransferBalance(const Address& from, const Address& to,
                         const uint256_t& delta);
    uint256_t GetBalance(const Address& address);

    bool IncreaseNonce(const Address& address);
    uint256_t GetNonce(const Address& address);

    h256 GetStateRootHash() const;
    bool UpdateStateTrieAll();
    void RepopulateStateTrie();

    void PrintAccountState();
};

#include "AccountStoreBase.tpp"

#endif // __ACCOUNTSTOREBASE_H__