/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <vector>

#include "Validator.h"
#include "libData/AccountData/Account.h"
#include "libMediator/Mediator.h"

using namespace std;
using namespace boost::multiprecision;

Validator::Validator(Mediator& mediator) : m_mediator(mediator) {}

Validator::~Validator() {}

bool Validator::VerifyTransaction(const Transaction& tran) const {
  vector<unsigned char> txnData;
  tran.SerializeCoreFields(txnData, 0);

  return Schnorr::GetInstance().Verify(txnData, tran.GetSignature(),
                                       tran.GetSenderPubKey());
}

bool Validator::CheckCreatedTransaction(const Transaction& tx,
                                        TransactionReceipt& receipt) const {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransaction not expected to be "
                "called from LookUp node.");
    return true;
  }
  // LOG_MARKER();

  // LOG_GENERAL(INFO, "Tran: " << tx.GetTranID());

  // Check if from account is sharded here
  const PubKey& senderPubKey = tx.GetSenderPubKey();
  Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_GENERAL(INFO, "fromAddr not found: " << fromAddr
                                             << ". Transaction rejected: "
                                             << tx.GetTranID());
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    return false;
  }

  return AccountStore::GetInstance().UpdateAccountsTemp(
      m_mediator.m_currentEpochNum, m_mediator.m_node->getNumShards(),
      m_mediator.m_ds->m_mode != DirectoryService::Mode::IDLE, tx, receipt);
}

bool Validator::CheckCreatedTransactionFromLookup(const Transaction& tx) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "Validator::CheckCreatedTransactionFromLookup not expected "
                "to be called from LookUp node.");
    return true;
  }

  // LOG_MARKER();

  // Check if from account is sharded here
  const PubKey& senderPubKey = tx.GetSenderPubKey();
  Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  unsigned int shardId = m_mediator.m_node->GetShardId();
  unsigned int numShards = m_mediator.m_node->getNumShards();
  unsigned int correct_shard_from =
      Transaction::GetShardIndex(fromAddr, numShards);

  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
    if (correct_shard_from != shardId) {
      LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "This tx is not sharded to me!"
                    << " From Account  = 0x" << fromAddr
                    << " Correct shard = " << correct_shard_from
                    << " This shard    = " << m_mediator.m_node->GetShardId());
      return false;
      // // Transaction created from the GenTransactionBulk will be rejected
      // // by all shards but one. Next line is commented to avoid this
      // return false;
    }

    if (tx.GetData().size() > 0 && tx.GetToAddr() != NullAddress) {
      unsigned int correct_shard_to =
          Transaction::GetShardIndex(tx.GetToAddr(), numShards);
      if (correct_shard_to != correct_shard_from) {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "The fromShard " << correct_shard_from << " and toShard "
                                   << correct_shard_to
                                   << " is different for the call SC txn");
        return false;
      }
    }
  }

  if (!VerifyTransaction(tx)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Signature incorrect: " << fromAddr << ". Transaction rejected: "
                                      << tx.GetTranID());
    return false;
  }

  // Check if from account exists in local storage
  if (!AccountStore::GetInstance().IsAccountExist(fromAddr)) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "fromAddr not found: " << fromAddr << ". Transaction rejected: "
                                     << tx.GetTranID());
    return false;
  }

  // Check if transaction amount is valid
  if (AccountStore::GetInstance().GetBalance(fromAddr) < tx.GetAmount()) {
    LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Insufficient funds in source account!"
                  << " From Account  = 0x" << fromAddr << " Balance = "
                  << AccountStore::GetInstance().GetBalance(fromAddr)
                  << " Debit Amount = " << tx.GetAmount());
    return false;
  }

  return true;
}
