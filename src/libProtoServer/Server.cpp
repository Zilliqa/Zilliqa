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

#include <grpc/grpc.h>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <mutex>

#include "Server.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"
#include "libData/DataStructures/CircularArray.h"
#include "libMediator/Mediator.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace grpc;
using namespace std;
using namespace ZilliqaMessage;

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

ProtoServer::ProtoServer(Mediator& mediator, const unsigned int serverPort) : m_mediator(mediator), m_serverPort(serverPort) {
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

void ProtoServer::StartServer() {
  string server_address("0.0.0.0:" + to_string(m_serverPort));

  ServerBuilder builder;
  builder.AddListeningPort(server_address, InsecureServerCredentials());
  builder.RegisterService(this);
  unique_ptr<grpc::Server> server(builder.BuildAndStart());
  cout << "ProtoServer listening on " << server_address << endl;
  server->Wait();
}

void ProtoServer::StopServer() {

}

boost::multiprecision::uint256_t ProtoServer::GetNumTransactions(
    uint64_t blockNum) {
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

void ProtoServer::AddToRecentTransactions(const dev::h256& txhash) {
  lock_guard<mutex> g(m_mutexRecentTxns);
  m_RecentTransactions.insert_new(m_RecentTransactions.size(), txhash.hex());
}

Status ProtoServer::GetClientVersion(ServerContext* context,
                                     const Empty* request,
                                     DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetNetworkId(ServerContext* context, const Empty* request,
                                 DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetProtocolVersion(ServerContext* context,
                                       const Empty* request,
                                       DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetGasPrice(ServerContext* context, const Empty* request,
                                DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetStorageAt(ServerContext* context,
                                 const GetStorageAtRequest* request,
                                 DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetBlockTransactionCount(
    ServerContext* context, const GetBlockTransactionCountRequest* request,
    DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::CreateMessage(ServerContext* context, const Empty* request,
                                  DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetGasEstimate(ServerContext* context, const Empty* request,
                                   DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetTransactionReceipt(ServerContext* context,
                                          const GetTransactionRequest* request,
                                          DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::isNodeSyncing(ServerContext* context, const Empty* request,
                                  DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::isNodeMining(ServerContext* context, const Empty* request,
                                 DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::GetHashrate(ServerContext* context, const Empty* request,
                                DefaultResponse* response) {
  return Status::OK;
}

Status ProtoServer::CreateTransaction(ServerContext* context,
                                      const CreateTransactionRequest* request,
                                      CreateTransactionResponse* ret) {
  LOG_MARKER();

  try {
    // Check if request has proto tx object.
    if (!request->has_tx()) {
      ret->set_error("Tx not present in request");
      return Status::OK;
    }

    // Convert ProtoTransaction to Transaction.
    Transaction tx;
    if (ProtobufToTransaction(request->tx(), tx) !=
        0) {  // check if conversion failed.
      ret->set_error("ProtoTransaction to Transaction conversion failed");
      return Status::OK;
    }

    // Verify the transaction.
    if (!m_mediator.m_validator->VerifyTransaction(tx)) {
      ret->set_error("Unable to Verify Transaction");
      return Status::OK;
    }

    unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

    const PubKey& senderPubKey = tx.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

    if (sender == nullptr) {
      ret->set_error("The sender of the txn is null");
      return Status::OK;
    }

    if (num_shards > 0) {
      unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);

      if (tx.GetData().empty() || tx.GetToAddr() == NullAddress) {
        if (tx.GetData().empty() && tx.GetCode().empty()) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret->set_info("Non-contract txn, sent to shard");
          ret->set_tranid(tx.GetTranID().hex());
        } else if (!tx.GetCode().empty() && tx.GetToAddr() == NullAddress) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret->set_info("Contract Creation txn, sent to shard");
          ret->set_tranid(tx.GetTranID().hex());
          ret->set_contractaddress(
              Account::GetAddressForContract(fromAddr, sender->GetNonce())
                  .hex());
        } else {
          ret->set_error("Code is empty and To addr is null");
        }

      } else {
        const Account* account =
            AccountStore::GetInstance().GetAccount(tx.GetToAddr());

        if (account == nullptr) {
          ret->set_error("To Addr is null");
          return Status::OK;
        } else if (!account->isContract()) {
          ret->set_error("Non - contract address called");
          return Status::OK;
        }

        unsigned int to_shard =
            Transaction::GetShardIndex(tx.GetToAddr(), num_shards);

        if (to_shard == shard) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret->set_info(
              "Contract Txn, Shards Match of the sender and reciever");
        } else {
          m_mediator.m_lookup->AddToTxnShardMap(tx, num_shards);
          ret->set_info("Contract Txn, Sent To Ds");
        }
        ret->set_tranid(tx.GetTranID().hex());
      }

    } else {
      LOG_GENERAL(INFO, "No shards yet");
      ret->set_error("Could not create Transaction");
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: ");
    ret->set_error("Unable to Process");
  }

  return Status::OK;
}

Status ProtoServer::GetTransaction(ServerContext* context,
                                   const GetTransactionRequest* request,
                                   GetTransactionResponse* ret) {
  LOG_MARKER();

  try {
    // Check if txhash is set in request.
    if (!request->has_txhash()) {
      ret->set_error("Tx hash not set in request");
      return Status::OK;
    }

    // Validate the txhash.
    if (request->txhash().size() != TRAN_HASH_SIZE * 2) {
      ret->set_error("Size not appropriate");
      return Status::OK;
    }

    // Retrieve the tx if it exists.
    TxBodySharedPtr tptr;
    TxnHash tranHash(request->txhash());
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent || tptr == nullptr) {
      ret->set_error("Txn Hash not Present");
      return Status::OK;
    }

    // Convert Transaction to proto.
    ProtoTransaction protoTx;
    TransactionToProtobuf(tptr->GetTransaction(), protoTx);
    ret->set_allocated_tx(&protoTx);

    ret->set_receipt(tptr->GetTransactionReceipt().GetString());

  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << request->txhash());
    ret->set_error("Unable to Process");
  }

  return Status::OK;
}

Status ProtoServer::GetDSBlock(ServerContext* context,
                               const ProtoBlockNum* protoBlockNum,
                               GetDSBlockResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoBlockNum->has_blocknum()) {
      ret->set_error("Blocknum not set in request");
      return Status::OK;
    }
    uint64_t blockNum = protoBlockNum->blocknum();

    // Get the DS block.
    DSBlock dsblock = m_mediator.m_dsBlockChain.GetBlock(blockNum);

    // Convert DSBlock to proto.
    ProtoDSBlock protoDSBlock;
    DSBlockToProtobuf(dsblock, protoDSBlock);
    ret->set_allocated_dsblock(&protoDSBlock);
  } catch (const char* msg) {
    ret->set_error(msg);
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "Error " << e.what());
    ret->set_error("String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Unable to Process");
  }

  return Status::OK;
}

Status ProtoServer::GetTxBlock(ServerContext* context,
                               const ProtoBlockNum* protoBlockNum,
                               GetTxBlockResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoBlockNum->has_blocknum()) {
      ret->set_error("blocknum not set in request");
      return Status::OK;
    }
    uint64_t blockNum = protoBlockNum->blocknum();

    // Get the tx block.
    TxBlock txblock = m_mediator.m_txBlockChain.GetBlock(blockNum);

    // Convert txblock to proto.
    ProtoTxBlock protoTxBlock;
    TxBlockToProtobuf(txblock, protoTxBlock);
    ret->set_allocated_txblock(&protoTxBlock);
  } catch (const char* msg) {
    ret->set_error(msg);
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "Error " << e.what());
    ret->set_error("String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Out of range");
  } catch (exception& e) {
    LOG_GENERAL(
        INFO, "[Error]" << e.what() << " Input: " << protoBlockNum->blocknum());
    ret->set_error("Unable to Process");
  }

  return Status::OK;
}

Status ProtoServer::GetLatestDsBlock(ServerContext* context,
                                     const Empty* request,
                                     GetDSBlockResponse* ret) {
  LOG_MARKER();

  // Retrieve the latest DS block.
  DSBlock dsblock = m_mediator.m_dsBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << dsblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        "
                        << dsblock.GetHeader().GetTimestamp().str());

  // Convert DSBlock to proto.
  ProtoDSBlock protoDSBlock;
  DSBlockToProtobuf(dsblock, protoDSBlock);
  ret->set_allocated_dsblock(&protoDSBlock);

  return Status::OK;
}

Status ProtoServer::GetLatestTxBlock(ServerContext* context,
                                     const Empty* request,
                                     GetTxBlockResponse* ret) {
  LOG_MARKER();

  // Get the latest tx block.
  TxBlock txblock = m_mediator.m_txBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << txblock.GetHeader().GetBlockNum()
                        << "  Timestamp:        "
                        << txblock.GetHeader().GetTimestamp().str());

  // Convert txblock to proto.
  ProtoTxBlock protoTxBlock;
  TxBlockToProtobuf(txblock, protoTxBlock);
  ret->set_allocated_txblock(&protoTxBlock);

  return Status::OK;
}

