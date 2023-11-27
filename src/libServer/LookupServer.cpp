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
#include "LookupServer.h"
#include <Schnorr.h>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "EthRpcMethods.h"
#include "JSONConversion.h"
#include "common/Messages.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountStore/AccountStore.h"
#include "libMessage/Messenger.h"
#include "libMetrics/Api.h"
#include "libNetwork/Guard.h"
#include "libNode/Node.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libRemoteStorageDB/RemoteStorageDB.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnExtras.h"

using namespace jsonrpc;
using namespace std;

CircularArray<std::string> LookupServer::m_RecentTransactions;
std::mutex LookupServer::m_mutexRecentTxns;

namespace zil {
namespace paging {
const unsigned int PAGE_SIZE = 10;
const unsigned int NUM_PAGES_CACHE = 2;
const unsigned int TXN_PAGE_SIZE = 100;
}  // namespace paging
}  // namespace zil

namespace {

Z_I64METRIC& GetCallsCounter() {
  static Z_I64METRIC count{Z_FL::LOOKUP_SERVER, "lookup.invocation.count",
                           "Calls to Lookup Server", "Calls"};
  return count;
}

Address ToBase16AddrHelper(const std::string& addr) {
  using RpcEC = ServerBase::RPCErrorCode;

  Address convertedAddr;
  auto retCode = ToBase16Addr(addr, convertedAddr);

  if (retCode == AddressConversionCode::INVALID_ADDR) {
    throw JsonRpcException(RpcEC::RPC_INVALID_ADDRESS_OR_KEY,
                           "invalid address");
  } else if (retCode == AddressConversionCode::INVALID_BECH32_ADDR) {
    throw JsonRpcException(RpcEC::RPC_INVALID_ADDRESS_OR_KEY,
                           "Bech32 address is invalid");
  } else if (retCode == AddressConversionCode::WRONG_ADDR_SIZE) {
    throw JsonRpcException(RpcEC::RPC_INVALID_PARAMETER,
                           "Address size not appropriate");
  }

  return convertedAddr;
}

}  // namespace

//[warning] do not make this constant too big as it loops over blockchain
const unsigned int REF_BLOCK_DIFF = 1;

