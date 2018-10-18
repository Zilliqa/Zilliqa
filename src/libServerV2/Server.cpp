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

using namespace std;
using namespace ZilliqaMessage;

CircularArray<std::string> Server::m_RecentTransactions;
std::mutex Server::m_mutexRecentTxns;

const unsigned int PAGE_SIZE = 10;
const unsigned int NUM_PAGES_CACHE = 2;
const unsigned int TXN_PAGE_SIZE = 100;

//[warning] do not make this constant too big as it loops over blockchain
const unsigned int REF_BLOCK_DIFF = 5;

// Forward declarations (implementation in libMessage).
void ProtobufToTransaction(const ProtoTransaction& protoTransaction, Transaction& transaction);
void TransactionToProtobuf(const Transaction& transaction, ProtoTransaction& protoTransaction);
void ProtobufToDSBlock(const ProtoDSBlock& protoDSBlock, DSBlock& dsBlock);
void DSBlockToProtobuf(const DSBlock& dsBlock, ProtoDSBlock& protoDSBlock);
void TxBlockToProtobuf(const TxBlock& txBlock, ProtoTxBlock& protoTxBlock);
void NumberToProtobufByteArray(const boost::multiprecision::uint256_t& number, ByteArray& byteArray);


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


DefaultResponse Server::GetClientVersion() {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::GetNetworkId() {
  DefaultResponse ret;
  ret.set_result("TestNet");
  return ret;
}


DefaultResponse Server::GetProtocolVersion() {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::GetGasPrice() {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::GetStorageAt([[gnu::unused]] GetStorageAtRequest& request) {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::GetBlockTransactionCount([[gnu::unused]] GetBlockTransactionCountRequest& request) {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::GetTransactionReceipt([[gnu::unused]] GetTransactionRequest& request) {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::isNodeSyncing() {
  DefaultResponse ret;
  return ret;
}


DefaultResponse Server::isNodeMining() {
  DefaultResponse ret;
  return ret;
}

DefaultResponse Server::GetHashrate() {
  DefaultResponse ret;
  return ret;
}



////////////////////////////////////////////////////////////////////////////////////////



CreateTransactionResponse Server::CreateTransaction(CreateTransactionRequest& request) {
  LOG_MARKER();

  CreateTransactionResponse ret;

  try {

    // Convert Protobuf transaction to Transaction.
    Transaction tx;
    if (!request.has_tx()) {
      ret.set_error("Tx not present in request");
      return ret;
    }
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


GetTransactionResponse Server::GetTransaction(GetTransactionRequest& request) {
  LOG_MARKER();

  GetTransactionResponse ret;

  try {
    // Check if txhash is set in request.
    if (!request.has_txhash()) {
      ret.set_error("Tx hash not set in request");
      return ret;
    }

    // Validate the txhash.
    TxBodySharedPtr tptr;
    TxnHash tranHash(request.txhash());
    if (request.txhash().size() != TRAN_HASH_SIZE * 2) {
      ret.set_error("Size not appropriate");
      return ret;
    }

    // Retrieve the tx if it exists.
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent || tptr == nullptr) {
      ret.set_error("Txn Hash not Present");
      return ret;
    }

    // Convert Transaction to proto.
    ProtoTransaction protoTx;
    TransactionToProtobuf(tptr->GetTransaction(), protoTx);
    ret.set_allocated_tx(&protoTx);

    ret.set_receipt(tptr->GetTransactionReceipt().GetString());

  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << request.txhash());
    ret.set_error("Unable to Process");
  }

  return ret;
}


GetDSBlockResponse Server::GetDsBlock(ProtoBlockNum& protoBlockNum) {
  LOG_MARKER();

  GetDSBlockResponse ret;

  try {
    // Convert string "blocknum" to ULL.
    if (!protoBlockNum.has_blocknum()) {
      ret.set_error("Blocknum not set in request");
      return ret;
    }
    uint64_t blockNum = stoull(protoBlockNum.blocknum());

    // Get the DS block.
    DSBlock dsblock = m_mediator.m_dsBlockChain.GetBlock(blockNum);

    // Convert DSBlock to proto.
    ProtoDSBlock protoDSBlock;
    DSBlockToProtobuf(dsblock, protoDSBlock);
    ret.set_allocated_dsblock(&protoDSBlock);
  } catch (const char* msg) {
    ret.set_error(msg);
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "Error " << e.what());
    ret.set_error("String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Unable to Process");
  }

  return ret;
}


GetTxBlockResponse Server::GetTxBlock(ProtoBlockNum& protoBlockNum) {
  LOG_MARKER();

  GetTxBlockResponse ret;

  try {
    // Convert string "blocknum" to ULL.
    if (!protoBlockNum.has_blocknum()) {
      ret.set_error("blocknum not set in request");
      return ret;
    }
    uint64_t blockNum = stoull(protoBlockNum.blocknum());

    // Get the tx block.
    TxBlock txblock = m_mediator.m_txBlockChain.GetBlock(blockNum);

    // Convert txblock to proto.
    ProtoTxBlock protoTxBlock;
    TxBlockToProtobuf(txblock, protoTxBlock);
    ret.set_allocated_txblock(&protoTxBlock);
  } catch (const char* msg) {
    ret.set_error(msg);
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "Error " << e.what());
    ret.set_error("String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Unable to Process");
  }

  return ret;
}


GetDSBlockResponse Server::GetLatestDsBlock() {
  LOG_MARKER();

  GetDSBlockResponse ret;

  // Retrieve the latest DS block.
  DSBlock dsblock = m_mediator.m_dsBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << dsblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        "
                        << dsblock.GetHeader().GetTimestamp().str());

  // Convert DSBlock to proto.
  ProtoDSBlock protoDSBlock;
  DSBlockToProtobuf(dsblock, protoDSBlock);
  ret.set_allocated_dsblock(&protoDSBlock);

  return ret;
}


GetTxBlockResponse Server::GetLatestTxBlock() {
  LOG_MARKER();

  GetTxBlockResponse ret;

  // Get the latest tx block.
  TxBlock txblock = m_mediator.m_txBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << txblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        "
                        << txblock.GetHeader().GetTimestamp().str());

  // Convert txblock to proto.
  ProtoTxBlock protoTxBlock;
  TxBlockToProtobuf(txblock, protoTxBlock);
  ret.set_allocated_txblock(&protoTxBlock);

  return ret;
}


GetBalanceResponse Server::GetBalance(ProtoAddress& protoAddress) {
  LOG_MARKER();

  GetBalanceResponse ret;

  try {
    if (!protoAddress.has_address()) {
      ret.set_error("Address not set in request");
      return ret;
    }

    if (protoAddress.address().size() != ACC_ADDR_SIZE * 2) {
      ret.set_error("Address size not appropriate");
      return ret;
    }

    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(protoAddress.address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account != nullptr) {
      boost::multiprecision::uint256_t balance = account->GetBalance();
      boost::multiprecision::uint256_t nonce = account->GetNonce();

      ByteArray balanceByteArray;
      NumberToProtobufByteArray(balance, balanceByteArray);
      ret.set_allocated_balance(&balanceByteArray);

      ByteArray nonceByteArray;
      NumberToProtobufByteArray(nonce, nonceByteArray);
      ret.set_allocated_nonce(&nonceByteArray);

      LOG_GENERAL(INFO, "balance " << balance.str() << " nonce: "
                                   << nonce.convert_to<unsigned int>());
    } else if (account == nullptr) {
      ByteArray balanceByteArray;
      NumberToProtobufByteArray(0, balanceByteArray);
      ret.set_allocated_balance(&balanceByteArray);

      ByteArray nonceByteArray;
      NumberToProtobufByteArray(0, nonceByteArray);
      ret.set_allocated_nonce(&nonceByteArray);
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}


GetSmartContractStateResponse Server::GetSmartContractState(ProtoAddress& protoAddress) {
  LOG_MARKER();

  GetSmartContractStateResponse ret;

  try {
    if (!protoAddress.has_address()) {
      ret.set_error("Address not set in request");
      return ret;
    }

    if (protoAddress.address().size() != ACC_ADDR_SIZE * 2) {
      ret.set_error("Address size inappropriate");
      return ret;
    }

    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(protoAddress.address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret.set_error("Address does not exist");
      return ret;
    }

    if (!account->isContract()) {
      ret.set_error("Address is not a contract account");
      return ret;
    }

    //return account->GetStorageJson();
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}


GetSmartContractCodeResponse GetSmartContractCode(ProtoAddress& protoAddress) {
  LOG_MARKER();

  GetSmartContractCodeResponse ret;

  try {
    if (!protoAddress.has_address()) {
      ret.set_error("Address not set in request");
      return ret;
    }

    if (protoAddress.address().size() != ACC_ADDR_SIZE * 2) {
      ret.set_error("Address size inappropriate");
      return ret;
    }

    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(protoAddress.address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret.set_error("Address does not exist");
      return ret;
    }

    if (!account->isContract()) {
      ret.set_error("Address is not a contract account");
      return ret;
    }

    ret.set_smartcontractcode(DataConversion::CharArrayToString(account->GetCode()));
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}


StringResponse Server::GetContractAddressFromTransactionID(ProtoTranId& protoTranId) {
  LOG_MARKER();

  StringResponse ret;

  try {
    if (!protoTranId.has_tranid()) {
      ret.set_result("Tran id not set in request");
      return ret;
    }

    TxBodySharedPtr tptr;
    TxnHash tranHash(protoTranId.tranid());
    if (protoTranId.tranid().size() != TRAN_HASH_SIZE * 2) {
      ret.set_result("Size not appropriate");
      return ret;
    }

    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent) {
      ret.set_result("Txn Hash not Present");
      return ret;
    }

    const Transaction& tx = tptr->GetTransaction();
    if (tx.GetData().empty() || tx.GetToAddr() == NullAddress) {
      ret.set_result("ID not a contract txn");
      return ret;
    }

    ret.set_result(Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce() - 1).hex());
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what() << " Input " << protoTranId.tranid());
    ret.set_result("Unable to process");
  }

  return ret;
}


UIntResponse Server::GetNumPeers() {
  LOG_MARKER();

  unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();
  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  UIntResponse ret;
  ret.set_result(numPeers + m_mediator.m_DSCommittee->size());
  return ret;
}


StringResponse Server::GetNumTxBlocks() {
  LOG_MARKER();

  StringResponse ret;
  ret.set_result(to_string(m_mediator.m_txBlockChain.GetBlockCount()));
  return ret;
}


StringResponse Server::GetNumDSBlocks() {
  LOG_MARKER();

  StringResponse ret;
  ret.set_result(to_string(m_mediator.m_dsBlockChain.GetBlockCount()));
  return ret;
}


StringResponse Server::GetNumTransactions() {
  LOG_MARKER();

  uint64_t currBlock =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  if (m_BlockTxPair.first < currBlock) {
    for (uint64_t i = m_BlockTxPair.first + 1; i <= currBlock; i++) {
      m_BlockTxPair.second +=
          m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
    }
  }
  m_BlockTxPair.first = currBlock;

  StringResponse ret;
  ret.set_result(m_BlockTxPair.second.str());
  return ret;
}
