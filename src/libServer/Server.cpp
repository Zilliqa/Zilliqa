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

#include "JSONConversion.h"

#include <jsonrpccpp/server.h>
#include <boost/multiprecision/cpp_dec_float.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
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
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPersistence/BlockStorage.h"
#include "libUtils/Logger.h"
#include "libUtils/TimeUtils.h"

using namespace jsonrpc;
using namespace std;

CircularArray<std::string> Server::m_RecentTransactions;
std::mutex Server::m_mutexRecentTxns;

const unsigned int PAGE_SIZE = 10;
const unsigned int NUM_PAGES_CACHE = 2;
const unsigned int TXN_PAGE_SIZE = 100;

//[warning] do not make this constant too big as it loops over blockchain
const unsigned int REF_BLOCK_DIFF = 1;

Server::Server(Mediator& mediator, HttpServer& httpserver)
    : AbstractZServer(httpserver), m_mediator(mediator) {
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

string Server::GetNetworkId() { return "TestNet"; }

Json::Value Server::CreateTransaction(const Json::Value& _json) {
  LOG_MARKER();

  Json::Value ret;

  try {
    if (!JSONConversion::checkJsonTx(_json)) {
      ret["Error"] = "Invalid Tx Json";
      return ret;
    }

    Transaction tx = JSONConversion::convertJsontoTx(_json);

    if (!m_mediator.m_validator->VerifyTransaction(tx)) {
      ret["Error"] = "Unable to Verify Transaction";
      return ret;
    }

    // LOG_GENERAL(INFO, "Nonce: "<<tx.GetNonce().str()<<" toAddr:
    // "<<tx.GetToAddr().hex()<<" senderPubKey:
    // "<<static_cast<string>(tx.GetSenderPubKey());<<" amount:
    // "<<tx.GetAmount().str());

    unsigned int num_shards = m_mediator.m_lookup->GetShardPeers().size();

    const PubKey& senderPubKey = tx.GetSenderPubKey();
    const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
    const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

    if (sender == nullptr) {
      ret["Error"] = "The sender of the txn is null";
      return ret;
    }
    // unsigned int curr_offset = 0;

    if (num_shards > 0) {
      unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);
      if (tx.GetData().empty() || tx.GetToAddr() == NullAddress) {
        if (tx.GetData().empty() && tx.GetCode().empty()) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret["Info"] = "Non-contract txn, sent to shard";
          ret["TranID"] = tx.GetTranID().hex();
        } else if (!tx.GetCode().empty() && tx.GetToAddr() == NullAddress) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret["Info"] = "Contract Creation txn, sent to shard";
          ret["TranID"] = tx.GetTranID().hex();
          ret["ContractAddress"] =
              Account::GetAddressForContract(fromAddr, sender->GetNonce())
                  .hex();
        } else {
          ret["Error"] = "Code is empty and To addr is null";
        }
        return ret;
      } else {
        const Account* account =
            AccountStore::GetInstance().GetAccount(tx.GetToAddr());

        if (account == nullptr) {
          ret["Error"] = "To Addr is null";
          return ret;
        }

        else if (!account->isContract()) {
          ret["Error"] = "Non - contract address called";
          return ret;
        }

        unsigned int to_shard =
            Transaction::GetShardIndex(tx.GetToAddr(), num_shards);
        if (to_shard == shard) {
          m_mediator.m_lookup->AddToTxnShardMap(tx, shard);
          ret["Info"] =
              "Contract Txn, Shards Match of the sender "
              "and reciever";
          ret["TranID"] = tx.GetTranID().hex();
          return ret;
        } else {
          m_mediator.m_lookup->AddToTxnShardMap(tx, num_shards);
          ret["Info"] = "Contract Txn, Sent To Ds";
          ret["TranID"] = tx.GetTranID().hex();
          return ret;
        }
      }
    } else {
      LOG_GENERAL(INFO, "No shards yet");
      ret["Error"] = "Could not create Transaction";
      return ret;
    }
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << _json.toStyledString());
    ret["Error"] = "Unable to Process";
    return ret;
  }
}