Status ProtoServer::GetBalance(ServerContext* context,
                               const ProtoAddress* protoAddress,
                               GetBalanceResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoAddress->has_address()) {
      ret->set_error("Address not set in request");
      return Status::OK;
    }

    if (protoAddress->address().size() != ACC_ADDR_SIZE * 2) {
      ret->set_error("Address size not appropriate");
      return Status::OK;
    }

    vector<unsigned char> tmpaddr =
        DataConversion::HexStrToUint8Vec(protoAddress->address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account != nullptr) {
      boost::multiprecision::uint256_t balance = account->GetBalance();
      ret->set_balance(balance.str());

      boost::multiprecision::uint256_t nonce = account->GetNonce();
      ret->set_nonce(nonce.str());

      LOG_GENERAL(INFO, "balance " << balance.str() << " nonce: "
                                   << nonce.convert_to<unsigned int>());
    } else if (account == nullptr) {
      ret->set_balance("0");
      ret->set_nonce("0");
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress->address());
    ret->set_error("Unable To Process");
  }

  return Status::OK;
}

Status ProtoServer::GetSmartContractState(ServerContext* context,
                                          const ProtoAddress* protoAddress,
                                          GetSmartContractStateResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoAddress->has_address()) {
      ret->set_error("Address not set in request");
      return Status::OK;
    }

    if (protoAddress->address().size() != ACC_ADDR_SIZE * 2) {
      ret->set_error("Address size inappropriate");
      return Status::OK;
    }

    vector<unsigned char> tmpaddr =
        DataConversion::HexStrToUint8Vec(protoAddress->address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret->set_error("Address does not exist");
      return Status::OK;
    }

    if (!account->isContract()) {
      ret->set_error("Address is not a contract account");
      return Status::OK;
    }

    ret->set_storagejson(account->GetStorageJson().toStyledString());
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress->address());
    ret->set_error("Unable To Process");
  }

  return Status::OK;
}

