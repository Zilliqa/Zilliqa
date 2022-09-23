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
#include "libUtils/DataConversion.h"
#include "libUtils/GasConv.h"
#include "libUtils/Logger.h"

using namespace jsonrpc;
using namespace std;

const char* ZEROES_HASH =
    "0x0000000000000000000000000000000000000000000000000000000000000";

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

  // todo: remove when all tests are updated to use eth_call
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("GetEthCall", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &LookupServer::GetEthCallZilI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_call", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthCallEthI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("web3_clientVersion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetWeb3ClientVersionI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("web3_sha3", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetWeb3Sha3I);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_mining", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthMiningI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getUncleByBlockHashAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, nullptr),
      &LookupServer::GetEthUncleBlockI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getUncleByBlockNumberAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, nullptr),
      &LookupServer::GetEthUncleBlockI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getUncleCountByBlockHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, nullptr),
      &LookupServer::GetEthUncleCountI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getUncleCountByBlockNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, nullptr),
      &LookupServer::GetEthUncleCountI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_coinbase", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthCoinbaseI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("net_listening", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNetListeningI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("net_peerCount", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNetPeerCountI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_protocolVersion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetProtocolVersionI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_chainId", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthChainIdI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_syncing", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthSyncingI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_accounts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthAccountsI);

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

    AbstractServer<IsolatedServer>::bindAndAddMethod(
        jsonrpc::Procedure("TogglePause", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_BOOLEAN, "param01",
                           jsonrpc::JSON_STRING, NULL),
        &IsolatedServer::TogglePauseI);

    AbstractServer<IsolatedServer>::bindAndAddMethod(
        jsonrpc::Procedure("CheckPause", jsonrpc::PARAMS_BY_POSITION,
                           jsonrpc::JSON_BOOLEAN, "param01",
                           jsonrpc::JSON_STRING, NULL),
        &IsolatedServer::CheckPauseI);

    StartBlocknumIncrement();
  }

  // Add the JSON-RPC eth style methods
  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_blockNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &IsolatedServer::GetEthBlockNumberI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("net_version", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNetVersionI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthBalanceI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getBlockByNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_BOOLEAN, NULL),
      &LookupServer::GetEthBlockByNumberI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getBlockByHash", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_BOOLEAN, NULL),
      &LookupServer::GetEthBlockByHashI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_gasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthGasPriceI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_estimateGas", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &LookupServer::GetEthEstimateGasI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getTransactionCount", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthTransactionCountI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_sendRawTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &IsolatedServer::GetEthSendRawTransactionI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getTransactionByHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthTransactionByHashI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getTransactionReceipt",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthTransactionReceiptI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_feeHistory", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthFeeHistoryI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getStorageAt", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, "param03",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthStorageAtI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthCodeI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getBlockTransactionCountByHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthBlockTransactionCountByHashI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getBlockTransactionCountByNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthBlockTransactionCountByNumberI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getTransactionByBlockHashAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthTransactionByBlockHashAndIndexI);

  AbstractServer<IsolatedServer>::bindAndAddMethod(
      jsonrpc::Procedure("eth_getTransactionByBlockNumberAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetEthTransactionByBlockNumberAndIndexI);
}

bool IsolatedServer::ValidateTxn(const Transaction& tx, const Address& fromAddr,
                                 const Account* sender,
                                 const uint128_t& gasPrice) {
  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    throw JsonRpcException(
        ServerBase::RPC_VERIFY_REJECTED,
        std::string("CHAIN_ID incorrect: ") +
            std::to_string(DataConversion::UnpackA(tx.GetVersion())) +
            " when expected " + std::to_string(CHAIN_ID));
  }

  if (tx.GetCode().size() > MAX_CODE_SIZE_IN_BYTES) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "Code size is too large");
  }

  if (tx.GetGasPriceQa() < gasPrice) {
    throw JsonRpcException(
        ServerBase::RPC_VERIFY_REJECTED,
        "GasPrice " + tx.GetGasPriceQa().convert_to<string>() +
            " lower than minimum allowable " + gasPrice.convert_to<string>());
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
      (tx.GetGasLimitZil() <
       max(CONTRACT_INVOKE_GAS, (unsigned int)(tx.GetData().size())))) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Gas limit (" + to_string(tx.GetGasLimitZil()) +
                               ") lower than minimum for invoking contract (" +
                               to_string(CONTRACT_INVOKE_GAS) + ")");
  }

  else if (type == Transaction::ContractType::CONTRACT_CREATION &&
           (tx.GetGasLimitZil() <
            max(CONTRACT_CREATE_GAS,
                (unsigned int)(tx.GetCode().size() + tx.GetData().size())))) {
    throw JsonRpcException(
        ServerBase::RPC_INVALID_PARAMETER,
        "Gas limit (" + to_string(tx.GetGasLimitZil()) +
            ") lower than minimum for creating contract (" +
            to_string(max(
                CONTRACT_CREATE_GAS,
                (unsigned int)(tx.GetCode().size() + tx.GetData().size()))) +
            ")");
  }

  if (sender->GetNonce() >= tx.GetNonce()) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Nonce (" + to_string(tx.GetNonce()) +
                               ") lower than current (" +
                               to_string(sender->GetNonce()) + ")");
  }

  return true;
}