Json::Value Server::GetTransaction(const string& transactionHash) {
  LOG_MARKER();
  try {
    TxBodySharedPtr tptr;
    TxnHash tranHash(transactionHash);
    if (transactionHash.size() != TRAN_HASH_SIZE * 2) {
      Json::Value _json;
      _json["error"] = "Size not appropriate";

      return _json;
    }
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent) {
      Json::Value _json;
      _json["error"] = "Txn Hash not Present";
      return _json;
    }
    return JSONConversion::convertTxtoJson(*tptr);
  } catch (exception& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << transactionHash);
    _json["Error"] = "Unable to Process";
    return _json;
  }
}

Json::Value Server::GetDsBlock(const string& blockNum) {
  try {
    uint64_t BlockNum = stoull(blockNum);
    return JSONConversion::convertDSblocktoJson(
        m_mediator.m_dsBlockChain.GetBlock(BlockNum));
  } catch (const char* msg) {
    Json::Value _json;
    _json["Error"] = msg;
    return _json;
  } catch (runtime_error& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "Error " << e.what());
    _json["Error"] = "String not numeric";
    return _json;
  } catch (invalid_argument& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Invalid arugment";
    return _json;
  } catch (out_of_range& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Out of range";
    return _json;
  } catch (exception& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Unable to Process";
    return _json;
  }
}

Json::Value Server::GetTxBlock(const string& blockNum) {
  try {
    uint64_t BlockNum = stoull(blockNum);
    return JSONConversion::convertTxBlocktoJson(
        m_mediator.m_txBlockChain.GetBlock(BlockNum));
  } catch (const char* msg) {
    Json::Value _json;
    _json["Error"] = msg;
    return _json;
  } catch (runtime_error& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "Error " << e.what());
    _json["Error"] = "String not numeric";
    return _json;
  } catch (invalid_argument& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Invalid arugment";
    return _json;
  } catch (out_of_range& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Out of range";
    return _json;
  } catch (exception& e) {
    Json::Value _json;
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    _json["Error"] = "Unable to Process";
    return _json;
  }
}

string Server::GetMinimumGasPrice() {
  return m_mediator.m_dsBlockChain.GetLastBlock()
      .GetHeader()
      .GetGasPrice()
      .str();
}

Json::Value Server::GetLatestDsBlock() {
  LOG_MARKER();
  DSBlock Latest = m_mediator.m_dsBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << Latest.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << Latest.GetTimestamp());

  return JSONConversion::convertDSblocktoJson(Latest);
}

Json::Value Server::GetLatestTxBlock() {
  LOG_MARKER();
  TxBlock Latest = m_mediator.m_txBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
            "BlockNum " << Latest.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << Latest.GetTimestamp());

  return JSONConversion::convertTxBlocktoJson(Latest);
}

Json::Value Server::GetBalance(const string& address) {
  LOG_MARKER();

  try {
    if (address.size() != ACC_ADDR_SIZE * 2) {
      Json::Value _json;
      _json["Error"] = "Address size not appropriate";
      return _json;
    }
    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    Json::Value ret;
    if (account != nullptr) {
      boost::multiprecision::uint128_t balance = account->GetBalance();
      uint64_t nonce = account->GetNonce();

      ret["balance"] = balance.str();
      // FIXME: a workaround, 256-bit unsigned int being truncated
      ret["nonce"] = static_cast<unsigned int>(nonce);
      LOG_GENERAL(INFO, "balance " << balance.str() << " nonce: " << nonce);
    } else if (account == nullptr) {
      ret["balance"] = "0";
      ret["nonce"] = 0;
    }

    return ret;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    Json::Value _json;
    _json["Error"] = "Unable To Process";

    return _json;
  }
}

Json::Value Server::GetSmartContractState(const string& address) {
  LOG_MARKER();

  try {
    Json::Value _json;
    if (address.size() != ACC_ADDR_SIZE * 2) {
      _json["Error"] = "Address size inappropriate";
      return _json;
    }
    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      _json["Error"] = "Address does not exist";
      return _json;
    }

    return account->GetStorageJson();
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    Json::Value _json;
    _json["Error"] = "Unable To Process";

    return _json;
  }
}

