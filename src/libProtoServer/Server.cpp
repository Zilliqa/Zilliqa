/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <boost/multiprecision/cpp_dec_float.hpp>
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
int ProtobufToTransaction(const ProtoTransaction& protoTransaction,
                          Transaction& transaction);
void TransactionToProtobuf(const Transaction& transaction,
                           ProtoTransaction& protoTransaction);
void ProtobufToDSBlock(const ProtoDSBlock& protoDSBlock, DSBlock& dsBlock);
void DSBlockToProtobuf(const DSBlock& dsBlock, ProtoDSBlock& protoDSBlock);
void TxBlockToProtobuf(const TxBlock& txBlock, ProtoTxBlock& protoTxBlock);

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

////////////////////////////////////////////////////////////////////////
// Auxillary functions.
////////////////////////////////////////////////////////////////////////

uint256_t Server::GetNumTransactions(uint64_t blockNum) {
  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (blockNum >= currBlockNum) {
    return 0;
  }

  uint64_t i, res = 0;
  for (i = blockNum + 1; i <= currBlockNum; i++) {
    res += m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
  }

  return res;
}

void Server::AddToRecentTransactions(const dev::h256& txhash) {
  lock_guard<mutex> g(m_mutexRecentTxns);
  m_RecentTransactions.insert_new(m_RecentTransactions.size(), txhash.hex());
}

////////////////////////////////////////////////////////////////////////////////////////

DefaultResponse Server::GetClientVersion() {
  DefaultResponse ret;
  return ret;
}

DefaultResponse Server::GetNetworkId() {
  DefaultResponse ret;
  ret.set_result(to_string(CHAIN_ID));
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

DefaultResponse Server::GetStorageAt([
    [gnu::unused]] GetStorageAtRequest& request) {
  DefaultResponse ret;
  return ret;
}

DefaultResponse Server::GetBlockTransactionCount([
    [gnu::unused]] GetBlockTransactionCountRequest& request) {
  DefaultResponse ret;
  return ret;
}

DefaultResponse Server::GetTransactionReceipt([
    [gnu::unused]] GetTransactionRequest& request) {
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

CreateTransactionResponse Server::CreateTransaction(
    CreateTransactionRequest& request) {
  LOG_MARKER();

  CreateTransactionResponse ret;

  try {
    // Check if request has proto tx object.
    if (!request.has_tx()) {
      ret.set_error("Tx not present in request");
      return ret;
    }

    // Convert ProtoTransaction to Transaction.
    Transaction tx;
    if (ProtobufToTransaction(request.tx(), tx) !=
        0) {  // check if conversion failed.
      ret.set_error("ProtoTransaction to Transaction conversion failed");
      return ret;
    }

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
          ret.set_contractaddress(
              Account::GetAddressForContract(fromAddr, sender->GetNonce())
                  .hex());
        } else {
          ret.set_error("Code is empty and To addr is null");
        }

      } else {
        const Account* account =
            AccountStore::GetInstance().GetAccount(tx.GetToAddr());

        if (account == nullptr) {
          ret.set_error("To Addr is null");
          return ret;
        } else if (!account->isContract()) {
          ret.set_error("Non - contract address called");
          return ret;
        }

        unsigned int to_shard =
            Transaction::GetShardIndex(tx.GetToAddr(), num_shards);

        if (to_shard == shard) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Contract Txn, Shards Match of the sender and reciever");
        } else {
          m_mediator.m_lookup->AddToTxnShardMap(tx, num_shards);
          ret.set_info("Contract Txn, Sent To Ds");
        }
        ret.set_tranid(tx.GetTranID().hex());
      }

      switch (Transaction::GetTransactionType(tx)) {
        case Transaction::CONTRACT_CALL: {
          const Account* account =
              AccountStore::GetInstance().GetAccount(tx.GetToAddr());

          if (account == nullptr) {
            ret.set_error("To Addr is null");
            return ret;
          } else if (!account->isContract()) {
            ret.set_error("Non - contract address called");
            return ret;
          }

          unsigned int to_shard =
              Transaction::GetShardIndex(tx.GetToAddr(), num_shards);

          if (to_shard == shard) {
            m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
            ret.set_info(
                "Contract Txn, Shards Match of the sender and reciever");
          } else {
            m_mediator.m_lookup->AddToTxnShardMap(tx, num_shards);
            ret.set_info("Contract Txn, Sent To Ds");
          }
          ret.set_tranid(tx.GetTranID().hex());
          break;
        }
        case Transaction::CONTRACT_CREATION: {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Contract Creation txn, sent to shard");
          ret.set_tranid(tx.GetTranID().hex());
          ret.set_contractaddress(
              Account::GetAddressForContract(fromAddr, sender->GetNonce())
                  .hex());
          break;
        }
        case Transaction::NON_CONTRACT: {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret.set_info("Non-contract txn, sent to shard");
          ret.set_tranid(tx.GetTranID().hex());
          break;
        }
        default:
          LOG_GENERAL(WARNING, "Type of transaction is not recognizable");
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
    if (request.txhash().size() != TRAN_HASH_SIZE * 2) {
      ret.set_error("Size not appropriate");
      return ret;
    }

    // Retrieve the tx if it exists.
    TxBodySharedPtr tptr;
    TxnHash tranHash(request.txhash());
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
    if (!protoBlockNum.has_blocknum()) {
      ret.set_error("Blocknum not set in request");
      return ret;
    }
    uint64_t blockNum = protoBlockNum.blocknum();

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
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Unable to Process");
  }

  return ret;
}