bool IsolatedServer::RetrieveHistory(const bool& nonisoload) {
  m_mediator.m_txBlockChain.Reset();

  std::shared_ptr<Retriever> m_retriever =
      std::make_shared<Retriever>(m_mediator);

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

  if (nonisoload) {  // construct from statedelta for only non isolated server's
                     // persistence.
    uint64_t lastBlockNum = txblock->GetHeader().GetBlockNum();
    unsigned int extra_txblocks = (lastBlockNum + 1) % NUM_FINAL_BLOCK_PER_POW;
    vector<bytes> stateDeltas;

    for (uint64_t blockNum = lastBlockNum + 1 - extra_txblocks;
         blockNum <= lastBlockNum; blockNum++) {
      bytes stateDelta;
      if (!BlockStorage::GetBlockStorage().GetStateDelta(blockNum,
                                                         stateDelta)) {
        LOG_GENERAL(INFO,
                    "Didn't find the state-delta for txBlkNum: " << blockNum);
      }
      stateDeltas.emplace_back(stateDelta);
    }

    m_retriever->ConstructFromStateDeltas(lastBlockNum, extra_txblocks,
                                          stateDeltas, false);
  }

  m_currEpochGas = 0;

  return true;
}

Json::Value IsolatedServer::CreateTransaction(const Json::Value& _json) {
  Json::Value ret;

  try {
    if (!JSONConversion::checkJsonTx(_json)) {
      throw JsonRpcException(RPC_PARSE_ERROR, "Invalid Transaction JSON");
    }

    if (m_pause) {
      throw JsonRpcException(RPC_INTERNAL_ERROR, "IsoServer is paused");
    }

    Transaction tx = JSONConversion::convertJsontoTx(_json);

    uint64_t senderNonce;
    uint128_t senderBalance;

    const Address fromAddr = tx.GetSenderAddr();

    lock_guard<mutex> g(m_blockMutex);

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

    if (senderBalance < tx.GetAmountQa()) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Insufficient Balance: " + senderBalance.str());
    }

    if (m_gasPrice > tx.GetGasPriceQa()) {
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
        ret["ContractAddress"] = Account::GetAddressForContract(
                                     fromAddr, senderNonce, TRANSACTION_VERSION)
                                     .hex();
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
                               "The code is empty and To addr is null");
        break;
      default:
        throw JsonRpcException(RPC_MISC_ERROR, "Txn type unexpected");
    }

    TransactionReceipt txreceipt;

    TxnStatus error_code;
    bool throwError = false;
    txreceipt.SetEpochNum(m_blocknum);
    if (!AccountStore::GetInstance().UpdateAccountsTemp(m_blocknum,
                                                        3  // Arbitrary values
                                                        ,
                                                        true, tx, txreceipt,
                                                        error_code)) {
      throwError = true;
    }
    LOG_GENERAL(INFO, "Processing On the isolated server");
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

    if (!BlockStorage::GetBlockStorage().PutTxBody(m_blocknum, tx.GetTranID(),
                                                   twr_ser)) {
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
    LOG_GENERAL(INFO, "Processing On the isolated server completed");
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << _json.toStyledString());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }

  // This will make sure the block height advances, the
  // TX can be found in a block etc.
  if (m_timeDelta == 0) {
    PostTxBlock();
  }

  return ret;
}