LookupServer::LookupServer(Mediator& mediator,
                           jsonrpc::AbstractServerConnector& server)
    : Server(mediator),
      EthRpcMethods(mediator),
      jsonrpc::AbstractServer<LookupServer>(server,
                                            jsonrpc::JSONRPC_SERVER_V2) {
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetCurrentMiniEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetCurrentMiniEpochI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("GetCurrentDSEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetCurrentDSEpochI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNodeType", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetNodeTypeI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNetworkId", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNetworkIdI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("CreateTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &LookupServer::CreateTransactionI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTransactionI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSoftConfirmedTransaction",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetSoftConfirmedTransactionI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetDsBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetDsBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetDsBlockVerbose", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetDsBlockVerboseI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTxBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTxBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTxBlockVerbose", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTxBlockVerboseI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetLatestDsBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetLatestDsBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetLatestTxBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetLatestTxBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetBalanceI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetMinimumGasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetMinimumGasPriceI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetPrevDSDifficulty", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &Server::GetPrevDSDifficultyI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetPrevDifficulty", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &Server::GetPrevDifficultyI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContracts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_ARRAY, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetContractAddressFromTransactionID",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetContractAddressFromTransactionIDI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumPeers", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &LookupServer::GetNumPeersI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumTxBlocks", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNumTxBlocksI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumDSBlocks", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNumDSBlocksI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumTransactions", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNumTransactionsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTransactionRate", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_REAL, NULL),
      &LookupServer::GetTransactionRateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTxBlockRate", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_REAL, NULL),
      &LookupServer::GetTxBlockRateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetDSBlockRate", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_REAL, NULL),
      &LookupServer::GetDSBlockRateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetShardMembers", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &LookupServer::GetShardMembersI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetCurrentDSComm", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetCurrentDSCommI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("DSBlockListing", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &LookupServer::DSBlockListingI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("TxBlockListing", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &LookupServer::TxBlockListingI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetBlockchainInfo", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetBlockchainInfoI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetRecentTransactions", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetRecentTransactionsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetShardingStructure", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetShardingStructureI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumTxnsTxEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNumTxnsTxEpochI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNumTxnsDSEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetNumTxnsDSEpochI);
  this->bindAndAddMethod(
      jsonrpc::Procedure(
          "GetSmartContractSubState", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING, "param02",
          jsonrpc::JSON_STRING, "param03", jsonrpc::JSON_ARRAY, NULL),
      &LookupServer::GetSmartContractSubStateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractState", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractStateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractCodeI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSmartContractInit", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetSmartContractInitI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTransactionsForTxBlock",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetTransactionsForTxBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTransactionsForTxBlockEx",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetTransactionsForTxBlockExI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTotalCoinSupply", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_REAL, NULL),
      &LookupServer::GetTotalCoinSupplyI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTotalCoinSupplyAsInt", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &LookupServer::GetTotalCoinSupplyAsIntI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetPendingTxns", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &LookupServer::GetPendingTxnsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetMinerInfo", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetMinerInfoI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTxnBodiesForTxBlock", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_ARRAY, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTxnBodiesForTxBlockI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTxnBodiesForTxBlockEx",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetTxnBodiesForTxBlockExI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetTransactionStatus", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &LookupServer::GetTransactionStatusI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetStateProof", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, "param03",
                         jsonrpc::JSON_STRING, NULL),
      &LookupServer::GetStateProofI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetVersion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &Server::GetVersionI);

  m_StartTimeTx = 0;
  m_StartTimeDs = 0;
  m_DSBlockCache.first = 0;
  m_DSBlockCache.second.resize(zil::paging::NUM_PAGES_CACHE *
                               zil::paging::PAGE_SIZE);
  m_TxBlockCache.first = 0;
  m_TxBlockCache.second.resize(zil::paging::NUM_PAGES_CACHE *
                               zil::paging::PAGE_SIZE);
  m_RecentTransactions.resize(zil::paging::TXN_PAGE_SIZE);
  m_TxBlockCountSumPair.first = 0;
  m_TxBlockCountSumPair.second = 0;
  random_device rd;
  m_eng = mt19937(rd());

  if (ENABLE_EVM) {
    // This is all that is required to Initialise the methods required for EVM
    EthRpcMethods::Init(this);
  }
}

string LookupServer::GetNetworkId() {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  return to_string(CHAIN_ID);
}

bool LookupServer::StartCollectorThread() {
  INC_CALLS(GetCallsCounter());

  if (!ARCHIVAL_LOOKUP) {
    LOG_GENERAL(
        WARNING,
        "Not expected to be called from node other than LOOKUP ARCHIVAL ");
    return false;
  }
  auto collectorThread = [this]() mutable -> void {
    this_thread::sleep_for(chrono::seconds(POW_WINDOW_IN_SECONDS));

    vector<Transaction> txnsToSend;
    LOG_GENERAL(INFO, "[ARCHLOOK]"
                          << "Start thread");
    while (true) {
      this_thread::sleep_for(chrono::seconds(SEED_TXN_COLLECTION_TIME_IN_SEC));
      txnsToSend.clear();

      if (m_mediator.m_disableTxns) {
        LOG_GENERAL(INFO, "Txns disabled - skipping forwarding to upper seed");
        continue;
      }

      if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
        LOG_GENERAL(INFO, "This new lookup (Seed) is not yet synced..");
        continue;
      }

      if (USE_REMOTE_TXN_CREATOR &&
          !m_mediator.m_lookup->GenTxnToSend(NUM_TXN_TO_SEND_PER_ACCOUNT,
                                             txnsToSend)) {
        LOG_GENERAL(WARNING, "GenTxnToSend failed");
      }

      LOG_GENERAL(INFO, "Size of generated txns to DS: " << txnsToSend.size());

      for (const auto& tx : txnsToSend) {
        m_mediator.m_lookup->AddTxnToMemPool(tx);
      }

      std::vector<Transaction> txnsInMemPool;
      {
        lock_guard<mutex> g(m_mediator.m_lookup->m_txnMemPoolMutex);
        txnsInMemPool = m_mediator.m_lookup->GetTransactionsFromMemPool();
        m_mediator.m_lookup->ClearTxnMemPool();
      }

      if (std::empty(txnsInMemPool)) {
        LOG_GENERAL(INFO, "Txn pool is empty - nothing to send");
        continue;
      }

      LOG_GENERAL(INFO,
                  "Size of txn batch sent to Lookup: " << txnsInMemPool.size());
      zbytes msg = {MessageType::LOOKUP, LookupInstructionType::FORWARDTXN};
      if (!Messenger::SetForwardTxnBlockFromSeed(msg, MessageOffset::BODY,
                                                 txnsInMemPool)) {
        LOG_GENERAL(WARNING, "Unable to serialize txn into protobuf msg");
      }
      m_sharedMediator.m_lookup->SendMessageToRandomSeedNode(msg);
    }
  };
  DetachedFunction(1, collectorThread);
  return true;
}

bool ValidateTxn(const Transaction& tx, const Address& fromAddr,
                 const Account* sender, const uint128_t& gasPrice) {
  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "CHAIN_ID incorrect");
  }

  if (!tx.VersionCorrect()) {
    throw JsonRpcException(
        ServerBase::RPC_VERIFY_REJECTED,
        "Transaction version incorrect! Expected:" +
            to_string(TRANSACTION_VERSION) +
            " Actual:" + to_string(DataConversion::UnpackB(tx.GetVersion())));
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
  if (!Transaction::Verify(tx)) {
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
  } else if (type == Transaction::ContractType::NON_CONTRACT &&
             tx.GetGasLimitZil() < NORMAL_TRAN_GAS) {
    throw JsonRpcException(
        ServerBase::RPC_INVALID_PARAMETER,
        "Gas limit (" + to_string(tx.GetGasLimitZil()) +
            ") lower than minimum for payment transaction (" +
            to_string(NORMAL_TRAN_GAS) + ")");
  }

  if (sender->GetNonce() >= tx.GetNonce()) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Nonce (" + to_string(tx.GetNonce()) +
                               ") lower than current (" +
                               to_string(sender->GetNonce()) + ")");
  }

  // Check if transaction amount is valid
  uint128_t gasDeposit = 0;
  if (!SafeMath<uint128_t>::mul(tx.GetGasLimitZil(), tx.GetGasPriceQa(),
                                gasDeposit)) {
    throw JsonRpcException(
        ServerBase::RPC_INVALID_PARAMETER,
        "tx.GetGasLimitZil() * tx.GetGasPriceQa() overflow!");
  }

  uint128_t debt = 0;
  if (!SafeMath<uint128_t>::add(gasDeposit, tx.GetAmountQa(), debt)) {
    throw JsonRpcException(
        ServerBase::RPC_INVALID_PARAMETER,
        "tx.GetGasLimitZil() * tx.GetGasPrice() + tx.GetAmountQa() overflow!");
  }

  if (sender->GetBalance() < debt) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Insufficient funds in source account!");
  }

  if ((type == Transaction::ContractType::CONTRACT_CREATION ||
       type == Transaction::ContractType::NON_CONTRACT) &&
      tx.GetGasLimitZil() > SHARD_MICROBLOCK_GAS_LIMIT) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Txn gas limit " + to_string(tx.GetGasLimitZil()) +
                               " greater than microblock gas limit" +
                               to_string(SHARD_MICROBLOCK_GAS_LIMIT));
  }

  return true;
}

