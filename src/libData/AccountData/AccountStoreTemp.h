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
#include <unordered_map>

#include <boost/multiprecision/cpp_int.hpp>

#include "AccountStoreBase.h"
#include "Account.h"
#include "Address.h"
#include "common/Constants.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

class AccountStoreTemp : public AccountStoreBase
{
    std::shared_ptr<std::unordered_map<Address, Account>> m_superAddressToAccount;

    bool ParseCreateContractJsonOutput(const Json::Value& _json) override;

    bool ParseCallContractJsonOutput(const Json::Value& _json) override;

public:
    AccountStoreTemp(const std::shared_ptr<std::unordered_map<Address, Account>>& addressToAccount);

    void Reset();

    /// Returns the Account associated with the specified address.
    Account* GetAccount(const Address& address) override;

    const std::unordered_map<Address, Account> m_addressToAccount& GetAddressToAccount();
};

#endif // __ACCOUNTSTORE_H__