Status ProtoServer::GetSmartContractInit(ServerContext* context,
                                         const ProtoAddress* protoAddress,
                                         GetSmartContractInitResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoAddress->has_address()) {
      ret->set_error("Address not set in request");
      return Status::OK;
    }

    if (protoAddress->address().size() != ACC_ADDR_SIZE * 2) {
      ret->set_error("Address size inappropriate");
      return Status::OK;
    }

    vector<unsigned char> tmpaddr =
        DataConversion::HexStrToUint8Vec(protoAddress->address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret->set_error("Address does not exist");
      return Status::OK;
    }

    if (!account->isContract()) {
      ret->set_error("Address not contract address");
      return Status::OK;
    }

    ret->set_initjson(account->GetInitJson().toStyledString());
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress->address());
    ret->set_error("Unable To Process");
  }

  return Status::OK;
}

Status ProtoServer::GetSmartContractCode(ServerContext* context,
                                         const ProtoAddress* protoAddress,
                                         GetSmartContractCodeResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoAddress->has_address()) {
      ret->set_error("Address not set in request");
      return Status::OK;
    }

    if (protoAddress->address().size() != ACC_ADDR_SIZE * 2) {
      ret->set_error("Address size inappropriate");
      return Status::OK;
    }

    vector<unsigned char> tmpaddr =
        DataConversion::HexStrToUint8Vec(protoAddress->address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret->set_error("Address does not exist");
      return Status::OK;
    }

    if (!account->isContract()) {
      ret->set_error("Address is not a contract account");
      return Status::OK;
    }

    ret->set_smartcontractcode(
        DataConversion::CharArrayToString(account->GetCode()));
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress->address());
    ret->set_error("Unable To Process");
  }

  return Status::OK;
}