Json::Value LookupServer::CreateTransaction(const Json::Value& _json,
                                            const uint128_t& gasPrice) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  if (Mediator::m_disableTxns) {
    LOG_GENERAL(INFO, "Txns disabled - rejecting new txn");
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }

  try {
    if (!JSONConversion::checkJsonTx(_json)) {
      throw JsonRpcException(RPC_PARSE_ERROR, "Invalid Transaction JSON");
    }

    Transaction tx = JSONConversion::convertJsontoTx(_json);

    if (tx.IsEth()) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Eth txs not supported for CreateTransaction api "
                             "- use eth_sendRawTransaction");
    }

    Json::Value ret;

    const Address fromAddr = tx.GetSenderAddr();

    bool toAccountExist;
    bool toAccountIsContract;

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account* sender =
          AccountStore::GetInstance().GetAccount(fromAddr, true);
      const Account* toAccount =
          AccountStore::GetInstance().GetAccount(tx.GetToAddr(), true);

      if (!ValidateTxn(tx, fromAddr, sender, gasPrice)) {
        return ret;
      }

      toAccountExist = (toAccount != nullptr);
      toAccountIsContract = toAccountExist && toAccount->isContract();
    }

    switch (Transaction::GetTransactionType(tx)) {
      case Transaction::ContractType::NON_CONTRACT:
        if (toAccountExist) {
          if (toAccountIsContract) {
            throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                                   "Contract account won't accept normal txn");
            return false;
          }
        }

        ret["Info"] = "Non-contract txn, sent to Ds";
        break;
      case Transaction::ContractType::CONTRACT_CREATION: {
        ret["Info"] = CheckContractTxn(tx, toAccountExist, toAccountIsContract);
        ret["ContractAddress"] =
            Account::GetAddressForContract(fromAddr, tx.GetNonce() - 1,
                                           tx.GetVersionIdentifier())
                .hex();
      } break;
      case Transaction::ContractType::CONTRACT_CALL: {
        ret["Info"] = CheckContractTxn(tx, toAccountExist, toAccountIsContract);
      } break;
      case Transaction::ContractType::ERROR:
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "Code is empty and To addr is null");
        break;
      default:
        throw JsonRpcException(RPC_MISC_ERROR, "Txn type unexpected");
    }

    if (!m_sharedMediator.m_lookup->AddTxnToMemPool(tx)) {
      throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                             "Unable to add transaction to mempool");
    }

    ret["TranID"] = tx.GetTranID().hex();
    return ret;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO,
                "[Error]" << e.what() << " Input: " << _json.toStyledString());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value LookupServer::GetTransaction(const string& transactionHash) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    TxBodySharedPtr tptr;
    TxnHash tranHash(transactionHash);

    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (isPresent) {
      Json::Value _json;
      return JSONConversion::convertTxtoJson(*tptr);
    } else {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Txn Hash not Present");
    }
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << transactionHash);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value LookupServer::GetSoftConfirmedTransaction(const string& txnHash) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    TxBodySharedPtr tptr;
    TxnHash tranHash(txnHash);
    if (txnHash.size() != TRAN_HASH_SIZE * 2) {
      throw JsonRpcException(RPC_INVALID_PARAMS, "Size not appropriate");
    }
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    bool isSoftConfirmed = false;
    if (!isPresent) {
      isSoftConfirmed =
          m_mediator.m_node->GetSoftConfirmedTransaction(tranHash, tptr);
    }

    if (isPresent || isSoftConfirmed) {
      Json::Value _json;
      return JSONConversion::convertTxtoJson(*tptr, isSoftConfirmed);
    } else {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Txn Hash not soft confirmed");
    }
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << txnHash);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value LookupServer::GetDsBlock(const string& blockNum, bool verbose) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    uint64_t BlockNum = stoull(blockNum);
    auto _json = JSONConversion::convertDSblocktoJson(
        m_mediator.m_dsBlockChain.GetBlock(BlockNum), verbose);
    if (verbose) {
      // also add last ds block hash
      BlockHash prevDSHash;
      if (BlockNum > 1) {
        prevDSHash =
            m_mediator.m_dsBlockChain.GetBlock(BlockNum - 1).GetBlockHash();
      }
      _json["PrevDSHash"] = prevDSHash.hex();
    }
    return _json;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetTxBlockByNum(const string& blockNum,
                                          bool verbose) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    uint64_t BlockNum = stoull(blockNum);
    return JSONConversion::convertTxBlocktoJson(
        m_mediator.m_txBlockChain.GetBlock(BlockNum), verbose);
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid argument");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

string LookupServer::GetMinimumGasPrice() {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  return m_mediator.m_dsBlockChain.GetLastBlock()
      .GetHeader()
      .GetGasPrice()
      .str();
}

Json::Value LookupServer::GetLatestDsBlock() {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  LOG_MARKER();
  DSBlock Latest = m_mediator.m_dsBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "BlockNum " << Latest.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << Latest.GetTimestamp());

  return JSONConversion::convertDSblocktoJson(Latest);
}

Json::Value LookupServer::GetLatestTxBlock() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  TxBlock Latest = m_mediator.m_txBlockChain.GetLastBlock();

  LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
            "BlockNum " << Latest.GetHeader().GetBlockNum()
                        << "  Timestamp:        " << Latest.GetTimestamp());

  return JSONConversion::convertTxBlocktoJson(Latest);
}

Json::Value LookupServer::GetBalanceAndNonce(const string& address) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};
    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account* account = AccountStore::GetInstance().GetAccount(addr, true);

    Json::Value ret;
    if (account != nullptr) {
      const uint128_t& balance = account->GetBalance();
      uint64_t nonce = account->GetNonce();

      ret["balance"] = balance.str();
      ret["nonce"] = static_cast<unsigned int>(nonce);
      LOG_GENERAL(INFO,
                  "DEBUG: Addr: " << address << " balance: " << balance.str()
                                  << " nonce: " << nonce << " " << account);
    } else if (account == nullptr) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Account is not created");
    }

    return ret;
  } catch (const JsonRpcException& je) {
    LOG_GENERAL(INFO, "[Error] getting balance" << je.GetMessage());
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetSmartContractState(const string& address,
                                                const string& vname,
                                                const Json::Value& indices) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (Mediator::m_disableGetSmartContractState) {
    LOG_GENERAL(WARNING, "API disabled");
    throw JsonRpcException(RPC_INVALID_REQUEST, "API disabled");
  }

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};

    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account* account = AccountStore::GetInstance().GetAccount(addr, true);

    if (account == nullptr) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Address does not exist");
    }

    if (!account->isContract()) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Address not contract address");
    }
    LOG_GENERAL(INFO, "Contract address: " << address);
    Json::Value root;
    const auto indices_vector =
        JSONConversion::convertJsonArrayToVector(indices);
    if (!account->FetchStateJson(root, vname, indices_vector)) {
      throw JsonRpcException(RPC_INTERNAL_ERROR, "FetchStateJson failed");
    }
    return root;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetSmartContractInit(const string& address) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};
    zbytes initData;
    zbytes code;

    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account* account =
          AccountStore::GetInstance().GetAccount(addr, true);

      if (account == nullptr) {
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "Address does not exist");
      }
      if (!account->isContract()) {
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "Address not contract address");
      }

      initData = account->GetInitData();
      code = account->GetCode();
    }

    string initDataStr;
    Json::Value initDataJson;
    // If the contract is EVM, represent the init data as a hex string.
    // Otherwise, it is JSON.
    if (EvmUtils::isEvm(code)) {
      initDataStr = DataConversion::Uint8VecToHexStrRet(initData);
      initDataJson = initDataStr;
    } else {
      initDataStr = DataConversion::CharArrayToString(initData);
      if (!JSONUtils::GetInstance().convertStrtoJson(initDataStr,
                                                     initDataJson)) {
        throw JsonRpcException(RPC_PARSE_ERROR,
                               "Unable to convert initData into Json");
      }
    }
    return initDataJson;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetSmartContractCode(const string& address) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};

    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account* account = AccountStore::GetInstance().GetAccount(addr, true);

    if (account == nullptr) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Address does not exist " + address);
    }

    if (!account->isContract()) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Address not contract address");
    }

    Json::Value _json;
    _json["code"] = DataConversion::CharArrayToString(account->GetCode());
    return _json;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetSmartContracts(const string& address) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};
    uint64_t nonce = 0;
    {
      shared_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account* account =
          AccountStore::GetInstance().GetAccount(addr, true);

      if (account == nullptr) {
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "Address does not exist");
      }
      if (account->isContract()) {
        throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                               "A contract account queried");
      }
      nonce = account->GetNonce();
    }

    //[TODO] find out a more efficient way (using storage)
    Json::Value _json;

    for (uint64_t i = 0; i < nonce; i++) {
      Address contractAddr =
          Account::GetAddressForContract(addr, i, TRANSACTION_VERSION);
      {
        shared_lock<shared_timed_mutex> lock(
            AccountStore::GetInstance().GetPrimaryMutex());

        const Account* contractAccount =
            AccountStore::GetInstance().GetAccount(contractAddr, true);

        if (contractAccount == nullptr || !contractAccount->isContract()) {
          continue;
        }
      }

      Json::Value tmpJson;
      tmpJson["address"] = contractAddr.hex();

      _json.append(tmpJson);
    }
    return _json;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