GetTxBlockResponse Server::GetTxBlock(ProtoBlockNum& protoBlockNum) {
  LOG_MARKER();

  GetTxBlockResponse ret;

  try {
    if (!protoBlockNum.has_blocknum()) {
      ret.set_error("blocknum not set in request");
      return ret;
    }
    uint64_t blockNum = protoBlockNum.blocknum();

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
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum.blocknum());
    ret.set_error("Unable to Process");
  }

  return ret;
}

GetDSBlockResponse Server::GetLatestDsBlock() {
  LOG_MARKER();

  GetDSBlockResponse ret;

  // Retrieve the latest DS block.
  DSBlock dsblock = m_mediator.m_dsBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "BlockNum " << dsblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << dsblock.GetTimestamp());

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

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "BlockNum " << txblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << txblock.GetTimestamp());

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

    bytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(protoAddress.address(), tmpaddr)) {
      ret.set_error("Address is not valid");
      return ret;
    }

    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account != nullptr) {
      uint128_t balance = account->GetBalance();
      ret.set_balance(balance.str());

      uint128_t nonce = account->GetNonce();
      ret.set_nonce(nonce.str());

      LOG_GENERAL(INFO, "balance " << balance.str() << " nonce: "
                                   << nonce.convert_to<unsigned int>());
    } else if (account == nullptr) {
      ret.set_balance("0");
      ret.set_nonce("0");
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}

GetSmartContractStateResponse Server::GetSmartContractState(
    ProtoAddress& protoAddress) {
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

    bytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(protoAddress.address(), tmpaddr)) {
      ret.set_error("Address is not valid");
      return ret;
    }

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

    pair<Json::Value, Json::Value> roots;
    if (!account->GetStorageJson(roots, false)) {
      ret.set_error("Scilla_version not set properly");
      return ret;
    }

    ret.set_initjson(roots.first.toStyledString());
    ret.set_storagejson(roots.second.toStyledString());
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress.address());
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

    bytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(protoAddress.address(), tmpaddr)) {
      ret.set_error("Address is not valid");
      return ret;
    }

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

    ret.set_smartcontractcode(
        DataConversion::CharArrayToString(account->GetCode()));
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}