Status ProtoServer::GetSmartContracts(ServerContext* context,
                                      const ProtoAddress* protoAddress,
                                      GetSmartContractResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoAddress->has_address()) {
      ret->set_error("Address not set in request");
      return Status::OK;
    }

    if (protoAddress->address().size() != ACC_ADDR_SIZE * 2) {
      ret->set_error("Address size inappropriate");
      return Status::OK;
    }

    vector<unsigned char> tmpaddr =
        DataConversion::HexStrToUint8Vec(protoAddress->address());
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      ret->set_error("Address does not exist");
      return Status::OK;
    }

    if (account->isContract()) {
      ret->set_error("A contract account queried");
      return Status::OK;
    }

    boost::multiprecision::uint256_t nonce = account->GetNonce();
    //[TODO] find out a more efficient way (using storage)

    for (boost::multiprecision::uint256_t i = 0; i < nonce; i++) {
      Address contractAddr = Account::GetAddressForContract(addr, i);
      const Account* contractAccount =
          AccountStore::GetInstance().GetAccount(contractAddr);

      if (contractAccount == nullptr || !contractAccount->isContract()) {
        continue;
      }

      auto protoContractAccount = ret->add_address();
      protoContractAccount->set_address(contractAddr.hex());
      protoContractAccount->set_state(
          contractAccount->GetStorageJson().toStyledString());
    }

  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << protoAddress->address());
    ret->set_error("Unable To Process");
  }

  return Status::OK;
}

Status ProtoServer::GetContractAddressFromTransactionID(
    ServerContext* context, const ProtoTxId* protoTxId, StringResponse* ret) {
  LOG_MARKER();

  try {
    if (!protoTxId->has_txid()) {
      ret->set_result("Tran id not set in request");
      return Status::OK;
    }

    TxBodySharedPtr tptr;
    TxnHash tranHash(protoTxId->txid());
    if (protoTxId->txid().size() != TRAN_HASH_SIZE * 2) {
      ret->set_result("Size not appropriate");
      return Status::OK;
    }

    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent) {
      ret->set_result("Txn Hash not Present");
      return Status::OK;
    }

    const Transaction& tx = tptr->GetTransaction();
    if (tx.GetData().empty() || tx.GetToAddr() == NullAddress) {
      ret->set_result("ID not a contract txn");
      return Status::OK;
    }

    ret->set_result(
        Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce() - 1)
            .hex());
  } catch (exception& e) {
    LOG_GENERAL(WARNING,
                "[Error]" << e.what() << " Input " << protoTxId->txid());
    ret->set_result("Unable to process");
  }

  return Status::OK;
}

Status ProtoServer::GetNumPeers(ServerContext* context, const Empty* request,
                                UIntResponse* ret) {
  LOG_MARKER();

  unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();
  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  ret->set_result(numPeers + m_mediator.m_DSCommittee->size());
  return Status::OK;
}

Status ProtoServer::GetNumTxBlocks(ServerContext* context, const Empty* request,
                                   StringResponse* ret) {
  LOG_MARKER();

  ret->set_result(to_string(m_mediator.m_txBlockChain.GetBlockCount()));
  return Status::OK;
}

Status ProtoServer::GetNumDSBlocks(ServerContext* context, const Empty* request,
                                   StringResponse* ret) {
  LOG_MARKER();

  ret->set_result(to_string(m_mediator.m_dsBlockChain.GetBlockCount()));
  return Status::OK;
}

Status ProtoServer::GetNumTransactions(ServerContext* context,
                                       const Empty* request,
                                       StringResponse* ret) {
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

  ret->set_result(m_BlockTxPair.second.str());
  return Status::OK;
}