Json::Value Server::GetSmartContractInit(const string& address) {
  LOG_MARKER();

  try {
    Json::Value _json;
    if (address.size() != ACC_ADDR_SIZE * 2) {
      _json["Error"] = "Address size inappropriate";
      return _json;
    }
    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      _json["Error"] = "Address does not exist";
      return _json;
    }
    if (!account->isContract()) {
      _json["Error"] = "Address not contract address";
      return _json;
    }

    return account->GetInitJson();
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    Json::Value _json;
    _json["Error"] = "Unable To Process";

    return _json;
  }
}

Json::Value Server::GetSmartContractCode(const string& address) {
  LOG_MARKER();

  try {
    Json::Value _json;
    if (address.size() != ACC_ADDR_SIZE * 2) {
      _json["Error"] = "Address size inappropriate";
      return _json;
    }
    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      _json["Error"] = "Address does not exist";
      return _json;
    }

    if (!account->isContract()) {
      _json["Error"] = "Address is not a contract account";
      return _json;
    }

    _json["code"] = DataConversion::CharArrayToString(account->GetCode());
    return _json;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    Json::Value _json;
    _json["Error"] = "Unable To Process";

    return _json;
  }
}

Json::Value Server::GetSmartContracts(const string& address) {
  Json::Value _json;
  LOG_MARKER();
  try {
    Json::Value _json;
    if (address.size() != ACC_ADDR_SIZE * 2) {
      _json["Error"] = "Address size inappropriate";
      return _json;
    }
    vector<unsigned char> tmpaddr = DataConversion::HexStrToUint8Vec(address);
    Address addr(tmpaddr);
    const Account* account = AccountStore::GetInstance().GetAccount(addr);

    if (account == nullptr) {
      _json["Error"] = "Address does not exist";
      return _json;
    }
    if (account->isContract()) {
      _json["Error"] = "A contract account queried";
      return _json;
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

      Json::Value tmpJson;
      tmpJson["address"] = contractAddr.hex();
      tmpJson["state"] = contractAccount->GetStorageJson();

      _json.append(tmpJson);
    }
    return _json;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    Json::Value _json;
    _json["Error"] = "Unable To Process";

    return _json;
  }
}

string Server::GetContractAddressFromTransactionID(const string& tranID) {
  try {
    TxBodySharedPtr tptr;
    TxnHash tranHash(tranID);
    if (tranID.size() != TRAN_HASH_SIZE * 2) {
      return "Size not appropriate";
    }
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent) {
      return "Txn Hash not Present";
    }
    const Transaction& tx = tptr->GetTransaction();
    if (tx.GetCode().empty() || tx.GetToAddr() != NullAddress) {
      return "ID not a contract txn";
    }

    return Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce() - 1)
        .hex();
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what() << " Input " << tranID);
    return "Unable to process";
  }
}

string Server::CreateMessage([[gnu::unused]] const Json::Value& _json) {
  return "Hello";
}

string Server::GetGasEstimate([[gnu::unused]] const Json::Value& _json) {
  return "Hello";
}

Json::Value Server::GetTransactionReceipt([
    [gnu::unused]] const string& transactionHash) {
  return "Hello";
}

unsigned int Server::GetNumPeers() {
  LOG_MARKER();
  unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();
  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
  return numPeers + m_mediator.m_DSCommittee->size();
}

string Server::GetNumTxBlocks() {
  LOG_MARKER();

  return to_string(m_mediator.m_txBlockChain.GetBlockCount());
}

string Server::GetNumDSBlocks() {
  LOG_MARKER();

  return to_string(m_mediator.m_dsBlockChain.GetBlockCount());
}

uint8_t Server::GetPrevDSDifficulty() {
  return m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDSDifficulty();
}

uint8_t Server::GetPrevDifficulty() {
  return m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetDifficulty();
}

string Server::GetNumTransactions() {
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

  return m_BlockTxPair.second.str();
}

