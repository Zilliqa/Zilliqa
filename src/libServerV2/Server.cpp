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

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <iostream>

#include "Server.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace jsonrpc;
using namespace std;
using namespace ZilliqaMessage;

CircularArray<std::string> Server::m_RecentTransactions;
std::mutex Server::m_mutexRecentTxns;

const unsigned int PAGE_SIZE = 10;
const unsigned int NUM_PAGES_CACHE = 2;
const unsigned int TXN_PAGE_SIZE = 100;

//[warning] do not make this constant too big as it loops over blockchain
const unsigned int REF_BLOCK_DIFF = 5;

// Forward declaration.
void ProtobufToTransaction(const ProtoTransaction& protoTransaction, Transaction& transaction);

Server::Server(Mediator& mediator) : m_mediator(mediator) {
  m_StartTimeTx = 0;
  m_StartTimeDs = 0;
  m_DSBlockCache.first = 0;
  m_DSBlockCache.second.resize(NUM_PAGES_CACHE * PAGE_SIZE);
  m_TxBlockCache.first = 0;
  m_TxBlockCache.second.resize(NUM_PAGES_CACHE * PAGE_SIZE);
  m_RecentTransactions.resize(TXN_PAGE_SIZE);
  m_TxBlockCountSumPair.first = 0;
  m_TxBlockCountSumPair.second = 0;
}

Server::~Server() {
  // destructor
}


CreateTransactionResponse Server::CreateTransaction(CreateTransactionRequest& request) {
  LOG_MARKER();

  CreateTransactionResponse ret;

  try {

    // Convert Protobuf transaction to Transaction.
    Transaction tx;
    ProtobufToTransaction(request.tx(), tx);

    // NOTE: Do we need to check if ProtobufToTransaction() failed?

    // Verify the transaction.
    if (!m_mediator.m_validator->VerifyTransaction(tx)) {
      ret.set_error("Unable to Verify Transaction");
      return ret;
    }

    unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

    const PubKey& senderPubKey = tx.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

    if (sender == nullptr) {
      ret.set_error("The sender of the txn is null");
      return ret;
    }

    if (num_shards > 0) {
      unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);

      if (tx.GetData().empty() || tx.GetToAddr() == NullAddress) {
        if (tx.GetData().empty() && tx.GetCode().empty()) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Non-contract txn, sent to shard");
          ret.set_tranid(tx.GetTranID().hex());
        } else if (!tx.GetCode().empty() && tx.GetToAddr() == NullAddress) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Contract Creation txn, sent to shard");
          ret.set_tranid(tx.GetTranID().hex());
          ret.set_contractaddress(Account::GetAddressForContract(fromAddr, sender->GetNonce()).hex());
        } else {
          ret.set_error("Code is empty and To addr is null");
        }

      } else {
        const Account* account = AccountStore::GetInstance().GetAccount(tx.GetToAddr());

        if (account == nullptr) {
          ret.set_error("To Addr is null");
          return ret;
        } else if (!account->isContract()) {
          ret.set_error("Non - contract address called");
          return ret;
        }

        unsigned int to_shard = Transaction::GetShardIndex(tx.GetToAddr(), num_shards);

        if (to_shard == shard) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Contract Txn, Shards Match of the sender and reciever");
        } else {
          m_mediator.m_lookup->AddToTxnShardMap(tx, num_shards);
          ret.set_info("Contract Txn, Sent To Ds");
        }
        ret.set_tranid(tx.GetTranID().hex());
      }

    } else {
      LOG_GENERAL(INFO, "No shards yet");
      ret.set_error("Could not create Transaction");
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: ");
    ret.set_error("Unable to Process");
  }

  return ret;
}