string LookupServer::GetContractAddressFromTransactionID(const string& tranID) {
  INC_CALLS(GetCallsCounter());

  std::string transactionID{tranID};
  DataConversion::NormalizeHexString(transactionID);

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    TxBodySharedPtr tptr;
    TxnHash tranHash(transactionID);
    if (transactionID.size() != TRAN_HASH_SIZE * 2) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Address size not appropriate");
    }
    bool isPresent = BlockStorage::GetBlockStorage().GetTxBody(tranHash, tptr);
    if (!isPresent) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "Txn Hash not Present");
    }
    const Transaction& tx = tptr->GetTransaction();
    if (tx.GetCode().empty() || !IsNullAddress(tx.GetToAddr())) {
      throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                             "ID is not a contract txn");
    }

    return Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce() - 1,
                                          tx.GetVersionIdentifier())
        .hex();
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what() << " Input " << tranID);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

unsigned int LookupServer::GetNumPeers() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  unsigned int numPeers = m_mediator.m_lookup->GetNodePeers().size();
  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);
  return numPeers + m_mediator.m_DSCommittee->size();
}

string LookupServer::GetNumTxBlocks() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  return to_string(m_mediator.m_txBlockChain.GetBlockCount());
}

string LookupServer::GetNumDSBlocks() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  return to_string(m_mediator.m_dsBlockChain.GetBlockCount());
}

string LookupServer::GetNumTransactions() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  lock_guard<mutex> g(m_mutexBlockTxPair);

  uint64_t currBlock =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  if (currBlock == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_IN_WARMUP, "No Tx blocks");
  }
  if (m_BlockTxPair.first < currBlock) {
    for (uint64_t i = m_BlockTxPair.first + 1; i <= currBlock; i++) {
      m_BlockTxPair.second +=
          m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
    }
  }
  m_BlockTxPair.first = currBlock;

  return m_BlockTxPair.second.str();
}

size_t LookupServer::GetNumTransactions(uint64_t blockNum) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  if (currBlockNum == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_IN_WARMUP, "No Tx blocks");
  }

  if (blockNum >= currBlockNum) {
    return 0;
  }

  size_t i, res = 0;

  for (i = blockNum + 1; i <= currBlockNum; i++) {
    res += m_mediator.m_txBlockChain.GetBlock(i).GetHeader().GetNumTxs();
  }

  return res;
}

double LookupServer::GetTransactionRate() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

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

  mp::cpp_dec_float_50 numTxns(LookupServer::GetNumTransactions(refBlockNum));
  LOG_GENERAL(INFO, "Num Txns: " << numTxns);

  try {
    TxBlock tx = m_mediator.m_txBlockChain.GetBlock(refBlockNum);
    refTimeTx = tx.GetTimestamp();
  } catch (const JsonRpcException& je) {
    throw je;
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
  mp::cpp_dec_float_50 TimeDiffFloat =
      static_cast<mp::cpp_dec_float_50>(TimeDiff);
  mp::cpp_dec_float_50 ans = numTxns / TimeDiffFloat;

  return ans.convert_to<double>();
}

double LookupServer::GetDSBlockRate() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  string numDSblockStr = to_string(m_mediator.m_dsBlockChain.GetBlockCount());
  mp::cpp_dec_float_50 numDs(numDSblockStr);

  if (m_StartTimeDs == 0)  // case when m_StartTime has not been set
  {
    try {
      // Refernce time chosen to be the first block's timestamp
      DSBlock dsb = m_mediator.m_dsBlockChain.GetBlock(1);
      m_StartTimeDs = dsb.GetTimestamp();
    } catch (const JsonRpcException& je) {
      throw je;
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
  mp::cpp_dec_float_50 TimeDiffFloat =
      static_cast<mp::cpp_dec_float_50>(TimeDiff);
  mp::cpp_dec_float_50 ans = numDs / TimeDiffFloat;
  return ans.convert_to<double>();
}

double LookupServer::GetTxBlockRate() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  string numTxblockStr = to_string(m_mediator.m_txBlockChain.GetBlockCount());
  mp::cpp_dec_float_50 numTx(numTxblockStr);

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
  mp::cpp_dec_float_50 TimeDiffFloat(to_string(TimeDiff));
  mp::cpp_dec_float_50 ans = numTx / TimeDiffFloat;
  return ans.convert_to<double>();
}

mp::cpp_dec_float_50 LookupServer::CalculateTotalSupply() {
  INC_CALLS(GetCallsCounter());

  auto totalSupply = TOTAL_COINBASE_REWARD + TOTAL_GENESIS_TOKEN;
  mp::cpp_dec_float_50 ans(totalSupply.str());

  uint128_t balance;

  {
    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    balance =
        AccountStore::GetInstance().GetAccount(NullAddress, true)->GetBalance();
  }

  mp::cpp_dec_float_50 rewards(balance.str());
  ans -= rewards;
  ans /= 1000000000000;  // Convert to ZIL
  return ans;
}

string LookupServer::GetTotalCoinSupply() {
  ostringstream streamObj;
  streamObj << std::fixed;
  streamObj << std::setprecision(12);
  streamObj << CalculateTotalSupply();
  return streamObj.str();
}

unsigned long LookupServer::GetTotalCoinSupplyAsInt() {
  return mp::round(CalculateTotalSupply()).convert_to<unsigned long>();
}