Status ProtoServer::GetTransactionRate(ServerContext* context,
                                       const Empty* request,
                                       DoubleResponse* ret) {
  LOG_MARKER();

  uint64_t refBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  boost::multiprecision::uint256_t refTimeTx = 0;

  if (refBlockNum <= REF_BLOCK_DIFF) {
    if (refBlockNum <= 1) {
      LOG_GENERAL(INFO, "Not enough blocks for information");
      return Status::OK;
    } else {
      refBlockNum = 1;
      // In case there are less than REF_DIFF_BLOCKS blocks in blockchain,
      // blocknum 1 can be ref block;
    }
  } else {
    refBlockNum = refBlockNum - REF_BLOCK_DIFF;
  }

  boost::multiprecision::cpp_dec_float_50 numTxns(
      GetNumTransactions(refBlockNum));
  LOG_GENERAL(INFO, "Num Txns: " << numTxns);

  try {
    TxBlock tx = m_mediator.m_txBlockChain.GetBlock(refBlockNum);
    refTimeTx = tx.GetHeader().GetTimestamp();
  } catch (const char* msg) {
    if (string(msg) == "Blocknumber Absent") {
      LOG_GENERAL(INFO, "Error in fetching ref block");
    }

    return Status::OK;
  }

  boost::multiprecision::uint256_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetTimestamp() -
      refTimeTx;

  if (TimeDiff == 0 || refTimeTx == 0) {
    // something went wrong
    LOG_GENERAL(INFO, "TimeDiff or refTimeTx = 0 \n TimeDiff:"
                          << TimeDiff.str()
                          << " refTimeTx:" << refTimeTx.str());
    return Status::OK;
  }

  numTxns = numTxns * 1000000;  // conversion from microseconds to seconds
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numTxns / TimeDiffFloat;

  ret->set_result(ans.convert_to<double>());
  return Status::OK;
}

Status ProtoServer::GetDSBlockRate(ServerContext* context, const Empty* request,
                                   DoubleResponse* ret) {
  LOG_MARKER();

  string numDSblockStr = to_string(m_mediator.m_dsBlockChain.GetBlockCount());
  boost::multiprecision::cpp_dec_float_50 numDs(numDSblockStr);

  if (m_StartTimeDs == 0) {  // case when m_StartTime has not been set
    try {
      // Refernce time chosen to be the first block's timestamp
      DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
      m_StartTimeDs = dsb.GetHeader().GetTimestamp();
    } catch (const char* msg) {
      if (string(msg) == "Blocknumber Absent") {
        LOG_GENERAL(INFO, "No DSBlock has been mined yet");
      }

      return Status::OK;
    }
  }

  boost::multiprecision::uint256_t TimeDiff =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetTimestamp() -
      m_StartTimeDs;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return Status::OK;
  }

  // To convert from microSeconds to seconds
  numDs = numDs * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numDs / TimeDiffFloat;

  ret->set_result(ans.convert_to<double>());
  return Status::OK;
}

Status ProtoServer::GetTxBlockRate(ServerContext* context, const Empty* request,
                                   DoubleResponse* ret) {
  LOG_MARKER();

  string numTxblockStr = to_string(m_mediator.m_txBlockChain.GetBlockCount());
  boost::multiprecision::cpp_dec_float_50 numTx(numTxblockStr);

  if (m_StartTimeTx == 0) {
    try {
      // Reference Time chosen to be first block's timestamp
      TxBlock txb = m_mediator.m_txBlockChain.GetBlock(1);
      m_StartTimeTx = txb.GetHeader().GetTimestamp();
    } catch (const char* msg) {
      if (string(msg) == "Blocknumber Absent") {
        LOG_GENERAL(INFO, "No TxBlock has been mined yet");
      }

      return Status::OK;
    }
  }

  boost::multiprecision::uint256_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetTimestamp() -
      m_StartTimeTx;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return Status::OK;
  }

  // To convert from microSeconds to seconds
  numTx = numTx * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat(TimeDiff.str());
  boost::multiprecision::cpp_dec_float_50 ans = numTx / TimeDiffFloat;

  ret->set_result(ans.convert_to<double>());
  return Status::OK;
}

Status ProtoServer::GetCurrentMiniEpoch(ServerContext* context,
                                        const Empty* request,
                                        UInt64Response* ret) {
  LOG_MARKER();

  ret->set_result(m_mediator.m_currentEpochNum);
  return Status::OK;
}