std::string IsolatedServer::CreateTransactionEth(Eth::EthFields const& fields,
                                                 bytes const& pubKey) {
  // Always return the TX hash or the null hash
  std::string ret = ZEROES_HASH;

  try {
    if (m_pause) {
      throw JsonRpcException(RPC_INTERNAL_ERROR, "IsoServer is paused");
    }

    const Address toAddr{fields.toAddr};
    bytes data;
    bytes code;
    if (IsNullAddress(toAddr)) {
      code = ToEVM(fields.code);
    } else {
      data = DataConversion::StringToCharArray(
          DataConversion::Uint8VecToHexStrRet(fields.code));
    }
    Transaction tx{fields.version,
                   fields.nonce,
                   toAddr,
                   PubKey(pubKey, 0),
                   fields.amount,
                   fields.gasPrice,
                   fields.gasLimit,
                   code,  // either empty or stripped EVM-less code
                   data,  // either empty or un-hexed byte-stream
                   Signature(fields.signature, 0)};

    ret = DataConversion::AddOXPrefix(tx.GetTranID().hex());

    uint256_t senderBalance;

    const uint128_t gasPriceWei =
        (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice() *
         EVM_ZIL_SCALING_FACTOR) /
        GasConv::GetScalingFactor();

    const Address fromAddr = tx.GetSenderAddr();

    lock_guard<mutex> g(m_blockMutex);

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account* sender = AccountStore::GetInstance().GetAccount(fromAddr);

      if (!Eth::ValidateEthTxn(tx, fromAddr, sender, gasPriceWei)) {
        return ret;
      }

      senderBalance = uint256_t{sender->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
    }

    switch (Transaction::GetTransactionType(tx)) {
      case Transaction::ContractType::NON_CONTRACT:
        break;
      case Transaction::ContractType::CONTRACT_CREATION: {
        if (!ENABLE_SC) {
          throw JsonRpcException(RPC_MISC_ERROR, "Smart contract is disabled");
        }
      } break;
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
          } else if (!account->isContract()) {
            throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                                   "Non - contract address called");
          }
        }
      } break;

      case Transaction::ContractType::ERROR: {
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "The code is empty and To addr is null");
      } break;

      default: {
        throw JsonRpcException(RPC_MISC_ERROR, "Txn type unexpected");
      }
    }  // end of switch

    TransactionReceipt txreceipt;

    TxnStatus error_code;
    bool throwError = false;
    txreceipt.SetEpochNum(m_blocknum);

    if (!AccountStore::GetInstance().UpdateAccountsTemp(m_blocknum,
                                                        3,  // Arbitrary values
                                                        true, tx, txreceipt,
                                                        error_code)) {
      LOG_GENERAL(WARNING, "failed to update accounts!!!");
      throwError = true;
    }
    LOG_GENERAL(INFO, "Processing On the isolated server...");

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

    if (!BlockStorage::GetBlockStorage().PutTxBody(m_blocknum, tx.GetTranID(),
                                                   twr_ser)) {
      LOG_GENERAL(WARNING, "Unable to put tx body");
    }

    const auto& txHash = tx.GetTranID();
    LookupServer::AddToRecentTransactions(txHash);
    {
      lock_guard<mutex> g(m_txnBlockNumMapMutex);
      m_txnBlockNumMap[m_blocknum].emplace_back(txHash);
    }

    LOG_GENERAL(INFO, "Added Txn " << txHash << " to blocknum: " << m_blocknum);
    WebsocketServer::GetInstance().ParseTxn(twr);

    LOG_GENERAL(
        INFO,
        "Processing On the isolated server completed. Minting a block...");
  } catch (const JsonRpcException& je) {
    LOG_GENERAL(INFO, "[Error]" << je.what() << " Input JSON: NA");
  } catch (const exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input code: NA");
  }

  // Double create a block to make sure TXs are 'flushed'
  // This will make sure the block height advances, the
  // TX can be found in a block etc.
  if (m_timeDelta == 0) {
    PostTxBlock();
    PostTxBlock();
  }
  return ret;
}

