/*
 * Copyright (C) 2020 Zilliqa
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

#include "IsolatedServer.h"
#include "JSONConversion.h"
#include "libPersistence/Retriever.h"
#include "libServer/WebsocketServer.h"

using namespace jsonrpc;
using namespace std;

IsolatedServer::IsolatedServer(Mediator& mediator,
                               AbstractServerConnector& server,
                               const uint64_t& blocknum,
                               const uint32_t& timeDelta)
    : LookupServer(mediator, server),
      jsonrpc::AbstractServer<IsolatedServer>(server,
                                              jsonrpc::JSONRPC_SERVER_V2),
      m_blocknum(blocknum),
      m_timeDelta(timeDelta),
      m_key(Schnorr::GenKeyPair()) {
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("CreateTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &IsolatedServer::CreateTransactionI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("IncreaseBlocknum", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &IsolatedServer::IncreaseBlocknumI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetBalanceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure(
          "GetSmartContractSubState", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING, "param02",
          jsonrpc::JSON_STRING, "param03", jsonrpc::JSON_ARRAY, NULL),
      &LookupServer::GetSmartContractSubStateI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractState", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractStateI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractCodeI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetMinimumGasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &IsolatedServer::GetMinimumGasPriceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("SetMinimumGasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &IsolatedServer::SetMinimumGasPriceI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContracts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_ARRAY, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractsI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetNetworkId", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNetworkIdI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractInit", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractInitI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTransactionI);
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetContractAddressFromTransactionID",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetContractAddressFromTransactionIDI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetBlocknum", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &IsolatedServer::GetBlocknumI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetRecentTransactions", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetRecentTransactionsI);

  if (timeDelta > 0) {
    AbstractServer<IsolatedServer>::bindAndAddMethod(
        jsonrpc::Procedure("GetTransactionsForTxBlock",
                           jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                           "param01", jsonrpc::JSON_STRING, NULL),
        &IsolatedServer::GetTransactionsForTxBlockI);
    AbstractServer<IsolatedServer>::bindAndAddMethod(
        jsonrpc::Procedure("GetTxBlock", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, "param01",
                           jsonrpc::JSON_STRING, NULL),
        &LookupServer::GetTxBlockI);

    AbstractServer<IsolatedServer>::bindAndAddMethod(
        jsonrpc::Procedure("GetLatestTxBlock", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_OBJECT, NULL),
        &LookupServer::GetLatestTxBlockI);

    StartBlocknumIncrement();
  }
}

bool IsolatedServer::ValidateTxn(const Transaction& tx, const Address& fromAddr,
                                 const Account* sender,
                                 const uint128_t& gasPrice) {
  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "CHAIN_ID incorrect");
  }

  if (tx.GetCode().size() > MAX_CODE_SIZE_IN_BYTES) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "Code size is too large");
  }

  if (tx.GetGasPrice() < gasPrice) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "GasPrice " + tx.GetGasPrice().convert_to<string>() +
                               " lower than minimum allowable " +
                               gasPrice.convert_to<string>());
  }
  if (!Validator::VerifyTransaction(tx)) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "Unable to verify transaction");
  }

  if (IsNullAddress(fromAddr)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                           "Invalid address for issuing transactions");
  }

  if (sender == nullptr) {
    throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                           "The sender of the txn has no balance");
  }
  const auto type = Transaction::GetTransactionType(tx);

  if (type == Transaction::ContractType::CONTRACT_CALL &&
      (tx.GetGasLimit() <
       max(CONTRACT_INVOKE_GAS, (unsigned int)(tx.GetData().size())))) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Gas limit (" + to_string(tx.GetGasLimit()) +
                               ") lower than minimum for invoking contract (" +
                               to_string(CONTRACT_INVOKE_GAS) + ")");
  }

  else if (type == Transaction::ContractType::CONTRACT_CREATION &&
           (tx.GetGasLimit() <
            max(CONTRACT_CREATE_GAS,
                (unsigned int)(tx.GetCode().size() + tx.GetData().size())))) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Gas limit (" + to_string(tx.GetGasLimit()) +
                               ") lower than minimum for creating contract (" +
                               to_string(CONTRACT_CREATE_GAS) + ")");
  }

  if (sender->GetNonce() >= tx.GetNonce()) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Nonce (" + to_string(tx.GetNonce()) +
                               ") lower than current (" +
                               to_string(sender->GetNonce()) + ")");
  }

  return true;
}

bool IsolatedServer::RetrieveHistory() {
  m_mediator.m_txBlockChain.Reset();

  std::shared_ptr<Retriever> m_retriever;

  bool st_result = m_retriever->RetrieveStates();

  if (!(st_result)) {
    LOG_GENERAL(WARNING, "Retrieval of states and tx block failed");
    return false;
  }
  TxBlockSharedPtr txblock;
  bool ret = BlockStorage::GetBlockStorage().GetLatestTxBlock(txblock);
  if (ret) {
    m_blocknum = txblock->GetHeader().GetBlockNum() + 1;
  } else {
    LOG_GENERAL(WARNING, "Could not retrieve latest block num");
    return false;
  }
  m_currEpochGas = 0;

  return true;
}

Json::Value IsolatedServer::CreateTransaction(const Json::Value& _json) {
  try {
    if (!JSONConversion::checkJsonTx(_json)) {
      throw JsonRpcException(RPC_PARSE_ERROR, "Invalid Transaction JSON");
    }

    lock_guard<mutex> g(m_blockMutex);

    LOG_GENERAL(INFO, "On the isolated server ");

    Transaction tx = JSONConversion::convertJsontoTx(_json);

    Json::Value ret;

    uint64_t senderNonce;
    uint128_t senderBalance;

    const Address fromAddr = tx.GetSenderAddr();

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

      if (!ValidateTxn(tx, fromAddr, sender, m_gasPrice)) {
        return ret;
      }

      senderNonce = sender->GetNonce();
      senderBalance = sender->GetBalance();
    }

    if (senderNonce + 1 != tx.GetNonce()) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Expected Nonce: " + to_string(senderNonce + 1));
    }

    if (senderBalance < tx.GetAmount()) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Insufficient Balance: " + senderBalance.str());
    }

    if (m_gasPrice > tx.GetGasPrice()) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Minimum gas price greater: " + m_gasPrice.str());
    }

    switch (Transaction::GetTransactionType(tx)) {
      case Transaction::ContractType::NON_CONTRACT:
        break;
      case Transaction::ContractType::CONTRACT_CREATION:
        if (!ENABLE_SC) {
          throw JsonRpcException(RPC_MISC_ERROR, "Smart contract is disabled");
        }
        ret["ContractAddress"] =
            Account::GetAddressForContract(fromAddr, senderNonce).hex();
        break;
      case Transaction::ContractType::CONTRACT_CALL: {
        if (!ENABLE_SC) {
          throw JsonRpcException(RPC_MISC_ERROR, "Smart contract is disabled");
        }

        {
          shared_lock<shared_timed_mutex> lock(
              AccountStore::GetInstance().GetPrimaryMutex());

          const Account* account =
              AccountStore::GetInstance().GetAccount(tx.GetToAddr());

          if (account == nullptr) {
            throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                                   "To addr is null");
          }

          else if (!account->isContract()) {
            throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                                   "Non - contract address called");
          }
        }
      } break;

      case Transaction::ContractType::ERROR:
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "Code is empty and To addr is null");
        break;
      default:
        throw JsonRpcException(RPC_MISC_ERROR, "Txn type unexpected");
    }

    TransactionReceipt txreceipt;

    ErrTxnStatus error_code;
    bool throwError = false;
    txreceipt.SetEpochNum(m_blocknum);
    if (!AccountStore::GetInstance().UpdateAccountsTemp(m_blocknum,
                                                        3  // Arbitrary values
                                                        ,
                                                        true, tx, txreceipt,
                                                        error_code)) {
      throwError = true;
    }

    AccountStore::GetInstance().ProcessStorageRootUpdateBufferTemp();
    AccountStore::GetInstance().CleanNewLibrariesCacheTemp();

    AccountStore::GetInstance().SerializeDelta();
    AccountStore::GetInstance().CommitTemp();

    if (!m_timeDelta) {
      AccountStore::GetInstance().InitTemp();
    }

    if (throwError) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Error Code: " + to_string(error_code));
    }

    TransactionWithReceipt twr(tx, txreceipt);

    bytes twr_ser;

    twr.Serialize(twr_ser, 0);

    m_currEpochGas += txreceipt.GetCumGas();

    if (!BlockStorage::GetBlockStorage().PutTxBody(tx.GetTranID(), twr_ser)) {
      LOG_GENERAL(WARNING, "Unable to put tx body");
    }
    const auto& txHash = tx.GetTranID();
    LookupServer::AddToRecentTransactions(txHash);
    {
      lock_guard<mutex> g(m_txnBlockNumMapMutex);
      m_txnBlockNumMap[m_blocknum].emplace_back(txHash);
    }
    LOG_GENERAL(INFO, "Added Txn " << txHash << " to blocknum: " << m_blocknum);
    ret["TranID"] = txHash.hex();
    ret["Info"] = "Txn processed";
    WebsocketServer::GetInstance().ParseTxn(twr);
    return ret;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << _json.toStyledString());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value IsolatedServer::GetTransactionsForTxBlock(
    const string& txBlockNum) {
  uint64_t txNum;
  try {
    txNum = strtoull(txBlockNum.c_str(), NULL, 0);
  } catch (exception& e) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, e.what());
  }

  auto const& txBlock = m_mediator.m_txBlockChain.GetBlock(txNum);

  if (txBlock.GetHeader().GetBlockNum() == INIT_BLOCK_NUMBER &&
      txBlock.GetHeader().GetDSBlockNum() == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_INVALID_PARAMS, "TxBlock does not exist");
  }

  auto microBlockInfos = txBlock.GetMicroBlockInfos();
  Json::Value _json = Json::arrayValue;
  bool hasTransactions = false;

  for (auto const& mbInfo : microBlockInfos) {
    MicroBlockSharedPtr mbptr;
    _json[mbInfo.m_shardId] = Json::arrayValue;

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       mbptr)) {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Failed to get Microblock");
    }

    const std::vector<TxnHash>& tranHashes = mbptr->GetTranHashes();
    if (tranHashes.size() > 0) {
      hasTransactions = true;
      for (const auto& tranHash : tranHashes) {
        _json[mbInfo.m_shardId].append(tranHash.hex());
      }
    }
  }

  if (!hasTransactions) {
    throw JsonRpcException(RPC_MISC_ERROR, "TxBlock has no transactions");
  }
  return _json;
}

string IsolatedServer::IncreaseBlocknum(const uint32_t& delta) {
  if (m_timeDelta > 0) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Manual trigger disallowed");
  }

  m_blocknum += delta;

  return to_string(m_blocknum);
}

string IsolatedServer::GetBlocknum() { return to_string(m_blocknum); }

string IsolatedServer::SetMinimumGasPrice(const string& gasPrice) {
  uint128_t newGasPrice;
  if (m_timeDelta > 0) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Manual trigger disallowed");
  }
  try {
    newGasPrice = move(uint128_t(gasPrice));
  } catch (exception& e) {
    throw JsonRpcException(RPC_INVALID_PARAMETER,
                           "Gas price should be numeric");
  }
  if (newGasPrice < 1) {
    throw JsonRpcException(RPC_INVALID_PARAMETER,
                           "Gas price cannot be less than 1");
  }

  m_gasPrice = move(newGasPrice);

  return m_gasPrice.str();
}

string IsolatedServer::GetMinimumGasPrice() { return m_gasPrice.str(); }

bool IsolatedServer::StartBlocknumIncrement() {
  LOG_GENERAL(INFO, "Starting automatic increment " << m_timeDelta);
  auto incrThread = [this]() mutable -> void {
    while (true) {
      this_thread::sleep_for(chrono::milliseconds(m_timeDelta));
      PostTxBlock();
    }
  };

  DetachedFunction(1, incrThread);
  return true;
}

TxBlock IsolatedServer::GenerateTxBlock() {
  uint numtxns;
  vector<TxnHash> txnhashes;
  {
    lock_guard<mutex> g(m_txnBlockNumMapMutex);
    numtxns = m_txnBlockNumMap[m_blocknum].size();
    txnhashes = m_txnBlockNumMap[m_blocknum];
    m_txnBlockNumMap[m_blocknum].clear();
  }
  TxBlockHeader txblockheader(0, m_currEpochGas, 0, m_blocknum,
                              TxBlockHashSet(), numtxns, m_key.first,
                              TXBLOCK_VERSION);
  MicroBlockHeader mbh(0, 0, m_currEpochGas, 0, m_blocknum, {}, numtxns,
                       m_key.first, 0);
  MicroBlock mb(mbh, txnhashes, CoSignatures());
  MicroBlockInfo mbInfo{mb.GetBlockHash(), mb.GetHeader().GetTxRootHash(),
                        mb.GetHeader().GetShardId()};
  LOG_GENERAL(INFO, "MicroBlock hash = " << mbInfo.m_microBlockHash);
  bytes body;

  mb.Serialize(body, 0);

  if (!BlockStorage::GetBlockStorage().PutMicroBlock(mb.GetBlockHash(), body)) {
    LOG_GENERAL(WARNING, "Failed to put microblock in body");
  }
  TxBlock txblock(txblockheader, {mbInfo}, CoSignatures());

  return txblock;
}

void IsolatedServer::PostTxBlock() {
  lock_guard<mutex> g(m_blockMutex);
  const TxBlock& txBlock = GenerateTxBlock();
  if (ENABLE_WEBSOCKET) {
    // send tx block and attach txhashes
    Json::Value j_txnhashes;
    try {
      j_txnhashes = GetTransactionsForTxBlock(to_string(m_blocknum));
    } catch (exception& e) {
      j_txnhashes = Json::arrayValue;
    }
    WebsocketServer::GetInstance().PrepareTxBlockAndTxHashes(
        JSONConversion::convertTxBlocktoJson(txBlock), j_txnhashes);

    // send event logs
    WebsocketServer::GetInstance().SendOutMessages();
  }
  m_mediator.m_txBlockChain.AddBlock(txBlock);

  bytes serializedTxBlock;
  txBlock.Serialize(serializedTxBlock, 0);
  if (!BlockStorage::GetBlockStorage().PutTxBlock(
          txBlock.GetHeader().GetBlockNum(), serializedTxBlock)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
  }

  AccountStore::GetInstance().MoveUpdatesToDisk();
  AccountStore::GetInstance().InitTemp();

  m_blocknum++;
  m_currEpochGas = 0;
}