GetSmartContractResponse Server::GetSmartContracts(ProtoAddress& protoAddress) {
  LOG_MARKER();

  GetSmartContractResponse ret;

  try {
    if (!protoAddress.has_address()) {
      ret.set_error("Address not set in request");
      return ret;
    }

    if (protoAddress.address().size() != ACC_ADDR_SIZE * 2) {
      ret.set_error("Address size inappropriate");
      return ret;
    }

    bytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(protoAddress.address(), tmpaddr)) {
      ret.set_error("Address is not valid");
      return ret;
    }

    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret.set_error("Address does not exist");
      return ret;
    }

    if (account->isContract()) {
      ret.set_error("A contract account queried");
      return ret;
    }

    uint64_t nonce = account->GetNonce();
    //[TODO] find out a more efficient way (using storage)

    for (uint64_t i = 0; i < nonce; i++) {
      Address contractAddr = Account::GetAddressForContract(addr, i);
      const Account* contractAccount =
          AccountStore::GetInstance().GetAccount(contractAddr);

      if (contractAccount == nullptr || !contractAccount->isContract()) {
        continue;
      }

      auto protoContractAccount = ret.add_address();
      protoContractAccount->set_address(contractAddr.hex());
      protoContractAccount->set_state(
          contractAccount->GetStateJson(false).toStyledString());
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress.address());
    ret.set_error("Unable To Process");
  }

  return ret;
}

StringResponse Server::GetContractAddressFromTransactionID(
    ProtoTxId& protoTxId) {
  LOG_MARKER();

  StringResponse ret;

  try {
    if (!protoTxId.has_txid()) {
      ret.set_result("Tran id not set in request");
      return ret;
    }

    TxBodySharedPtr tptr;
    TxnHash tranHash(protoTxId.txid());
    if (protoTxId.txid().size() != TRAN_HASH_SIZE * 2) {
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

    ret.set_result(
        Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce() - 1)
            .hex());
  } catch (exception& e) {
    LOG_GENERAL(WARNING,
                "[Error]" << e.what() << " Input " << protoTxId.txid());
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

DoubleResponse Server::GetTransactionRate() {
  LOG_MARKER();

  DoubleResponse ret;

  uint64_t refBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  uint64_t refTimeTx = 0;

  if (refBlockNum <= REF_BLOCK_DIFF) {
    if (refBlockNum <= 1) {
      LOG_GENERAL(INFO, "Not enough blocks for information");
      return ret;
    } else {
      refBlockNum = 1;
      // In case there are less than REF_DIFF_BLOCKS blocks in blockchain,
      // blocknum 1 can be ref block;
    }
  } else {
    refBlockNum = refBlockNum - REF_BLOCK_DIFF;
  }

  boost::multiprecision::cpp_dec_float_50 numTxns(
      Server::GetNumTransactions(refBlockNum));
  LOG_GENERAL(INFO, "Num Txns: " << numTxns);

  try {
    TxBlock tx = m_mediator.m_txBlockChain.GetBlock(refBlockNum);
    refTimeTx = tx.GetTimestamp();
  } catch (const char* msg) {
    if (string(msg) == "Blocknumber Absent") {
      LOG_GENERAL(INFO, "Error in fetching ref block");
    }

    return ret;
  }

  uint64_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetTimestamp() - refTimeTx;

  if (TimeDiff == 0 || refTimeTx == 0) {
    // something went wrong
    LOG_GENERAL(INFO, "TimeDiff or refTimeTx = 0 \n TimeDiff:"
                          << TimeDiff << " refTimeTx:" << refTimeTx);
    return ret;
  }

  numTxns = numTxns * 1000000;  // conversion from microseconds to seconds
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numTxns / TimeDiffFloat;

  ret.set_result(ans.convert_to<double>());
  return ret;
}

DoubleResponse Server::GetDSBlockRate() {
  LOG_MARKER();

  DoubleResponse ret;

  string numDSblockStr = to_string(m_mediator.m_dsBlockChain.GetBlockCount());
  boost::multiprecision::cpp_dec_float_50 numDs(numDSblockStr);

  if (m_StartTimeDs == 0) {  // case when m_StartTime has not been set
    try {
      // Refernce time chosen to be the first block's timestamp
      DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
      m_StartTimeDs = dsb.GetTimestamp();
    } catch (const char* msg) {
      if (string(msg) == "Blocknumber Absent") {
        LOG_GENERAL(INFO, "No DSBlock has been mined yet");
      }

      return ret;
    }
  }

  uint64_t TimeDiff =
      m_mediator.m_dsBlockChain.GetLastBlock().GetTimestamp() - m_StartTimeDs;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return ret;
  }

  // To convert from microSeconds to seconds
  numDs = numDs * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numDs / TimeDiffFloat;

  ret.set_result(ans.convert_to<double>());
  return ret;
}

DoubleResponse Server::GetTxBlockRate() {
  LOG_MARKER();

  DoubleResponse ret;

  string numTxblockStr = to_string(m_mediator.m_txBlockChain.GetBlockCount());
  boost::multiprecision::cpp_dec_float_50 numTx(numTxblockStr);

  if (m_StartTimeTx == 0) {
    try {
      // Reference Time chosen to be first block's timestamp
      TxBlock txb = m_mediator.m_txBlockChain.GetBlock(1);
      m_StartTimeTx = txb.GetTimestamp();
    } catch (const char* msg) {
      if (string(msg) == "Blocknumber Absent") {
        LOG_GENERAL(INFO, "No TxBlock has been mined yet");
      }

      return ret;
    }
  }

  uint64_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetTimestamp() - m_StartTimeTx;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return ret;
  }

  // To convert from microSeconds to seconds
  numTx = numTx * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat(to_string(TimeDiff));
  boost::multiprecision::cpp_dec_float_50 ans = numTx / TimeDiffFloat;

  ret.set_result(ans.convert_to<double>());
  return ret;
}