Status ProtoServer::GetCurrentDSEpoch(ServerContext* context,
                                      const Empty* request,
                                      UInt64Response* ret) {
  LOG_MARKER();

  ret->set_result(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum());
  return Status::OK;
}

Status ProtoServer::DSBlockListing(ServerContext* context,
                                   const ProtoPage* protoPage,
                                   ProtoBlockListing* ret) {
  LOG_MARKER();

  if (!protoPage->has_page()) {
    ret->set_error("Page not in request");
    return Status::OK;
  }

  uint64_t currBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;
  ret->set_maxpages(int(maxPages));

  if (m_DSBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      DSBlockHeader dshead = m_mediator.m_dsBlockChain.GetBlock(0).GetHeader();
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      vector<unsigned char> vec;
      dshead.Serialize(vec, 0);
      sha2.Update(vec);
      const vector<unsigned char>& resVec = sha2.Finalize();
      m_DSBlockCache.second.insert_new(
          m_DSBlockCache.second.size(),
          DataConversion::Uint8VecToHexStr(resVec));
    } catch (const char* msg) {
      ret->set_error(msg);
      return Status::OK;
    }
  }

  unsigned int page = protoPage->page();
  if (page > maxPages || page < 1) {
    ret->set_error("Pages out of limit");
    return Status::OK;
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
    vector<unsigned char> vec;
    dshead.Serialize(vec, 0);
    sha2.Update(vec);
    const vector<unsigned char>& resVec = sha2.Finalize();

    m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(),
                                     DataConversion::Uint8VecToHexStr(resVec));
    m_DSBlockCache.first = currBlockNum;
  }

  unsigned int offset = PAGE_SIZE * (page - 1);
  if (page <= NUM_PAGES_CACHE) {  // can use cache
    boost::multiprecision::uint256_t cacheSize(
        m_DSBlockCache.second.capacity());
    if (cacheSize > m_DSBlockCache.second.size()) {
      cacheSize = m_DSBlockCache.second.size();
    }

    uint64_t size = m_DSBlockCache.second.size();

    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      auto blockData = ret->add_data();
      blockData->set_hash(m_DSBlockCache.second[size - i - 1]);
      blockData->set_blocknum(int(currBlockNum - i));
    }

  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      auto blockData = ret->add_data();
      blockData->set_hash(
          m_mediator.m_dsBlockChain.GetBlock(currBlockNum - i + 1)
              .GetHeader()
              .GetPrevHash()
              .hex());
      blockData->set_blocknum(int(currBlockNum - i));
    }
  }

  return Status::OK;
}

Status ProtoServer::TxBlockListing(ServerContext* context,
                                   const ProtoPage* protoPage,
                                   ProtoBlockListing* ret) {
  LOG_MARKER();

  if (!protoPage->has_page()) {
    ret->set_error("Page not in request");
    return Status::OK;
  }

  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;
  ret->set_maxpages(int(maxPages));

  if (m_TxBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      TxBlockHeader txhead = m_mediator.m_txBlockChain.GetBlock(0).GetHeader();
      SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
      vector<unsigned char> vec;
      txhead.Serialize(vec, 0);
      sha2.Update(vec);
      const vector<unsigned char>& resVec = sha2.Finalize();
      m_TxBlockCache.second.insert_new(
          m_TxBlockCache.second.size(),
          DataConversion::Uint8VecToHexStr(resVec));
    } catch (const char* msg) {
      ret->set_error(msg);
      return Status::OK;
    }
  }

  unsigned int page = protoPage->page();
  if (page > maxPages || page < 1) {
    ret->set_error("Pages out of limit");
    return Status::OK;
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
    vector<unsigned char> vec;
    txhead.Serialize(vec, 0);
    sha2.Update(vec);
    const vector<unsigned char>& resVec = sha2.Finalize();

    m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(),
                                     DataConversion::Uint8VecToHexStr(resVec));
    m_TxBlockCache.first = currBlockNum;
  }

  unsigned int offset = PAGE_SIZE * (page - 1);
  if (page <= NUM_PAGES_CACHE) {  // can use cache
    boost::multiprecision::uint256_t cacheSize(
        m_TxBlockCache.second.capacity());

    if (cacheSize > m_TxBlockCache.second.size()) {
      cacheSize = m_TxBlockCache.second.size();
    }

    uint64_t size = m_TxBlockCache.second.size();
    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      auto blockData = ret->add_data();
      blockData->set_hash(m_TxBlockCache.second[size - i - 1]);
      blockData->set_blocknum(int(currBlockNum - i));
    }

  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      auto blockData = ret->add_data();
      blockData->set_hash(
          m_mediator.m_txBlockChain.GetBlock(currBlockNum - i + 1)
              .GetHeader()
              .GetPrevHash()
              .hex());
      blockData->set_blocknum(int(currBlockNum - i));
    }
  }

  return Status::OK;
}