Json::Value IsolatedServer::GetTransactionsForTxBlock(
    const string& txBlockNum) {
  uint64_t txNum;
  try {
    txNum = strtoull(txBlockNum.c_str(), NULL, 0);
  } catch (const exception& e) {
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

Json::Value IsolatedServer::GetEthBlockNumber() {
  Json::Value ret;

  try {
    const auto txBlock = m_mediator.m_txBlockChain.GetLastBlock();

    auto blockHeight = txBlock.GetHeader().GetBlockNum();
    blockHeight =
        blockHeight == std::numeric_limits<uint64_t>::max() ? 1 : blockHeight;

    std::ostringstream returnVal;
    returnVal << "0x" << std::hex << blockHeight << std::dec;
    ret = returnVal.str();
  } catch (const std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " When getting block number!");
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }

  return ret;
}

string IsolatedServer::SetMinimumGasPrice(const string& gasPrice) {
  uint128_t newGasPrice;
  if (m_timeDelta > 0) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Manual trigger disallowed");
  }
  try {
    newGasPrice = uint128_t(gasPrice);
  } catch (const exception& e) {
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
      if (m_pause) {
        continue;
      }
      PostTxBlock();
    }
  };

  DetachedFunction(1, incrThread);
  return true;
}

bool IsolatedServer::TogglePause(const string& uuid) {
  if (uuid != m_uuid) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid UUID");
  }
  m_pause = !m_pause;
  return m_pause;
}

bool IsolatedServer::CheckPause(const string& uuid) {
  if (uuid != m_uuid) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY, "Invalid UUID");
  }
  return m_pause;
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

  // In order that the m_txRootHash is not empty if there are actually TXs
  // in the microblock, set the root hash to a TXn hash if there is one
  MicroBlockHashSet hashSet{};
  if (txnhashes.size() > 0) {
    hashSet.m_txRootHash = txnhashes[0];
  }

  MicroBlockHeader mbh(0, 0, m_currEpochGas, 0, m_blocknum, hashSet, numtxns,
                       m_key.first, 0);
  MicroBlock mb(mbh, txnhashes, CoSignatures());
  MicroBlockInfo mbInfo{mb.GetBlockHash(), mb.GetHeader().GetTxRootHash(),
                        mb.GetHeader().GetShardId()};
  LOG_GENERAL(INFO, "MicroBlock hash = " << mbInfo.m_microBlockHash);
  bytes body;

  mb.Serialize(body, 0);

  if (!BlockStorage::GetBlockStorage().PutMicroBlock(
          mb.GetBlockHash(), mb.GetHeader().GetEpochNum(),
          mb.GetHeader().GetShardId(), body)) {
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
    } catch (const exception& e) {
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
  if (!BlockStorage::GetBlockStorage().PutTxBlock(txBlock.GetHeader(),
                                                  serializedTxBlock)) {
    LOG_GENERAL(WARNING, "BlockStorage::PutTxBlock failed " << txBlock);
  }
  AccountStore::GetInstance().MoveUpdatesToDisk();
  AccountStore::GetInstance().InitTemp();

  m_blocknum++;
  m_currEpochGas = 0;
}