UInt64Response Server::GetCurrentMiniEpoch() {
  LOG_MARKER();

  UInt64Response ret;
  ret.set_result(m_mediator.m_currentEpochNum);
  return ret;
}

UInt64Response Server::GetCurrentDSEpoch() {
  LOG_MARKER();

  UInt64Response ret;
  ret.set_result(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum());
  return ret;
}

ProtoBlockListing Server::DSBlockListing(ProtoPage& protoPage) {
  LOG_MARKER();

  ProtoBlockListing ret;
  if (protoPage.has_page()) {
    ret.set_error("Page not in request");
    return ret;
  }

  uint64_t currBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;
  ret.set_maxpages(int(maxPages));

  if (m_DSBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      DSBlockHeader dshead = m_mediator.m_dsBlockChain.GetBlock(0).GetHeader();
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      bytes vec;
      dshead.Serialize(vec, 0);
      sha2.Update(vec);
      string vecStr;
      const bytes& resVec = sha2.Finalize();
      DataConversion::Uint8VecToHexStr(resVec, vecStr);
      m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(), vecStr);
    } catch (const char* msg) {
      ret.set_error(msg);
      return ret;
    }
  }

  unsigned int page = protoPage.page();
  if (page > maxPages || page < 1) {
    ret.set_error("Pages out of limit");
    return ret;
  }

  if (currBlockNum > m_DSBlockCache.first) {
    for (uint64_t i = m_DSBlockCache.first + 1; i < currBlockNum; i++) {
      m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(),
                                       m_mediator.m_dsBlockChain.GetBlock(i + 1)
                                           .GetHeader()
                                           .GetPrevHash()
                                           .hex());
    }
    // for the latest block
    DSBlockHeader dshead =
        m_mediator.m_dsBlockChain.GetBlock(currBlockNum).GetHeader();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    bytes vec;
    dshead.Serialize(vec, 0);
    sha2.Update(vec);
    const bytes& resVec = sha2.Finalize();
    string resStr;
    DataConversion::Uint8VecToHexStr(resVec, resStr);
    m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(), resStr);
    m_DSBlockCache.first = currBlockNum;
  }

  unsigned int offset = PAGE_SIZE * (page - 1);
  if (page <= NUM_PAGES_CACHE) {  // can use cache
    uint256_t cacheSize(m_DSBlockCache.second.capacity());
    if (cacheSize > m_DSBlockCache.second.size()) {
      cacheSize = m_DSBlockCache.second.size();
    }

    uint64_t size = m_DSBlockCache.second.size();

    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      auto blockData = ret.add_data();
      blockData->set_hash(m_DSBlockCache.second[size - i - 1]);
      blockData->set_blocknum(int(currBlockNum - i));
    }

  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      auto blockData = ret.add_data();
      blockData->set_hash(
          m_mediator.m_dsBlockChain.GetBlock(currBlockNum - i + 1)
              .GetHeader()
              .GetPrevHash()
              .hex());
      blockData->set_blocknum(int(currBlockNum - i));
    }
  }

  return ret;
}

