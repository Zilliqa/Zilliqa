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

#ifndef __VALIDATOR_H__
#define __VALIDATOR_H__

#include <string>

#include "libData/AccountData/Transaction.h"

class Mediator;

class ValidatorBase
{
public:
    virtual ~ValidatorBase() {}
    virtual std::string name() const = 0;

    /// Verifies the transaction w.r.t given pubKey and signature
    virtual bool VerifyTransaction(const Transaction& tran) const = 0;

    virtual void CleanVariables() = 0;

#ifndef IS_LOOKUP_NODE
    virtual bool CheckCreatedTransaction(const Transaction& tx) const = 0;
    virtual bool CheckCreatedTransactionFromLookup(const Transaction& tx) = 0;
#endif // IS_LOOKUP_NODE
};

class Validator : public ValidatorBase
{

    // Nonce information
    std::mutex m_mutexTxnNonceMap;
    std::unordered_map<Address, boost::multiprecision::uint256_t> m_txnNonceMap;

public:
    Validator(Mediator& mediator);
    ~Validator();
    std::string name() const override { return "Validator"; }
    bool VerifyTransaction(const Transaction& tran) const override;
    void CleanVariables() override;

#ifndef IS_LOOKUP_NODE
    bool CheckCreatedTransaction(const Transaction& tx) const override;
    bool CheckCreatedTransactionFromLookup(const Transaction& tx) override;
#endif // IS_LOOKUP_NODE

    Mediator* m_mediator;
};

#endif // __VALIDATOR_H__