Status ProtoServer::GetBlockchainInfo(ServerContext* context,
                                      const Empty* request,
                                      ProtoBlockChainInfo* ret) {
  {
    UIntResponse response;
    Status status = GetNumPeers(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numpeers(response.result());
    }
  }

  {
    StringResponse response;
    Status status = GetNumTxBlocks(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numtxblocks(response.result());
    }
  }

  {
    StringResponse response;
    Status status = GetNumDSBlocks(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numdsblocks(response.result());
    }
  }

  {
    StringResponse response;
    Status status = GetNumTransactions(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numtxns(response.result());
    }
  }

  {
    DoubleResponse response;
    Status status = GetTransactionRate(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_txrate(response.result());
    }
  }

  {
    DoubleResponse response;
    Status status = GetTxBlockRate(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_txblockrate(response.result());
    }
  }

  {
    DoubleResponse response;
    Status status = GetDSBlockRate(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_dsblockrate(response.result());
    }
  }

  {
    UInt64Response response;
    Status status = GetCurrentMiniEpoch(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_currentminiepoch(response.result());
    }
  }

  {
    UInt64Response response;
    Status status = GetCurrentDSEpoch(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_currentdsepoch(response.result());
    }
  }

  {
    StringResponse response;
    Status status = GetNumTxnsDSEpoch(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numtxnsdsepoch(response.result());
    }
  }

  {
    UIntResponse response;
    Status status = GetNumTxnsTxEpoch(context, request, &response);
    if (status.ok() && response.has_result()) {
      ret->set_numtxnstxepoch(response.result());
    }
  }

  {
    ProtoShardingStruct response;
    Status status = GetShardingStructure(context, request, &response);
    if (status.ok()) {
      ret->set_allocated_shardingstructure(&response);
    }
  }

  return Status::OK;
}

Status ProtoServer::GetRecentTransactions(ServerContext* context,
                                          const Empty* request,
                                          ProtoTxHashes* ret) {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexRecentTxns);

  uint64_t actualSize(m_RecentTransactions.capacity());
  if (actualSize > m_RecentTransactions.size()) {
    actualSize = m_RecentTransactions.size();
  }

  ret->set_number(int(actualSize));

  uint64_t size = m_RecentTransactions.size();
  for (uint64_t i = 0; i < actualSize; i++) {
    auto txhash = ret->add_txhashes();
    txhash->set_txhash(m_RecentTransactions[size - i - 1]);
  }

  return Status::OK;
}

Status ProtoServer::GetShardingStructure(ServerContext* context,
                                         const Empty* request,
                                         ProtoShardingStruct* ret) {
  LOG_MARKER();

  try {
    auto shards = m_mediator.m_lookup->GetShardPeers();

    unsigned int num_shards = shards.size();

    if (num_shards == 0) {
      ret->set_error("No shards yet");
    } else {
      for (unsigned int i = 0; i < num_shards; i++) {
        ret->set_numpeers(i, static_cast<unsigned int>(shards[i].size()));
      }
    }

  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret->set_error("Unable to process");
  }

  return Status::OK;
}

Status ProtoServer::GetNumTxnsTxEpoch(ServerContext* context,
                                      const Empty* request, UIntResponse* ret) {
  LOG_MARKER();

  try {
    ret->set_result(
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetNumTxs());
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret->set_result(0);
  }

  return Status::OK;
}

Status ProtoServer::GetNumTxnsDSEpoch(ServerContext* context,
                                      const Empty* request,
                                      StringResponse* ret) {
  LOG_MARKER();

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

    ret->set_result(m_TxBlockCountSumPair.second.str());

  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    ret->set_result("0");
  }

  return Status::OK;
}