ProtoBlockListing Server::TxBlockListing(ProtoPage& protoPage) {
  LOG_MARKER();

  ProtoBlockListing ret;
  if (protoPage.has_page()) {
    ret.set_error("Page not in request");
    return ret;
  }

  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;
  ret.set_maxpages(int(maxPages));

  if (m_TxBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      TxBlockHeader txhead = m_mediator.m_txBlockChain.GetBlock(0).GetHeader();
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      bytes vec;
      txhead.Serialize(vec, 0);
      sha2.Update(vec);
      const bytes& resVec = sha2.Finalize();
      string resStr;
      DataConversion::Uint8VecToHexStr(resVec, resStr);
      m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(), resStr);
    } catch (const char* msg) {
      ret.set_error(msg);
      return ret;
    }
  }

  unsigned int page = protoPage.page();
  if (page > maxPages || page < 1) {
    ret.set_error("Pages out of limit");
    return ret;
  }

  if (currBlockNum > m_TxBlockCache.first) {
    for (uint64_t i = m_TxBlockCache.first + 1; i < currBlockNum; i++) {
      m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(),
                                       m_mediator.m_txBlockChain.GetBlock(i + 1)
                                           .GetHeader()
                                           .GetPrevHash()
                                           .hex());
    }
    // for the latest block
    TxBlockHeader txhead =
        m_mediator.m_txBlockChain.GetBlock(currBlockNum).GetHeader();
    SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
    bytes vec;
    txhead.Serialize(vec, 0);
    sha2.Update(vec);
    const bytes& resVec = sha2.Finalize();
    string resStr;
    DataConversion::Uint8VecToHexStr(resVec, resStr);
    m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(), resStr);
    m_TxBlockCache.first = currBlockNum;
  }

  unsigned int offset = PAGE_SIZE * (page - 1);
  if (page <= NUM_PAGES_CACHE) {  // can use cache
    uint256_t cacheSize(m_TxBlockCache.second.capacity());

    if (cacheSize > m_TxBlockCache.second.size()) {
      cacheSize = m_TxBlockCache.second.size();
    }

    uint64_t size = m_TxBlockCache.second.size();
    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      auto blockData = ret.add_data();
      blockData->set_hash(m_TxBlockCache.second[size - i - 1]);
      blockData->set_blocknum(int(currBlockNum - i));
    }

  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      auto blockData = ret.add_data();
      blockData->set_hash(
          m_mediator.m_txBlockChain.GetBlock(currBlockNum - i + 1)
              .GetHeader()
              .GetPrevHash()
              .hex());
      blockData->set_blocknum(int(currBlockNum - i));
    }
  }

  return ret;
}