Json::Value LookupServer::DSBlockListing(unsigned int page) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  uint64_t currBlockNum =
      m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  Json::Value _json;

  uint maxPages = (currBlockNum / zil::paging::PAGE_SIZE) + 1;

  if (currBlockNum == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_IN_WARMUP, "No DS blocks");
  }

  _json["maxPages"] = maxPages;

  lock_guard<mutex> g(m_mutexDSBlockCache);

  if (m_DSBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      DSBlockHeader dshead = m_mediator.m_dsBlockChain.GetBlock(0).GetHeader();
      SHA256Calculator sha2;
      zbytes vec;
      dshead.Serialize(vec, 0);
      sha2.Update(vec);
      const zbytes& resVec = sha2.Finalize();
      string resStr;
      DataConversion::Uint8VecToHexStr(resVec, resStr);
      m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(), resStr);
    } catch (const char* msg) {
      throw JsonRpcException(RPC_MISC_ERROR, string(msg));
    }
  }

  if (page > maxPages || page < 1) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Pages out of limit");
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
    SHA256Calculator sha2;
    zbytes vec;
    dshead.Serialize(vec, 0);
    sha2.Update(vec);
    const zbytes& resVec = sha2.Finalize();
    string resStr;
    DataConversion::Uint8VecToHexStr(resVec, resStr);

    m_DSBlockCache.second.insert_new(m_DSBlockCache.second.size(), resStr);
    m_DSBlockCache.first = currBlockNum;
  }

  unsigned int offset = zil::paging::PAGE_SIZE * (page - 1);
  Json::Value tmpJson;
  if (page <= zil::paging::NUM_PAGES_CACHE)  // can use cache
  {
    uint128_t cacheSize(m_DSBlockCache.second.capacity());
    if (cacheSize > m_DSBlockCache.second.size()) {
      cacheSize = m_DSBlockCache.second.size();
    }

    uint64_t size = m_DSBlockCache.second.size();

    for (unsigned int i = offset;
         i < zil::paging::PAGE_SIZE + offset && i < cacheSize; i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_DSBlockCache.second[size - i - 1];
      tmpJson["BlockNum"] = uint(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  } else {
    for (uint64_t i = offset;
         i < zil::paging::PAGE_SIZE + offset && i <= currBlockNum; i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_mediator.m_dsBlockChain.GetBlock(currBlockNum - i + 1)
                            .GetHeader()
                            .GetPrevHash()
                            .hex();
      tmpJson["BlockNum"] = uint(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  }

  return _json;
}

Json::Value LookupServer::TxBlockListing(unsigned int page) {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  uint64_t currBlockNum =
      m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();
  Json::Value _json;

  if (currBlockNum == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_IN_WARMUP, "No Tx blocks");
  }

  uint maxPages = (currBlockNum / zil::paging::PAGE_SIZE) + 1;

  _json["maxPages"] = maxPages;

  lock_guard<mutex> g(m_mutexTxBlockCache);

  if (m_TxBlockCache.second.size() == 0) {
    try {
      // add the hash of genesis block
      TxBlockHeader txhead = m_mediator.m_txBlockChain.GetBlock(0).GetHeader();
      SHA256Calculator sha2;
      zbytes vec;
      txhead.Serialize(vec, 0);
      sha2.Update(vec);
      const zbytes& resVec = sha2.Finalize();
      string resStr;
      DataConversion::Uint8VecToHexStr(resVec, resStr);
      m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(), resStr);
    } catch (const char* msg) {
      throw JsonRpcException(RPC_MISC_ERROR, string(msg));
    }
  }

  if (page > maxPages || page < 1) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Pages out of limit");
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
    SHA256Calculator sha2;
    zbytes vec;
    txhead.Serialize(vec, 0);
    sha2.Update(vec);
    const zbytes& resVec = sha2.Finalize();
    string resStr;
    DataConversion::Uint8VecToHexStr(resVec, resStr);

    m_TxBlockCache.second.insert_new(m_TxBlockCache.second.size(), resStr);
    m_TxBlockCache.first = currBlockNum;
  }

  unsigned int offset = zil::paging::PAGE_SIZE * (page - 1);
  Json::Value tmpJson;
  if (page <= zil::paging::NUM_PAGES_CACHE)  // can use cache
  {
    uint128_t cacheSize(m_TxBlockCache.second.capacity());

    if (cacheSize > m_TxBlockCache.second.size()) {
      cacheSize = m_TxBlockCache.second.size();
    }

    uint64_t size = m_TxBlockCache.second.size();

    for (unsigned int i = offset;
         i < zil::paging::PAGE_SIZE + offset && i < cacheSize; i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_TxBlockCache.second[size - i - 1];
      tmpJson["BlockNum"] = uint(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  } else {
    for (uint64_t i = offset;
         i < zil::paging::PAGE_SIZE + offset && i <= currBlockNum; i++) {
      tmpJson.clear();
      tmpJson["Hash"] = m_mediator.m_txBlockChain.GetBlock(currBlockNum - i + 1)
                            .GetHeader()
                            .GetPrevHash()
                            .hex();
      tmpJson["BlockNum"] = uint(currBlockNum - i);
      _json["data"].append(tmpJson);
    }
  }

  return _json;
}

// TODO - Publish these as Gauges

Json::Value LookupServer::GetBlockchainInfo() {
  INC_CALLS(GetCallsCounter());

  Json::Value _json;
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  _json["NumPeers"] = LookupServer::GetNumPeers();
  _json["NumTxBlocks"] = LookupServer::GetNumTxBlocks();
  _json["NumDSBlocks"] = LookupServer::GetNumDSBlocks();
  _json["NumTransactions"] = LookupServer::GetNumTransactions();
  _json["TransactionRate"] = LookupServer::GetTransactionRate();
  _json["TxBlockRate"] = LookupServer::GetTxBlockRate();
  _json["DSBlockRate"] = LookupServer::GetDSBlockRate();
  _json["CurrentMiniEpoch"] = LookupServer::GetCurrentMiniEpoch();
  _json["CurrentDSEpoch"] = LookupServer::GetCurrentDSEpoch();
  _json["NumTxnsDSEpoch"] = LookupServer::GetNumTxnsDSEpoch();
  _json["NumTxnsTxEpoch"] = LookupServer::GetNumTxnsTxEpoch();
  _json["ShardingStructure"] = LookupServer::GetShardingStructure();

  return _json;
}

Json::Value LookupServer::GetRecentTransactions() {
  // todo - make an instance in local namespace as this is static
  /*
  if (zil::metrics::Filter::GetInstance().Enabled(
        zil::metrics::FilterClass::LOOKUP_SERVER)) {
    m_callCount->Add(1, {{"Method", "GetRecentTransactions"}});
  }
   */
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  lock_guard<mutex> g(m_mutexRecentTxns);
  Json::Value _json;
  uint64_t actualSize(m_RecentTransactions.capacity());
  if (actualSize > m_RecentTransactions.size()) {
    actualSize = m_RecentTransactions.size();
  }
  uint64_t size = m_RecentTransactions.size();
  _json["number"] = uint(actualSize);
  _json["TxnHashes"] = Json::Value(Json::arrayValue);
  for (uint64_t i = 0; i < actualSize; i++) {
    _json["TxnHashes"].append(m_RecentTransactions[size - i - 1]);
  }

  return _json;
}

void LookupServer::AddToRecentTransactions(const TxnHash& txhash) {
  lock_guard<mutex> g(m_mutexRecentTxns);
  m_RecentTransactions.insert_new(m_RecentTransactions.size(), txhash.hex());
}

Json::Value LookupServer::GetShardingStructure() {
  LOG_MARKER();

  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  try {
    Json::Value _json;

    auto shards = m_mediator.m_lookup->GetShardPeers();

    _json["NumPeers"].append(static_cast<unsigned int>(shards.size()));

    return _json;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

string LookupServer::GetNumTxnsTxEpoch() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  try {
    return to_string(
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetNumTxs());
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return "0";
  }
}

string LookupServer::GetNumTxnsDSEpoch() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  try {
    auto latestTxBlock = m_mediator.m_txBlockChain.GetLastBlock().GetHeader();
    auto latestTxBlockNum = latestTxBlock.GetBlockNum();
    auto latestDSBlockNum = latestTxBlock.GetDSBlockNum();

    lock_guard<mutex> g(m_mutexTxBlockCountSumPair);

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
  } catch (const JsonRpcException& je) {
    throw je;
  }

  catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    return "0";
  }
}

Json::Value LookupServer::GetTransactionsForTxBlock(const string& txBlockNum,
                                                    const string& pageNumber) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  uint64_t txNum;
  uint64_t pageNum = 0;
  try {
    txNum = strtoull(txBlockNum.c_str(), NULL, 0);
    pageNum = (pageNumber != "") ? strtoull(pageNumber.c_str(), NULL, 0)
                                 : std::numeric_limits<uint32_t>::max();
  } catch (exception& e) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, e.what());
  }

  auto const& txBlock = m_mediator.m_txBlockChain.GetBlock(txNum);

  return GetTransactionsForTxBlock(txBlock, pageNum);
}