size_t Server::GetNumTransactions(uint64_t blockNum) {
  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (blockNum >= currBlockNum) {
    return 0;
  }

  size_t i, res = 0;

  for (i = blockNum + 1; i <= currBlockNum; i++) {
    res += m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
  }

  return res;
}
double Server::GetTransactionRate() {
  LOG_MARKER();

  uint64_t refBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  uint64_t refTimeTx = 0;

  if (refBlockNum <= REF_BLOCK_DIFF) {
    if (refBlockNum <= 1) {
      LOG_GENERAL(INFO, "Not enough blocks for information");
      return 0;
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
    return 0;
  }

  uint64_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetTimestamp() - refTimeTx;

  if (TimeDiff == 0 || refTimeTx == 0) {
    // something went wrong
    LOG_GENERAL(INFO, "TimeDiff or refTimeTx = 0 \n TimeDiff:"
                          << TimeDiff << " refTimeTx:" << refTimeTx);
    return 0;
  }
  numTxns = numTxns * 1000000;  // conversion from microseconds to seconds
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numTxns / TimeDiffFloat;

  return ans.convert_to<double>();
}

double Server::GetDSBlockRate() {
  LOG_MARKER();

  string numDSblockStr = to_string(m_mediator.m_dsBlockChain.GetBlockCount());
  boost::multiprecision::cpp_dec_float_50 numDs(numDSblockStr);

  if (m_StartTimeDs == 0)  // case when m_StartTime has not been set
  {
    try {
      // Refernce time chosen to be the first block's timestamp
      DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
      m_StartTimeDs = dsb.GetTimestamp();
    } catch (const char* msg) {
      if (string(msg) == "Blocknumber Absent") {
        LOG_GENERAL(INFO, "No DSBlock has been mined yet");
      }
      return 0;
    }
  }
  uint64_t TimeDiff =
      m_mediator.m_dsBlockChain.GetLastBlock().GetTimestamp() - m_StartTimeDs;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return 0;
  }
  // To convert from microSeconds to seconds
  numDs = numDs * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat =
      static_cast<boost::multiprecision::cpp_dec_float_50>(TimeDiff);
  boost::multiprecision::cpp_dec_float_50 ans = numDs / TimeDiffFloat;
  return ans.convert_to<double>();
}

double Server::GetTxBlockRate() {
  LOG_MARKER();

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
      return 0;
    }
  }
  uint64_t TimeDiff =
      m_mediator.m_txBlockChain.GetLastBlock().GetTimestamp() - m_StartTimeTx;

  if (TimeDiff == 0) {
    LOG_GENERAL(INFO, "Wait till the second block");
    return 0;
  }
  // To convert from microSeconds to seconds
  numTx = numTx * 1000000;
  boost::multiprecision::cpp_dec_float_50 TimeDiffFloat(to_string(TimeDiff));
  boost::multiprecision::cpp_dec_float_50 ans = numTx / TimeDiffFloat;
  return ans.convert_to<double>();
}

string Server::GetCurrentMiniEpoch() {
  LOG_MARKER();

  return to_string(m_mediator.m_currentEpochNum);
}

string Server::GetCurrentDSEpoch() {
  LOG_MARKER();

  return to_string(
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum());
}