ProtoBlockChainInfo Server::GetBlockchainInfo() {
  ProtoBlockChainInfo ret;

  ret.set_numpeers(Server::GetNumPeers().result());
  ret.set_numtxblocks(Server::GetNumTxBlocks().result());
  ret.set_numdsblocks(Server::GetNumDSBlocks().result());
  ret.set_numtxns(Server::GetNumTransactions().result());
  ret.set_txrate(Server::GetTransactionRate().result());
  ret.set_txblockrate(Server::GetTxBlockRate().result());
  ret.set_dsblockrate(Server::GetDSBlockRate().result());
  ret.set_currentminiepoch(Server::GetCurrentMiniEpoch().result());
  ret.set_currentdsepoch(Server::GetCurrentDSEpoch().result());
  ret.set_numtxnsdsepoch(Server::GetNumTxnsDSEpoch().result());
  ret.set_numtxnstxepoch(Server::GetNumTxnsTxEpoch().result());

  ProtoShardingStruct sharding = Server::GetShardingStructure();
  ret.set_allocated_shardingstructure(&sharding);

  return ret;
}

ProtoTxHashes Server::GetRecentTransactions() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexRecentTxns);

  uint64_t actualSize(m_RecentTransactions.capacity());
  if (actualSize > m_RecentTransactions.size()) {
    actualSize = m_RecentTransactions.size();
  }

  ProtoTxHashes ret;
  ret.set_number(int(actualSize));

  uint64_t size = m_RecentTransactions.size();
  for (uint64_t i = 0; i < actualSize; i++) {
    auto txhash = ret.add_txhashes();
    txhash->set_txhash(m_RecentTransactions[size - i - 1]);
  }

  return ret;
}

ProtoShardingStruct Server::GetShardingStructure() {
  LOG_MARKER();

  ProtoShardingStruct ret;

  try {
    auto shards = m_mediator.m_lookup->GetShardPeers();

    unsigned int num_shards = shards.size();

    if (num_shards == 0) {
      ret.set_error("No shards yet");
    } else {
      for (unsigned int i = 0; i < num_shards; i++) {
        ret.set_numpeers(i, static_cast<unsigned int>(shards[i].size()));
      }
    }

  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret.set_error("Unable to process");
  }

  return ret;
}

UIntResponse Server::GetNumTxnsTxEpoch() {
  LOG_MARKER();

  UIntResponse ret;

  try {
    ret.set_result(
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetNumTxs());
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret.set_result(0);
  }

  return ret;
}

StringResponse Server::GetNumTxnsDSEpoch() {
  LOG_MARKER();

  StringResponse ret;

  try {
    auto latestTxBlock = m_mediator.m_txBlockChain.GetLastBlock().GetHeader();
    auto latestTxBlockNum = latestTxBlock.GetBlockNum();
    auto latestDSBlockNum = latestTxBlock.GetDSBlockNum();

    if (latestTxBlockNum > m_TxBlockCountSumPair.first) {
      // Case where the DS Epoch is same
      if (m_mediator.m_txBlockChain.GetBlock(m_TxBlockCountSumPair.first)
              .GetHeader()
              .GetDSBlockNum() == latestDSBlockNum) {
        for (auto i = latestTxBlockNum; i > m_TxBlockCountSumPair.first; i--) {
          m_TxBlockCountSumPair.second +=
              m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
        }

      } else {  // Case if DS Epoch Changed
        m_TxBlockCountSumPair.second = 0;

        for (auto i = latestTxBlockNum; i > m_TxBlockCountSumPair.first; i--) {
          if (m_mediator.m_txBlockChain.GetBlock(i)
                  .GetHeader()
                  .GetDSBlockNum() < latestDSBlockNum) {
            break;
          }
          m_TxBlockCountSumPair.second +=
              m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
        }
      }

      m_TxBlockCountSumPair.first = latestTxBlockNum;
    }

    ret.set_result(m_TxBlockCountSumPair.second.str());

  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret.set_result("0");
  }

  return ret;
}