Json::Value LookupServer::GetTxnBodiesForTxBlock(const string& txBlockNum,
                                                 const string& pageNumber) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  if (!ENABLE_GETTXNBODIESFORTXBLOCK) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "GetTxnBodiesForTxBlock not enabled");
  }
  uint64_t txNum;
  uint32_t pageNum = 0;
  Json::Value _json = Json::arrayValue;
  try {
    txNum = strtoull(txBlockNum.c_str(), NULL, 0);
    pageNum = (pageNumber != "") ? strtoull(pageNumber.c_str(), NULL, 0)
                                 : std::numeric_limits<uint32_t>::max();
  } catch (exception& e) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, e.what());
  }

  uint32_t numTransactions = 0;
  try {
    auto const& txBlock = m_mediator.m_txBlockChain.GetBlock(txNum);
    numTransactions = txBlock.GetHeader().GetNumTxs();

    auto const& hashes = GetTransactionsForTxBlock(txBlock, pageNum);

    if (pageNumber != "") {
      if (hashes["Transactions"].empty()) {
        throw JsonRpcException(RPC_MISC_ERROR, "TxBlock has no transactions");
      }

      for (const auto& shard_txn : hashes["Transactions"]) {
        for (const auto& txn_hash : shard_txn) {
          auto json_txn = GetTransaction(txn_hash.asString());
          _json.append(json_txn);
        }
      }
    } else {
      if (hashes.empty()) {
        throw JsonRpcException(RPC_MISC_ERROR, "TxBlock has no transactions");
      }

      for (const auto& shard_txn : hashes) {
        for (const auto& txn_hash : shard_txn) {
          auto json_txn = GetTransaction(txn_hash.asString());
          _json.append(json_txn);
        }
      }
    }
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error] " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }

  if (pageNumber == "") {
    // Backward compatibility: return array of txns if no page number was
    // specified
    return _json;
  }

  // For GetTxnBodiesForTxBlockEx: return map{Transactions:[], CurrPage:int,
  // NumPages:int}
  Json::Value _json2;
  _json2["Transactions"] = std::move(_json);
  _json2["CurrPage"] = pageNum;
  _json2["NumPages"] = (numTransactions / NUM_TXNS_PER_PAGE) +
                       ((numTransactions % NUM_TXNS_PER_PAGE) ? 1 : 0);
  return _json2;
}

Json::Value LookupServer::GetTransactionsForTxBlock(const TxBlock& txBlock,
                                                    const uint32_t pageNumber) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  // TODO
  // Workaround to identify dummy block as == comparator does not work on
  // empty object for TxBlock and TxBlockheader().
  // if (txBlock == TxBlock()) {
  if (txBlock.GetHeader().GetBlockNum() == INIT_BLOCK_NUMBER &&
      txBlock.GetHeader().GetDSBlockNum() == INIT_BLOCK_NUMBER) {
    throw JsonRpcException(RPC_INVALID_PARAMS, "Tx Block does not exist");
  }

  auto microBlockInfos = txBlock.GetMicroBlockInfos();
  Json::Value _json = Json::arrayValue;
  bool hasTransactions = false;
  const uint32_t transactionBeg =
      (pageNumber != std::numeric_limits<uint32_t>::max())
          ? (pageNumber * NUM_TXNS_PER_PAGE)
          : 0;
  const uint32_t transactionEnd =
      (pageNumber != std::numeric_limits<uint32_t>::max())
          ? transactionBeg + NUM_TXNS_PER_PAGE - 1
          : std::numeric_limits<uint32_t>::max();
  uint32_t transactionCur = 0;

  for (auto const& mbInfo : microBlockInfos) {
    MicroBlockSharedPtr mbptr;
    _json[mbInfo.m_shardId] = Json::arrayValue;

    if (mbInfo.m_txnRootHash == TxnHash()) {
      continue;
    }

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       mbptr)) {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Failed to get Microblock");
    }

    const std::vector<TxnHash>& tranHashes = mbptr->GetTranHashes();
    if (tranHashes.size() > 0) {
      // Skip this microblock's transactions since it is before transactionBeg
      if ((transactionCur + tranHashes.size() + 1) < transactionBeg) {
        transactionCur += tranHashes.size();
        continue;
      }
      // Skip this microblock's transactions since we've reached
      // transactionEnd
      if (transactionCur >= transactionEnd) {
        continue;
      }
      for (const auto& tranHash : tranHashes) {
        // Skip the first transactions until we reach transactionBeg
        if (transactionCur < transactionBeg) {
          transactionCur++;
          continue;
        }
        _json[mbInfo.m_shardId].append(tranHash.hex());
        hasTransactions = true;
        // Stop fetching remaining transactions since we've reached
        // transactionEnd
        if (transactionCur >= transactionEnd) {
          break;
        }
        transactionCur++;
      }
    }
  }

  if (!hasTransactions) {
    throw JsonRpcException(RPC_MISC_ERROR, "TxBlock has no transactions");
  }

  if (pageNumber == std::numeric_limits<uint32_t>::max()) {
    // Backward compatibility: return array of txns if no page number was
    // specified
    return _json;
  }

  // For GetTransactionsForTxBlockEx and GetTxnBodiesForTxBlockEx: return
  // map{Transactions:[], CurrPage:int, NumPages:int}
  Json::Value _json2;
  _json2["Transactions"] = std::move(_json);
  _json2["CurrPage"] = pageNumber;
  _json2["NumPages"] =
      (txBlock.GetHeader().GetNumTxs() / NUM_TXNS_PER_PAGE) +
      ((txBlock.GetHeader().GetNumTxs() % NUM_TXNS_PER_PAGE) ? 1 : 0);
  return _json2;
}

