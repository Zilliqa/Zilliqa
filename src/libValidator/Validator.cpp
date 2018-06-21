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

#include <vector>

#include "Validator.h"
#include "libMediator/Mediator.h"

using namespace std;
using namespace boost::multiprecision;

MediatorAdapter::MediatorAdapter(Mediator& mediator)
    : m_mediator(mediator)
{
}

MediatorAdapter::~MediatorAdapter() {}

unsigned int MediatorAdapter::getShardID() const
{
    return m_mediator.m_node->getShardID();
}

unsigned int MediatorAdapter::getNumShards() const
{
    return m_mediator.m_node->getNumShards();
}

string MediatorAdapter::currentEpochNumAsString() const
{
    return to_string(m_mediator.m_currentEpochNum);
}

DefaultAccountStoreView::~DefaultAccountStoreView() {}

bool DefaultAccountStoreView::DoesAccountExist(const Address& address)
{
    return AccountStore::GetInstance().DoesAccountExist(address);
}

void DefaultAccountStoreView::AddAccount(const Address& address,
                                         const Account& account)
{
    AccountStore::GetInstance().AddAccount(address, account);
}

boost::multiprecision::uint256_t
DefaultAccountStoreView::GetBalance(const Address& address)
{
    return AccountStore::GetInstance().GetBalance(address);
}

boost::multiprecision::uint256_t
DefaultAccountStoreView::GetNonce(const Address& address)
{
    return AccountStore::GetInstance().GetNonce(address);
}

Validator::Validator(MediatorView& mediator, AccountStoreView& accountStoreView)
#ifndef IS_LOOKUP_NODE
    : m_mediator(mediator)
    , m_accountStoreView(accountStoreView)
#endif // IS_LOOKUP_NODE
{
}

Validator::~Validator() {}

bool Validator::VerifyTransaction(const Transaction& tran) const
{
    vector<unsigned char> txnData;
    tran.SerializeCoreFields(txnData, 0);

    return Schnorr::GetInstance().Verify(txnData, tran.GetSignature(),
                                         tran.GetSenderPubKey());
}

void Validator::CleanVariables()
{
    // Clear m_txnNonceMap
    {
        lock_guard<mutex> g(m_mutexTxnNonceMap);
        m_txnNonceMap.clear();
    }
}

#ifndef IS_LOOKUP_NODE
bool Validator::CheckCreatedTransaction(const Transaction& tx)
{
    return checkCreatedTransactionCommon(tx, false);
}

bool Validator::CheckCreatedTransactionFromLookup(const Transaction& tx)
{
    return checkCreatedTransactionCommon(tx, true);
}

bool Validator::checkCreatedTransactionCommon(const Transaction& tx,
                                              bool checkNonce)
{
    LOG_MARKER();

    // Check if from account is sharded here
    const PubKey& senderPubKey = tx.GetSenderPubKey();
    Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    unsigned int shardID = m_mediator.getShardID();
    unsigned int numShards = m_mediator.getNumShards();
    unsigned int correct_shard
        = Transaction::GetShardIndex(fromAddr, numShards);
    if (correct_shard != shardID)
    {
        LOG_EPOCH(WARNING, m_mediator.currentEpochNumAsString().c_str(),
                  "This tx is not sharded to me!"
                      << " From Account  = 0x" << fromAddr
                      << " Correct shard = " << correct_shard
                      << " This shard    = " << m_mediator.getShardID());
        // Transaction created from the GenTransactionBulk will be rejected
        // by all shards but one. Comment the following line to avoid this
        return false;
    }

    // Check if from account exists in local storage
    if (!m_accountStoreView.DoesAccountExist(fromAddr))
    {
        LOG_GENERAL(INFO,
                    "fromAddr not found: " << fromAddr
                                           << ". Transaction rejected: "
                                           << tx.GetTranID());
        return false;
    }

    if (checkNonce && !checkFromAccountNonce(tx))
    {
        return false;
    }

    // Check if to account exists in local storage
    const Address& toAddr = tx.GetToAddr();
    if (!m_accountStoreView.DoesAccountExist(toAddr))
    {
        LOG_GENERAL(INFO, "New account is added: " << toAddr);
        m_accountStoreView.AddAccount(toAddr, {0, 0});
        // Note: Above, we implicitly create an Account (second argument)
        //       which in turn does Account::InitStorage().
    }

    // Check if transaction amount is valid
    if (m_accountStoreView.GetBalance(fromAddr) < tx.GetAmount())
    {
        LOG_EPOCH(WARNING, m_mediator.currentEpochNumAsString().c_str(),
                  "Insufficient funds in source account!"
                      << " From Account  = 0x" << fromAddr << " Balance = "
                      << m_accountStoreView.GetBalance(fromAddr)
                      << " Debit Amount = " << tx.GetAmount());
        return false;
    }

    return true;
}

bool Validator::checkFromAccountNonce(const Transaction& tx)
{
    LOG_MARKER();

    Address fromAddr = Account::GetAddressFromPublicKey(tx.GetSenderPubKey());

    // Check from account nonce
    lock_guard<mutex> g(m_mutexTxnNonceMap);
    if (m_txnNonceMap.find(fromAddr) == m_txnNonceMap.end())
    {
        LOG_GENERAL(INFO, "Txn from " << fromAddr << "is new.");

        if (tx.GetNonce() != m_accountStoreView.GetNonce(fromAddr) + 1)
        {
            LOG_EPOCH(WARNING, m_mediator.currentEpochNumAsString().c_str(),
                      "Tx nonce not in line with account state!"
                          << " From Account = 0x" << fromAddr
                          << " Account Nonce = "
                          << m_accountStoreView.GetNonce(fromAddr)
                          << " Expected Tx Nonce = "
                          << m_accountStoreView.GetNonce(fromAddr) + 1
                          << " Actual Tx Nonce = " << tx.GetNonce());
            return false;
        }
        m_txnNonceMap.insert(make_pair(fromAddr, tx.GetNonce()));
    }
    else
    {
        if (tx.GetNonce() != m_txnNonceMap.at(fromAddr) + 1)
        {
            LOG_EPOCH(WARNING, m_mediator.currentEpochNumAsString().c_str(),
                      "Tx nonce not in line with account state!"
                          << " From Account = 0x" << fromAddr
                          << " Account Nonce = " << m_txnNonceMap.at(fromAddr)
                          << " Expected Tx Nonce = "
                          << m_txnNonceMap.at(fromAddr) + 1
                          << " Actual Tx Nonce   = " << tx.GetNonce());
            return false;
        }
        m_txnNonceMap.at(fromAddr) += 1;
    }
    return true;
}

#endif // IS_LOOKUP_NODE