Json::Value Server::DSBlockListing(unsigned int page) {
  LOG_MARKER();

  uint64_t currBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  Json::Value _json;

  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;

  _json["maxPages"] = int(maxPages);

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
      _json["Error"] = msg;
      return _json;
    }
  }

  if (page > maxPages || page < 1) {
    _json["Error"] = "Pages out of limit";
    return _json;
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
  Json::Value tmpJson;
  if (page <= NUM_PAGES_CACHE)  // can use cache
  {
    boost::multiprecision::uint128_t cacheSize(
        m_DSBlockCache.second.capacity());
    if (cacheSize > m_DSBlockCache.second.size()) {
      cacheSize = m_DSBlockCache.second.size();
    }

    uint64_t size = m_DSBlockCache.second.size();

    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_DSBlockCache.second[size - i - 1];
      tmpJson["BlockNum"] = int(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_mediator.m_dsBlockChain.GetBlock(currBlockNum - i + 1)
                            .GetHeader()
                            .GetPrevHash()
                            .hex();
      tmpJson["BlockNum"] = int(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  }

  return _json;
}

Json::Value Server::TxBlockListing(unsigned int page) {
  LOG_MARKER();

  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  Json::Value _json;

  auto maxPages = (currBlockNum / PAGE_SIZE) + 1;

  _json["maxPages"] = int(maxPages);

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
      _json["Error"] = msg;
      return _json;
    }
  }

  if (page > maxPages || page < 1) {
    _json["Error"] = "Pages out of limit";
    return _json;
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
  Json::Value tmpJson;
  if (page <= NUM_PAGES_CACHE)  // can use cache
  {
    boost::multiprecision::uint128_t cacheSize(
        m_TxBlockCache.second.capacity());

    if (cacheSize > m_TxBlockCache.second.size()) {
      cacheSize = m_TxBlockCache.second.size();
    }

    uint64_t size = m_TxBlockCache.second.size();

    for (unsigned int i = offset; i < PAGE_SIZE + offset && i < cacheSize;
         i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_TxBlockCache.second[size - i - 1];
      tmpJson["BlockNum"] = int(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  } else {
    for (uint64_t i = offset; i < PAGE_SIZE + offset && i <= currBlockNum;
         i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_mediator.m_txBlockChain.GetBlock(currBlockNum - i + 1)
                            .GetHeader()
                            .GetPrevHash()
                            .hex();
      tmpJson["BlockNum"] = int(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  }

  return _json;
}

Json::Value Server::GetBlockchainInfo() {
  Json::Value _json;

  _json["NumPeers"] = Server::GetNumPeers();
  _json["NumTxBlocks"] = Server::GetNumTxBlocks();
  _json["NumDSBlocks"] = Server::GetNumDSBlocks();
  _json["NumTransactions"] = Server::GetNumTransactions();
  _json["TransactionRate"] = Server::GetTransactionRate();
  _json["TxBlockRate"] = Server::GetTxBlockRate();
  _json["DSBlockRate"] = Server::GetDSBlockRate();
  _json["CurrentMiniEpoch"] = Server::GetCurrentMiniEpoch();
  _json["CurrentDSEpoch"] = Server::GetCurrentDSEpoch();
  _json["NumTxnsDSEpoch"] = Server::GetNumTxnsDSEpoch();
  _json["NumTxnsTxEpoch"] = Server::GetNumTxnsTxEpoch();
  _json["ShardingStructure"] = Server::GetShardingStructure();

  return _json;
}

Json::Value Server::GetRecentTransactions() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexRecentTxns);
  Json::Value _json;
  uint64_t actualSize(m_RecentTransactions.capacity());
  if (actualSize > m_RecentTransactions.size()) {
    actualSize = m_RecentTransactions.size();
  }
  uint64_t size = m_RecentTransactions.size();
  _json["number"] = int(actualSize);
  _json["TxnHashes"] = Json::Value(Json::arrayValue);
  for (uint64_t i = 0; i < actualSize; i++) {
    _json["TxnHashes"].append(m_RecentTransactions[size - i - 1]);
  }

  return _json;
}

void Server::AddToRecentTransactions(const TxnHash& txhash) {
  lock_guard<mutex> g(m_mutexRecentTxns);
  m_RecentTransactions.insert_new(m_RecentTransactions.size(), txhash.hex());
}
Json::Value Server::GetShardingStructure() {
  LOG_MARKER();

  try {
    Json::Value _json;

    auto shards = m_mediator.m_lookup->GetShardPeers();

    unsigned int num_shards = shards.size();

    if (num_shards == 0) {
      _json["Error"] = "No shards yet";
      return _json;
    } else {
      for (unsigned int i = 0; i < num_shards; i++) {
        _json["NumPeers"].append(static_cast<unsigned int>(shards[i].size()));
      }
    }
    return _json;
  } catch (exception& e) {
    Json::Value _json;
    _json["Error"] = "Unable to process ";
    LOG_GENERAL(WARNING, e.what());
    return _json;
  }
}

uint32_t Server::GetNumTxnsTxEpoch() {
  LOG_MARKER();

  try {
    return m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetNumTxs();
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return 0;
  }
}

string Server::GetNumTxnsDSEpoch() {
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
      }
      // Case if DS Epoch Changed
      else {
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

    return m_TxBlockCountSumPair.second.str();
  }

  catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return "0";
  }
}