vector<uint> GenUniqueIndices(uint32_t size, uint32_t num, mt19937& eng) {
  // case when the number required is greater than total numbers being
  // shuffled
  if (size < num) {
    num = size;
  }
  if (num == 0) {
    return vector<uint>();
  }
  vector<uint> v(num);

  for (uint i = 0; i < num; i++) {
    uniform_int_distribution<> dis(
        0, size - i - 1);  // random num between 0 to i-1
    uint x = dis(eng);
    uint j = 0;
    for (j = 0; j < i; j++) {
      if (x < v.at(j)) {
        break;
      }
      x++;
    }
    for (uint k = j + 1; k <= i; k++) {
      v.at(i + j + 1 - k) = v.at(i + j - k);
    }
    v.at(j) = x;
  }
  return v;
}

Json::Value LookupServer::GetCurrentDSComm() {
  INC_CALLS(GetCallsCounter());

  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  try {
    Json::Value _json;

    _json["CurrentDSEpoch"] = LookupServer::GetCurrentDSEpoch();
    _json["CurrentTxEpoch"] = LookupServer::GetCurrentMiniEpoch();
    _json["NumOfDSGuard"] = Guard::GetInstance().GetNumOfDSGuard();

    auto dsComm = m_mediator.m_lookup->GetDSComm();
    _json["dscomm"] = Json::Value(Json::arrayValue);
    for (const auto& dsnode : dsComm) {
      _json["dscomm"].append(static_cast<string>(dsnode.first));
    }

    return _json;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

Json::Value LookupServer::GetShardMembers(unsigned int shardID) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }
  const auto shardMembers = m_mediator.m_lookup->GetShardPeers();
  if (shardID > 1) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Invalid shard ID");
  }
  Json::Value _json;
  try {
    auto random_vec =
        GenUniqueIndices(shardMembers.size(), NUM_SHARD_PEER_TO_REVEAL, m_eng);
    for (auto const& x : random_vec) {
      const auto& node = shardMembers.at(x);
      _json.append(JSONConversion::convertNode(node));
    }
    return _json;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error] " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

Json::Value LookupServer::GetPendingTxns() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  if (m_mediator.m_disableGetPendingTxns || !REMOTESTORAGE_DB_ENABLE) {
    throw JsonRpcException(RPC_DATABASE_ERROR, "API not supported");
  }

  try {
    const Json::Value result = RemoteStorageDB::GetInstance().QueryPendingTxns(
        m_mediator.m_currentEpochNum - PENDING_TXN_QUERY_NUM_EPOCHS,
        m_mediator.m_currentEpochNum);
    if (result.isMember("error")) {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Internal database error");
    }
    return result;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what());
    throw JsonRpcException(RPC_MISC_ERROR,
                           string("Unable To Process: ") + e.what());
  }
}

Json::Value LookupServer::GetMinerInfo(const std::string& blockNum) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  try {
    const DSBlock& latest = m_mediator.m_dsBlockChain.GetLastBlock();
    const uint64_t requestedDSBlockNum = stoull(blockNum);

    if (latest.GetHeader().GetBlockNum() < requestedDSBlockNum) {
      throw JsonRpcException(RPC_MISC_ERROR, "Requested data not found");
    }

    // For DS Committee

    // Retrieve the minerInfoDSComm database entry for the nearest multiple of
    // STORE_DS_COMMITTEE_INTERVAL Set the initial DS committee result to the
    // public keys in the entry
    const uint64_t initDSBlockNum =
        requestedDSBlockNum -
        (requestedDSBlockNum % STORE_DS_COMMITTEE_INTERVAL);
    MinerInfoDSComm minerInfoDSComm;
    if (!BlockStorage::GetBlockStorage().GetMinerInfoDSComm(initDSBlockNum,
                                                            minerInfoDSComm)) {
      throw JsonRpcException(
          RPC_DATABASE_ERROR,
          "Failed to get DS committee miner info for block " +
              boost::lexical_cast<std::string>(initDSBlockNum));
    }

    // From the entry after that until the requested block
    uint64_t currDSBlockNum = initDSBlockNum;
    while (currDSBlockNum < requestedDSBlockNum) {
      currDSBlockNum++;

      // Retrieve the dsBlocks database entry for the current block number
      const DSBlock& currDSBlock =
          m_mediator.m_dsBlockChain.GetBlock(currDSBlockNum);

      // Add the public keys of the PoWWinners in that entry to the DS
      // committee
      for (const auto& winner : currDSBlock.GetHeader().GetDSPoWWinners()) {
        minerInfoDSComm.m_dsNodes.emplace_front(winner.first);
      }

      // Retrieve the minerInfoDSComm database entry for the current block
      // number
      MinerInfoDSComm tmp;
      if (!BlockStorage::GetBlockStorage().GetMinerInfoDSComm(currDSBlockNum,
                                                              tmp)) {
        throw JsonRpcException(
            RPC_DATABASE_ERROR,
            "Failed to get DS committee miner info for block " +
                boost::lexical_cast<std::string>(currDSBlockNum));
      }

      // Remove the public keys of the ejected nodes in that entry from the DS
      // committee
      for (const auto& ejected : tmp.m_dsNodesEjected) {
        auto entry = find(minerInfoDSComm.m_dsNodes.begin(),
                          minerInfoDSComm.m_dsNodes.end(), ejected);
        if (entry == minerInfoDSComm.m_dsNodes.end()) {
          throw JsonRpcException(RPC_MISC_ERROR,
                                 "Failed to get DS committee miner info");
        }
        minerInfoDSComm.m_dsNodes.erase(entry);
      }
    }

    // For Shards

    // Retrieve the minerInfo database entry for the requested DS block
    MinerInfoShards minerInfoShards;
    if (!BlockStorage::GetBlockStorage().GetMinerInfoShards(requestedDSBlockNum,
                                                            minerInfoShards)) {
      throw JsonRpcException(
          RPC_DATABASE_ERROR,
          "Failed to get shards miner info for block " +
              boost::lexical_cast<std::string>(requestedDSBlockNum));
    }

    Json::Value _json;

    // Record the final DS committee public keys in the API response message
    _json["dscommittee"] = Json::Value(Json::arrayValue);
    for (const auto& dsnode : minerInfoDSComm.m_dsNodes) {
      _json["dscommittee"].append(static_cast<string>(dsnode));
    }

    // There are no shards now but keep this field for compatibility reasons
    _json["shards"] = Json::Value(Json::arrayValue);

    return _json;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (runtime_error& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "String not numeric");
  } catch (invalid_argument& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid arugment");
  } catch (out_of_range& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNum);
    throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value LookupServer::GetTransactionStatus(const string& txnhash) {
  INC_CALLS(GetCallsCounter());

  try {
    if (!REMOTESTORAGE_DB_ENABLE) {
      throw JsonRpcException(RPC_DATABASE_ERROR, "API not supported");
    }

    TxnHash tranHash(txnhash);

    const auto& result = RemoteStorageDB::GetInstance().QueryTxnHash(tranHash);

    if (result.isMember("error")) {
      throw JsonRpcException(RPC_DATABASE_ERROR, "Internal database error");
    } else if (result == Json::Value::null) {
      // No txnhash matches the one in DB
      throw JsonRpcException(RPC_DATABASE_ERROR, "Txn Hash not Present");
    }
    return result;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what());
    throw JsonRpcException(RPC_MISC_ERROR,
                           string("Unable To Process: ") + e.what());
  }
}

