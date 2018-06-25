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

#include <boost/multiprecision/cpp_int.hpp>

#include "Account.h"
#include "Address.h"
#include "Transaction.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"

using namespace std;
using namespace boost::multiprecision;

template<class MAP> class AccountStoreBase : public Serializable
{
protected:
    shared_ptr<MAP> m_addressToAccount;

    AccountStoreBase();

public:
    virtual void Init();

    /// Implements the Serialize function inherited from Serializable.
    unsigned int Serialize(vector<unsigned char>& dst,
                           unsigned int offset) const;

    /// Implements the Deserialize function inherited from Serializable.
    virtual int Deserialize(const vector<unsigned char>& src,
                            unsigned int offset);

    virtual Account* GetAccount(const Address& address);

    virtual bool UpdateAccounts(const uint64_t& blockNum,
                                const Transaction& transaction);

    /// Verifies existence of Account in the list.
    bool IsAccountExist(const Address& address);

    /// Adds an Account to the list.
    void AddAccount(const Address& address, const Account& account);
    void AddAccount(const PubKey& pubKey, const Account& account);

    uint256_t GetNumOfAccounts() const;

    bool IncreaseBalance(const Address& address, const uint256_t& delta);
    bool DecreaseBalance(const Address& address, const uint256_t& delta);

    /// Updates the source and destination accounts included in the specified Transaction.
    bool TransferBalance(const Address& from, const Address& to,
                         const uint256_t& delta);
    uint256_t GetBalance(const Address& address);

    bool IncreaseNonce(const Address& address);
    uint256_t GetNonce(const Address& address);

    virtual void PrintAccountState();
};

#include "AccountStoreBase.tpp"

#endif // __ACCOUNTSTOREBASE_H__