Json::Value LookupServer::GetStateProof(const string& address,
                                        const string& key,
                                        const string& txBlockNumOrTag) {
  INC_CALLS(GetCallsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Sent to a non-lookup");
  }

  if (!KEEP_HISTORICAL_STATE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Historical states not enabled");
  }

  dev::h256 rootHash;
  if (txBlockNumOrTag == "latest") {
    rootHash = dev::h256();
  } else {
    uint64_t requestedTxBlockNum;
    try {
      // blockNum check
      requestedTxBlockNum = stoull(txBlockNumOrTag);
    } catch (runtime_error& e) {
      LOG_GENERAL(INFO,
                  "[Error]" << e.what() << " TxBlockNum: " << txBlockNumOrTag);
      throw JsonRpcException(RPC_INVALID_PARAMS, "TxBlockNum not valid");
    } catch (invalid_argument& e) {
      LOG_GENERAL(INFO,
                  "[Error]" << e.what() << " TxBlockNum: " << txBlockNumOrTag);
      throw JsonRpcException(RPC_INVALID_PARAMS, "Invalid arugment");
    } catch (out_of_range& e) {
      LOG_GENERAL(INFO,
                  "[Error]" << e.what() << " TxBlockNum: " << txBlockNumOrTag);
      throw JsonRpcException(RPC_INVALID_PARAMS, "Out of range");
    } catch (exception& e) {
      LOG_GENERAL(INFO,
                  "[Error]" << e.what() << " TxBlockNum: " << txBlockNumOrTag);
      throw JsonRpcException(RPC_MISC_ERROR, "Unable To Process");
    }

    if (m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() <
        requestedTxBlockNum) {
      throw JsonRpcException(RPC_MISC_ERROR, "Requested txBlock not mined yet");
    }

    const uint64_t& earliestTrieDSEpoch = m_mediator.GetEarliestTrieDSEpoch(
        m_mediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum() /
        NUM_FINAL_BLOCK_PER_POW);

    if ((requestedTxBlockNum / NUM_FINAL_BLOCK_PER_POW) < earliestTrieDSEpoch) {
      throw JsonRpcException(
          RPC_MISC_ERROR,
          "Proof from requested txBlock is expired, earliest: " +
              boost::lexical_cast<std::string>(
                  (earliestTrieDSEpoch)*NUM_FINAL_BLOCK_PER_POW));
    }

    rootHash = m_mediator.m_txBlockChain.GetBlock(requestedTxBlockNum)
                   .GetHeader()
                   .GetStateRootHash();
  }

  // address check
  if (address.size() != ACC_ADDR_SIZE * 2) {
    throw JsonRpcException(RPC_INVALID_PARAMETER,
                           "Address size not appropriate");
  }

  if (key.size() != STATE_HASH_SIZE * 2) {
    throw JsonRpcException(RPC_INVALID_PARAMETER, "Key size not appropriate");
  }

  zbytes tmpaddr;
  zbytes tmpHashedKey;
  if (!DataConversion::HexStrToUint8Vec(address, tmpaddr)) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY, "invalid address");
  }
  if (!DataConversion::HexStrToUint8Vec(key, tmpHashedKey)) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY, "invalid key");
  }
  Address addr(tmpaddr);
  dev::h256 hashedKey(tmpHashedKey);

  // get account info & proof
  std::set<std::string> t_accountProof;
  Account account;
  if (!AccountStore::GetInstance().GetProof(addr, rootHash, account,
                                            t_accountProof)) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                           "Address does not exist in requested epoch");
  }

  if (!account.isContract()) {
    throw JsonRpcException(RPC_INVALID_ADDRESS_OR_KEY,
                           "Address not contract address");
  }

  // get proof
  std::set<std::string> t_stateProof;
  if (!Contract::ContractStorage::GetContractStorage()
           .FetchStateProofForContract(t_stateProof, account.GetStorageRoot(),
                                       hashedKey)) {
    throw JsonRpcException(RPC_DATABASE_ERROR, "Proof not found");
  }

  Json::Value ret;
  for (const auto& ap : t_accountProof) {
    string hexstr;
    if (!DataConversion::StringToHexStr(ap, hexstr)) {
      LOG_GENERAL(INFO, "StringToHexStr failed");
      throw JsonRpcException(RPC_INTERNAL_ERROR, "Hex encoding failed");
    }
    ret["accountProof"].append(hexstr);
  }
  for (const auto& sp : t_stateProof) {
    string hexstr;
    if (!DataConversion::StringToHexStr(sp, hexstr)) {
      LOG_GENERAL(INFO, "StringToHexStr failed");
      throw JsonRpcException(RPC_INTERNAL_ERROR, "Hex encoding failed");
    }
    ret["stateProof"].append(hexstr);
  }

  return ret;
}

std::string LookupServer::CheckContractTxn(const Transaction& tx,
                                           bool toAccountExist,
                                           bool toAccountIsContract) {
  TRACE(zil::trace::FilterClass::DEMO);

  INC_CALLS(GetCallsCounter());

  if (!ENABLE_SC) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           "Smart contract is disabled");
  }

  if (!toAccountExist) {
    throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                           "Target account does not exist");
  }

  else if (Transaction::GetTransactionType(tx) == Transaction::CONTRACT_CALL &&
           !toAccountIsContract) {
    throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                           "Non - contract address called");
  }

  if (tx.GetGasLimitZil() > DS_MICROBLOCK_GAS_LIMIT) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "txn gas limit exceeding shard maximum limit");
  }

  return "Contract Creation/Call Txn, Sent To Ds";
}
