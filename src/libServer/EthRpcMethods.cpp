/*
 * Copyright (C) 2022 Zilliqa
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
#include "EthRpcMethods.h"
#include <jsonrpccpp/common/exception.h>
#include <boost/algorithm/hex.hpp>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <ethash/keccak.hpp>
#include <stdexcept>
#include "JSONConversion.h"
#include "LookupServer.h"
#include "common/CommonData.h"
#include "common/Constants.h"
#include "json/value.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountStore/AccountStore.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libEth/Eth.h"
#include "libEth/Filters.h"
#include "libEth/utils/EthUtils.h"
#include "libMessage/Messenger.h"
#include "libMetrics/Api.h"
#include "libNetwork/Guard.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libServer/AddressChecksum.h"
#include "libUtils/CommonUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TimeUtils.h"

// These two violate our own standards.
using namespace jsonrpc;
using namespace std;

namespace {

Z_DBLMETRIC &GetInvocationsCounter() {
  static Z_DBLMETRIC counter{Z_FL::EVM_RPC, "ethrpc.invocation.count",
                             "Calls to ethereum API", "Calls"};
  return counter;
}

bool isNumber(const std::string &str) {
  char *endp;
  strtoull(str.c_str(), &endp, 0);
  return (str.size() > 0 && endp != nullptr && *endp == '\0');
}

bool isSupportedTag(const std::string &tag) {
  return tag == "latest" || tag == "earliest" || tag == "pending" ||
         isNumber(tag);
}

Address ToBase16AddrHelper(const std::string &addr) {
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

using zil::metrics::FilterClass;

struct EthRpcMethods::ApiKeys {
  std::string from;
  std::string to;
  std::string value;
  std::string gas;
  std::string data;
};

void EthRpcMethods::Init(LookupServer *lookupServer) {
  if (lookupServer != nullptr) {
    m_lookupServer = lookupServer;
  }

  if (m_lookupServer == nullptr) {
    LOG_GENERAL(INFO, "nullptr EthRpcMethods - Init Required");
    return;
  }

  // Add Eth compatible RPC endpoints
  // todo: remove when all tests are updated to use eth_call
  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("GetEthCall", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &EthRpcMethods::GetEthCallZilI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_call", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthCallEthI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("debug_traceCall", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         "param02", jsonrpc::JSON_STRING, "param03",
                         jsonrpc::JSON_OBJECT, NULL),
      &EthRpcMethods::DebugTraceCallI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_blockNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthBlockNumberI);

  //Parameters are not listed to bypass library's parameter validation.
  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthBalanceI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockByNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthBlockByNumberI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockByHash", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_BOOLEAN, NULL),
      &EthRpcMethods::GetEthBlockByHashI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockTransactionCountByHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthBlockTransactionCountByHashI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockTransactionCountByNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::GetEthBlockTransactionCountByNumberI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionByBlockHashAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthTransactionByBlockHashAndIndexI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionByBlockNumberAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         nullptr),
      &EthRpcMethods::GetEthTransactionByBlockNumberAndIndexI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_gasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthGasPriceI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthCodeI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_estimateGas", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         "param02", OPTIONAL_JSONTYPE(jsonrpc::JSON_STRING),
                         NULL),
      &EthRpcMethods::GetEthEstimateGasI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionCount", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthTransactionCountI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_sendRawTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::GetEthSendRawTransactionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionByHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthTransactionByHashI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("web3_clientVersion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetWeb3ClientVersionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("web3_sha3", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::GetWeb3Sha3I);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_mining", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthMiningI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_coinbase", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthCoinbaseI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getUncleByBlockHashAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthUncleBlockI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getUncleByBlockNumberAndIndex",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthUncleBlockI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getUncleCountByBlockHash",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         "param01", jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetEthUncleCountI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getUncleCountByBlockNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_ARRAY,
                         nullptr),
      &EthRpcMethods::GetEthUncleCountI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("net_version", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetNetVersionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("net_listening", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetNetListeningI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_protocolVersion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetProtocolVersionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("net_peerCount", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetNetPeerCountI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_chainId", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthChainIdI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_syncing", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthSyncingI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_accounts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthAccountsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getStorageAt", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, "param03",
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthStorageAtI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionReceipt",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthTransactionReceiptI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_newFilter", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &EthRpcMethods::EthNewFilterI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_newBlockFilter", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::EthNewBlockFilterI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_newPendingTransactionFilter",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::EthNewPendingTransactionFilterI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getFilterChanges", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::EthGetFilterChangesI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_uninstallFilter", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::EthUninstallFilterI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getFilterLogs", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::EthGetFilterLogsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getLogs", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &EthRpcMethods::EthGetLogsI);

  // Recover who the sender of a transaction was given only the RLP
  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_recoverTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &EthRpcMethods::EthRecoverTransactionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockReceipts", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::GetEthBlockReceiptsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("debug_traceTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_OBJECT, NULL),
      &EthRpcMethods::DebugTraceTransactionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_enable", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_BOOLEAN,
                         NULL),
      &EthRpcMethods::OtterscanEnableI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getInternalOperations",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::OtterscanGetInternalOperationsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_traceTransaction", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::OtterscanTraceTransactionI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure(
          "ots_searchTransactionsBefore", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING, "param02",
          jsonrpc::JSON_INTEGER, "param03", jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::OtterscanSearchTransactionsBeforeI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure(
          "ots_searchTransactionsAfter", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING, "param02",
          jsonrpc::JSON_INTEGER, "param03", jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::OtterscanSearchTransactionsAfterI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getTransactionBySenderAndNonce",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::OtterscanGetTransactionBySenderAndNonceI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getTransactionError", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::OtterscanGetTransactionErrorI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("debug_traceBlockByNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_STRING,
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_OBJECT, NULL),
      &EthRpcMethods::DebugTraceBlockByNumberI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("GetDSLeaderTxnPool", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, nullptr),
      &EthRpcMethods::GetDSLeaderTxnPoolI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("erigon_getHeaderByNumber",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_OBJECT,
                         "param01", jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::GetHeaderByNumberI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getApiLevel", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::GetOtterscanApiLevelI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_hasCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::HasCodeI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getBlockDetails", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER,
                         NULL),
      &EthRpcMethods::GetBlockDetailsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure(
          "ots_getBlockTransactions", jsonrpc::PARAMS_BY_POSITION,
          jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_INTEGER, "param02",
          jsonrpc::JSON_INTEGER, "param03", jsonrpc::JSON_INTEGER, NULL),
      &EthRpcMethods::GetBlockTransactionsI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("ots_getContractCreator", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &EthRpcMethods::GetContractCreatorI);
}

std::string EthRpcMethods::CreateTransactionEth(Eth::EthFields const &fields,
                                                zbytes const &pubKey,
                                                const unsigned int num_shards,
                                                const uint128_t &gasPrice) {
  TRACE(zil::trace::FilterClass::TXN);

  INC_CALLS(GetInvocationsCounter());

  std::string ret;

  if (!LOOKUP_NODE_MODE) {
    TRACE_ERROR("Message Sent to a non-lookup");

    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  if (Mediator::m_disableTxns) {
    TRACE_ERROR("Txns disabled - rejecting new txn");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }

  auto tx = GetTxFromFields(fields, pubKey, ret);
  // When we see TXs being submitted to this seedpub/lookup, we add it to the
  // pending TXn pool if we are in extended mode
  if (ARCHIVAL_LOOKUP_WITH_TX_TRACES) {
    m_sharedMediator.AddPendingTxn(tx);
  }

  // Add some attributes to the span
  {
    std::stringstream ss;
    ss << tx.GetTranID();
    span.SetAttribute("txn.id", ss.str());
    ss.clear();
    ss << tx.GetSenderAddr();
    span.SetAttribute("txn.from.id", ss.str());
    ss.clear();
    ss << tx.GetToAddr();
    span.SetAttribute("txn.to.id", ss.str());
  }

  try {
    const Address fromAddr = tx.GetSenderAddr();

    {
      unique_lock<shared_timed_mutex> lock(
          AccountStore::GetInstance().GetPrimaryMutex());

      const Account *sender =
          AccountStore::GetInstance().GetAccount(fromAddr, true);

      uint64_t minGasLimit = 0;
      if (Transaction::GetTransactionType(tx) ==
          Transaction::ContractType::CONTRACT_CREATION) {
        minGasLimit =
            Eth::getGasUnitsForContractDeployment(tx.GetCode(), tx.GetData());
      } else {
        minGasLimit = MIN_ETH_GAS;
      }
      if (!Eth::ValidateEthTxn(tx, fromAddr, sender, gasPrice, minGasLimit)) {
        TRACE_ERROR("failed to validate TX!");
        return ret;
      }
      TRACE_EVENT("Validated", "status", "OK");
    }

    if (tx.GetGasLimitZil() > DS_MICROBLOCK_GAS_LIMIT) {
      throw JsonRpcException(
          ServerBase::RPC_INVALID_PARAMETER,
          (boost::format(
               "txn gas limit exceeding ds maximum limit! Tx: %i DS: %i") %
           tx.GetGasLimitZil() % DS_MICROBLOCK_GAS_LIMIT)
              .str());
    }

    LOG_GENERAL(WARNING, "EthRpcMethods::CreateTransactionEth ADDR: "
                             << tx.GetSenderAddr()
                             << ", NONCE: " << tx.GetNonce()
                             << ", HASH: " << tx.GetTranID().hex()
                             << ", TO: " << tx.GetCoreInfo().toAddr.hex());

    switch (Transaction::GetTransactionType(tx)) {
      case Transaction::ContractType::NON_CONTRACT:
      case Transaction::ContractType::CONTRACT_CREATION:
      case Transaction::ContractType::CONTRACT_CALL: {
      } break;
      case Transaction::ContractType::ERROR:
        throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                               "Code is empty and To addr is null");
        break;
      default:
        throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                               "Txn type unexpected");
    }
    if (!m_sharedMediator.m_lookup->AddTxnToMemPool(tx)) {
      throw JsonRpcException(ServerBase::RPC_DATABASE_ERROR,
                             "Txn could not be added as database exceeded "
                             "limit or the txn was already present");
    }

  } catch (const JsonRpcException &je) {
    LOG_GENERAL(INFO, "[Error]" << je.what() << " Input: N/A");
    throw je;
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: N/A");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
  return ret;
}

Json::Value EthRpcMethods::GetBalanceAndNonce(const string &address) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  auto span = zil::trace::Tracing::CreateSpan(zil::trace::FilterClass::TXN,
                                              __FUNCTION__);

  INC_CALLS(GetInvocationsCounter());

  try {
    Address addr{ToBase16AddrHelper(address)};
    unique_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account *account = AccountStore::GetInstance().GetAccount(addr, true);

    Json::Value ret;
    if (account != nullptr) {
      const uint128_t &balance = account->GetBalance();
      uint64_t nonce = account->GetNonce();

      ret["balance"] = balance.str();
      ret["nonce"] = static_cast<unsigned int>(nonce);
      LOG_GENERAL(INFO,
                  "DEBUG: Addr: " << address << " balance: " << balance.str()
                                  << " nonce: " << nonce << " " << account);
    } else if (account == nullptr) {
      throw JsonRpcException(
          ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
          "Account with addr: " + address + " is not created");
    }

    return ret;
  } catch (const JsonRpcException &je) {
    LOG_GENERAL(INFO, "[Error] getting balance for acc: "
                          << address << ", msg: " << je.GetMessage());
    throw je;
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

string EthRpcMethods::GetEthCallZil(const Json::Value &_json) {
  auto span = zil::trace::Tracing::CreateSpan(zil::trace::FilterClass::TXN,
                                              __FUNCTION__);

  INC_CALLS(GetInvocationsCounter());

  return this->GetEthCallImpl(
      _json, {"fromAddr", "toAddr", "amount", "gasLimit", "data"});
}

string EthRpcMethods::GetEthCallEth(const Json::Value &_json,
                                    const string &block_or_tag) {
  INC_CALLS(GetInvocationsCounter());

  if (!isSupportedTag(block_or_tag)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMS,
                           "Unsupported block or tag in eth_call");
  }

  return this->GetEthCallImpl(_json, {"from", "to", "value", "gas", "data"});
}

// Convenience fn to extract the tracer - valid types are 'raw' and 'callTracer'
// This is as the tracer is a JSON which has both types as entries
Json::Value extractTracer(const std::string &tracer, const std::string &trace) {
  Json::Value parsed;

  try {
    Json::Value trace_json;
    JSONUtils::GetInstance().convertStrtoJson(trace, trace_json);

    if (tracer.compare("callTracer") == 0) {
      auto const item = trace_json["call_tracer"][0];
      parsed = item;
    } else if (tracer.compare("raw") == 0) {
      auto const item = trace_json["raw_tracer"];
      parsed = item;
    } else if (tracer.compare("otter_internal_tracer") == 0) {
      auto const item = trace_json["otter_internal_tracer"];
      if (item.isNull()) {
        parsed = Json::Value(Json::ValueType::arrayValue);
      } else {
        parsed = item;
      }
    } else if (tracer.compare("otter_call_tracer") == 0) {
      auto const item = trace_json["otter_call_tracer"];
      parsed = item;
    } else if (tracer.compare("otter_transaction_error") == 0) {
      auto const item = trace_json["otter_transaction_error"];
      // If there was no error return 0x
      if (item.isNull()) {
        parsed = Json::Value("0x");
      } else {
        parsed = item;
      }
    } else {
      throw JsonRpcException(
          ServerBase::RPC_MISC_ERROR,
          std::string(
              "Only callTracer, internal_tracer, otter_call_tracer, "
              "otter_transaction_error, and raw are supported. Received: ") +
              tracer);
    }
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what());
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }

  return parsed;
}

string EthRpcMethods::DebugTraceCallEth(const Json::Value &_json,
                                        const string &block_or_tag,
                                        const Json::Value &tracer) {
  INC_CALLS(GetInvocationsCounter());

  if (!isSupportedTag(block_or_tag)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMS,
                           "Unsupported block or tag in debug_TraceCall");
  }

  if (!TX_TRACES) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           "TX traces are not enabled for this server");
  }

  // Default to call tracer
  std::string tracerType = "callTracer";

  if (tracer.isMember("tracer")) {
    tracerType = tracer["tracer"].asString();
  }

  return this->GetEthCallImpl(_json, {"from", "to", "value", "gas", "data"},
                              tracerType);
}

// See
// https://github.com/ethereum/go-ethereum/blob/9b9a1b677d894db951dc4714ea1a46a2e7b74ffc/accounts/abi/abi.go#L242
bool EthRpcMethods::UnpackRevert(const std::string &data_in,
                                 std::string &message) {
  zbytes data(data_in.begin(), data_in.end());
  // 68 bytes is the minimum: 4 prefix + 32 offset + 32 string length.
  if (data.size() < 68 ||
      // Keccack-256("Error(string)")[:4] == 0x08c379a0
      !(data[0] == 0x08 && data[1] == 0xc3 && data[2] == 0x79 &&
        data[3] == 0xa0)) {
    TRACE_ERROR("Invalid revert data for unpacking");
    return false;
  }
  // Take offset of the parameter
  zbytes offset_vec(data.begin() + 4, data.begin() + 36);
  size_t offset =
      static_cast<size_t>(dev::fromBigEndian<dev::u256, zbytes>(offset_vec));
  zbytes len_vec(data.begin() + 4 + offset, data.begin() + 4 + offset + 32);
  size_t len =
      static_cast<size_t>(dev::fromBigEndian<dev::u256, zbytes>(len_vec));
  message.clear();
  if (data.size() < 4 + offset + 32 + len) {
    TRACE_ERROR("Invalid revert data for unpacking");
    return false;
  }
  std::copy(data.begin() + 4 + offset + 32,
            data.begin() + 4 + offset + 32 + len, std::back_inserter(message));
  return true;
}

std::string EthRpcMethods::GetEthEstimateGas(const Json::Value &json,
                                             const std::string *block_or_tag) {
  Address fromAddr;

  auto span = zil::trace::Tracing::CreateSpan(zil::trace::FilterClass::TXN,
                                              __FUNCTION__);

  INC_CALLS(GetInvocationsCounter());

  if (block_or_tag != nullptr && !isSupportedTag(*block_or_tag)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMS,
                           "Unsupported block or tag in eth_call");
  }

  if (!json.isMember("from")) {
    TRACE_ERROR("Missing from account");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Missing from field");
  } else {
    fromAddr = Address{json["from"].asString()};
  }

  Address toAddr;

  if (json.isMember("to")) {
    auto toAddrStr = json["to"].asString();
    DataConversion::NormalizeHexString(toAddrStr);
    toAddr = Address{toAddrStr};
  }

  zbytes code;
  uint256_t accountFunds{};
  bool contractCreation = false;
  {
    unique_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account *sender =
        !IsNullAddress(fromAddr)
            ? AccountStore::GetInstance().GetAccount(fromAddr, true)
            : nullptr;
    if (sender == nullptr) {
      TRACE_ERROR("Sender doesn't exist");
      throw JsonRpcException(
          ServerBase::RPC_MISC_ERROR,
          "The sender of the tx doesn't appear to have funds");
    }
    accountFunds = sender->GetBalance();

    const Account *toAccount =
        !IsNullAddress(toAddr)
            ? AccountStore::GetInstance().GetAccount(toAddr, true)
            : nullptr;

    if (toAccount != nullptr && toAccount->isContract()) {
      code = toAccount->GetCode();
    } else if (toAccount == nullptr) {
      if (!ENABLE_CPS) {
        toAddr = Account::GetAddressForContract(fromAddr, sender->GetNonce(),
                                                TRANSACTION_VERSION_ETH_LEGACY);
      }
      contractCreation = true;
    }
  }

  zbytes data;
  if (json.isMember("data")) {
    if (!DataConversion::HexStrToUint8Vec(json["data"].asString(), data)) {
      TRACE_ERROR("data argument invalid");
      throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                             "data argument invalid");
    }
  }

  uint256_t value = 0;
  if (json.isMember("value")) {
    const auto valueStr = json["value"].asString();
    value = DataConversion::ConvertStrToInt<uint256_t>(valueStr, 0);
  }

  uint256_t gasPrice = GetEthGasPriceNum();
  if (json.isMember("gasPrice")) {
    const auto gasPriceStr = json["gasPrice"].asString();
    uint256_t inputGasPrice =
        DataConversion::ConvertStrToInt<uint256_t>(gasPriceStr, 0);
    gasPrice = max(gasPrice, inputGasPrice);
  }
  uint256_t gasDeposit = 0;
  if (!SafeMath<uint256_t>::mul(gasPrice, MIN_ETH_GAS, gasDeposit)) {
    TRACE_ERROR("gasPrice * MIN_ETH_GAS overflow!");
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "gasPrice * MIN_ETH_GAS overflow!");
  }
  uint256_t balance = 0;
  if (!SafeMath<uint256_t>::mul(accountFunds, EVM_ZIL_SCALING_FACTOR,
                                balance)) {
    TRACE_ERROR("accountFunds * EVM_ZIL_SCALING_FACTOR overflow!");
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "accountFunds * EVM_ZIL_SCALING_FACTOR overflow!");
  }

  if (balance < gasDeposit) {
    TRACE_ERROR("Insufficient funds to perform this operation");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           "Insufficient funds to perform this operation");
  }

  // Typical fund transfer
  if (code.empty() && data.empty()) {
    return (boost::format("0x%x") % MIN_ETH_GAS).str();
  }

  if (contractCreation && code.empty() && !data.empty()) {
    std::swap(data, code);
  }

  uint64_t gas = GasConv::GasUnitsFromCoreToEth(2 * DS_MICROBLOCK_GAS_LIMIT);

  // Use gas specified by user
  if (json.isMember("gas")) {
    const auto gasLimitStr = json["gas"].asString();
    const uint64_t userGas =
        DataConversion::ConvertStrToInt<uint64_t>(gasLimitStr, 0);
    gas = min(gas, userGas);
  }

  const auto txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
  const auto dsBlock = m_sharedMediator.m_dsBlockChain.GetLastBlock();
  // TODO: adapt to any block, not just latest.
  TxnExtras txnExtras{
      dsBlock.GetHeader().GetGasPrice(),
      txBlock.GetTimestamp() / 1000000,  // From microseconds to seconds.
      dsBlock.GetHeader().GetDifficulty()};
  uint64_t blockNum =
      m_sharedMediator.m_txBlockChain.GetLastBlock().GetHeader().GetBlockNum();

  {
    std::stringstream ss;
    ss << "msg " << fromAddr << " to " << toAddr << " gas " << gas << " value "
       << value << " block " << blockNum;
    TRACE_EVENT("GasEstimate", "informational", ss.str());
  }

  EvmProcessContext evmMessageContext(fromAddr, toAddr, code, data, gas, value,
                                      blockNum, txnExtras, "eth_estimateGas",
                                      true, false);

  evm::EvmResult result;

  if (AccountStore::GetInstance().EvmProcessMessageTemp(evmMessageContext,
                                                        result) &&
      result.exit_reason().exit_reason_case() ==
          evm::ExitReason::ExitReasonCase::kSucceed) {
    const auto gasRemained = result.remaining_gas();
    const auto consumedEvmGas =
        (gas >= gasRemained) ? (gas - gasRemained) : gas;
    const auto baseFee = contractCreation
                             ? Eth::getGasUnitsForContractDeployment(code, data)
                             : MIN_ETH_GAS;
    uint64_t retGas = 0;

    if (ENABLE_CPS) {
      retGas = consumedEvmGas;
    } else {
      retGas = consumedEvmGas + baseFee;
    }

    // We can't go beyond gas provided by user (or taken from last block)
    if (retGas >= gas) {
      throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                             "Base fee exceeds gas limit");
    }
    LOG_GENERAL(WARNING, "Gas estimated: " << retGas);

    return (boost::format("0x%x") % retGas).str();
  } else if (result.exit_reason().exit_reason_case() ==
             evm::ExitReason::kRevert) {
    // Error code 3 is a special case. It is practially documented only in geth
    // and its clones, e.g. here:
    // https://github.com/ethereum/go-ethereum/blob/9b9a1b677d894db951dc4714ea1a46a2e7b74ffc/internal/ethapi/api.go#L1026
    std::string return_value;
    DataConversion::StringToHexStr(result.return_value(), return_value);
    boost::algorithm::to_lower(return_value);
    std::string revert_error_str;
    std::ostringstream message;
    message << "execution reverted";
    if (UnpackRevert(result.return_value(), revert_error_str)) {
      message << ": " << revert_error_str;
    }
    throw JsonRpcException(3, message.str(), "0x" + return_value);
  } else {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           EvmUtils::ExitReasonString(result.exit_reason()));
  }
}

string EthRpcMethods::GetEthCallImpl(const Json::Value &_json,
                                     const ApiKeys &apiKeys,
                                     std::string const &tracer) {
  LOG_GENERAL(WARNING, "GetEthCall:" << _json);
  TRACE(zil::trace::FilterClass::DEMO);
  INC_CALLS(GetInvocationsCounter());

  const auto &addr = JSONConversion::checkJsonGetEthCall(_json, apiKeys.to);
  zbytes code{};
  auto success{false};
  {
    unique_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());
    Account *contractAccount =
        AccountStore::GetInstance().GetAccount(addr, true);

    if (contractAccount == nullptr) {
      LOG_GENERAL(WARNING, "Eth call made to location that had no code...");
      return "0x";
    }
    code = contractAccount->GetCode();
  }

  evm::EvmResult result;
  try {
    Address fromAddr;
    if (_json.isMember(apiKeys.from)) {
      fromAddr = Address(_json[apiKeys.from].asString());
    }

    uint256_t value = 0;
    if (_json.isMember(apiKeys.value)) {
      const auto valueStr = _json[apiKeys.value].asString();
      value = DataConversion::ConvertStrToInt<uint256_t>(valueStr, 0);
    }

    // for now set total gas as twice the ds gas limit
    uint64_t gasRemained =
        GasConv::GasUnitsFromCoreToEth(2 * DS_MICROBLOCK_GAS_LIMIT);
    if (_json.isMember(apiKeys.gas)) {
      const auto gasLimit_str = _json[apiKeys.gas].asString();
      const uint64_t userGas =
          DataConversion::ConvertStrToInt<uint64_t>(gasLimit_str, 0);
      gasRemained = min(gasRemained, userGas);
      if (gasRemained < MIN_ETH_GAS) {
        throw JsonRpcException(3, "execution reverted", "0x");
      }
    }

    zbytes data;
    if (!DataConversion::HexStrToUint8Vec(_json[apiKeys.data].asString(),
                                          data)) {
      TRACE_ERROR("Data Argument invalid");
      throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                             "data argument invalid");
    }

    const auto txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
    const auto dsBlock = m_sharedMediator.m_dsBlockChain.GetLastBlock();
    // TODO: adapt to any block, not just latest.
    TxnExtras txnExtras{
        dsBlock.GetHeader().GetGasPrice(),
        txBlock.GetTimestamp() / 1000000,  // From microseconds to seconds.
        dsBlock.GetHeader().GetDifficulty()};
    uint64_t blockNum = m_sharedMediator.m_txBlockChain.GetLastBlock()
                            .GetHeader()
                            .GetBlockNum();

    /*
     * EVM estimate only is currently disabled, as per n-hutton advice.
     */
    EvmProcessContext evmMessageContext(fromAddr, addr, code, data, gasRemained,
                                        value, blockNum, txnExtras, "eth_call",
                                        false, true);

    if (AccountStore::GetInstance().EvmProcessMessageTemp(evmMessageContext,
                                                          result) &&
        result.exit_reason().exit_reason_case() ==
            evm::ExitReason::ExitReasonCase::kSucceed) {
      success = true;
    }
  } catch (const exception &e) {
    LOG_GENERAL(WARNING, "Error: " << e.what());
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to process");
  }

  // tracerException : we want only the call trace
  if (!tracer.empty()) {
    LOG_GENERAL(WARNING, "Returning trace! ");
    LOG_GENERAL(WARNING, result.tx_trace());
    return extractTracer(tracer, result.tx_trace()).asString();
  }

  std::string return_value;
  DataConversion::StringToHexStr(result.return_value(), return_value);
  boost::algorithm::to_lower(return_value);
  if (success) {
    return "0x" + return_value;
  } else if (result.exit_reason().exit_reason_case() ==
             evm::ExitReason::kRevert) {
    LOG_GENERAL(WARNING, "Warning! Execution reverted...");
    // Error code 3 is a special case. It is practially documented only in geth
    // and its clones, e.g. here:
    // https://github.com/ethereum/go-ethereum/blob/9b9a1b677d894db951dc4714ea1a46a2e7b74ffc/internal/ethapi/api.go#L1026
    std::string revert_error_str;
    std::ostringstream message;
    message << "execution reverted";
    if (UnpackRevert(result.return_value(), revert_error_str)) {
      message << ": " << revert_error_str;
    }
    throw JsonRpcException(3, message.str(), "0x" + return_value);
  } else {
    LOG_GENERAL(WARNING, "Warning! Misc error...");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           EvmUtils::ExitReasonString(result.exit_reason()));
  }
}

std::string EthRpcMethods::GetWeb3ClientVersion() {
  INC_CALLS(GetInvocationsCounter());

  return "Zilliqa/v8.2";
}

string EthRpcMethods::GetWeb3Sha3(const Json::Value &_json) {
  INC_CALLS(GetInvocationsCounter());

  zbytes input = DataConversion::HexStrToUint8VecRet(_json.asString());
  return POW::BlockhashToHexString(
      ethash::keccak256(input.data(), input.size()));
}

Json::Value EthRpcMethods::GetEthUncleCount() {
  INC_CALLS(GetInvocationsCounter());

  // There's no concept of longest chain hence there will be no uncles
  // Return 0 instead
  return Json::Value{"0x0"};
}

Json::Value EthRpcMethods::GetEthUncleBlock() {
  INC_CALLS(GetInvocationsCounter());

  // There's no concept of longest chain hence there will be no uncles
  // Return null instead
  return Json::nullValue;
}

Json::Value EthRpcMethods::GetEthMining() {
  INC_CALLS(GetInvocationsCounter());

  return Json::Value(false);
}

std::string EthRpcMethods::GetEthCoinbase() {
  INC_CALLS(GetInvocationsCounter());

  throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                         "Unsupported method: eth_coinbase. Zilliqa mining "
                         "model is different from that of Etherium");
}

Json::Value EthRpcMethods::GetNetListening() {
  INC_CALLS(GetInvocationsCounter());

  return Json::Value(true);
}

std::string EthRpcMethods::GetNetPeerCount() {
  INC_CALLS(GetInvocationsCounter());

  return "0x0";
}

std::string EthRpcMethods::GetProtocolVersion() {
  INC_CALLS(GetInvocationsCounter());

  return "0x41";  // Similar to Infura, Alchemy
}

std::string EthRpcMethods::GetEthChainId() {
  INC_CALLS(GetInvocationsCounter());

  return (boost::format("0x%x") % ETH_CHAINID).str();
}

std::string EthRpcMethods::GetNetVersion() {
  INC_CALLS(GetInvocationsCounter());

  return (boost::format("%d") % ETH_CHAINID).str();
}

Json::Value EthRpcMethods::GetEthSyncing() {
  INC_CALLS(GetInvocationsCounter());

  return Json::Value(false);
}

Json::Value EthRpcMethods::GetEmptyResponse() {
  INC_CALLS(GetInvocationsCounter());

  const Json::Value expectedResponse = Json::arrayValue;
  return expectedResponse;
}

Json::Value EthRpcMethods::GetEthTransactionByHash(
    const std::string &transactionHash) {
  INC_CALLS(GetInvocationsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }
  try {
    TxBodySharedPtr transactionBodyPtr;
    TxnHash tranHash(transactionHash);
    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxBody(tranHash, transactionBodyPtr);
    if (!isPresent) {
      return Json::nullValue;
    }

    const TxBlock EMPTY_BLOCK;
    const auto txBlock = GetBlockFromTransaction(*transactionBodyPtr);
    if (txBlock == EMPTY_BLOCK) {
      LOG_GENERAL(WARNING, "Unable to get the TX from a minted block!");
      return Json::nullValue;
    }

    constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
    auto transactionIndex =
        GetTransactionIndexFromBlock(txBlock, transactionHash);
    if (transactionIndex == WRONG_INDEX) {
      return Json::nullValue;
    }

    return JSONConversion::convertTxtoEthJson(transactionIndex,
                                              *transactionBodyPtr, txBlock);
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << transactionHash);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::GetEthStorageAt(std::string const &address,
                                           std::string const &position,
                                           std::string const & /*blockNum*/) {
  INC_CALLS(GetInvocationsCounter());

  Json::Value indices = Json::arrayValue;

  if (Mediator::m_disableGetSmartContractState) {
    LOG_GENERAL(WARNING, "API disabled");
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST, "API disabled");
  }

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  try {
    Address addr{ToBase16AddrHelper(address)};
    unique_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account *account = AccountStore::GetInstance().GetAccount(addr, true);

    if (account == nullptr) {
      throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                             "Address does not exist");
    }

    if (!account->isContract()) {
      throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                             "Address not contract address");
    }
    LOG_GENERAL(INFO, "Contract address: " << address);
    Json::Value root;
    const auto indices_vector =
        JSONConversion::convertJsonArrayToVector(indices);

    string vname{};
    if (!account->FetchStateJson(root, vname, indices_vector)) {
      throw JsonRpcException(ServerBase::RPC_INTERNAL_ERROR,
                             "FetchStateJson failed");
    }

    LOG_GENERAL(INFO, "State JSON: " << root);

    // Attempt to get storage at position.
    // Left-pad position with 0s up to 64
    std::string zeroes =
        "0000000000000000000000000000000000000000000000000000000000000000";

    auto positionIter = position.begin();
    auto zeroIter = zeroes.begin();

    // Move position iterator past '0x' if it exists
    if (position.size() > 2 && position[0] == '0' && position[1] == 'x') {
      std::advance(positionIter, 2);
    }

    if ((position.end() - positionIter) > static_cast<int>(zeroes.size())) {
      throw JsonRpcException(ServerBase::RPC_INTERNAL_ERROR,
                             "position string is too long! " + position);
    }

    std::advance(zeroIter,
                 zeroes.size() - std::distance(positionIter, position.end()));

    zeroes.replace(zeroIter, zeroes.end(), positionIter, position.end());

    // Must be uppercase
    std::transform(zeroes.begin(), zeroes.end(), zeroes.begin(), ::toupper);

    auto res = root["_evm_storage"][zeroes];
    zbytes resAsStringBytes;

    for (const auto &item : res.asString()) {
      resAsStringBytes.push_back(item);
    }

    auto const resAsStringHex =
        std::string("0x") +
        DataConversion::Uint8VecToHexStrRet(resAsStringBytes);

    return resAsStringHex;
  } catch (const JsonRpcException &je) {
    throw je;
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthCode(std::string const &address,
                                      std::string const & /*blockNum*/) {
  INC_CALLS(GetInvocationsCounter());

  zbytes code;
  try {
    Address addr{address, Address::FromHex};
    unique_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account *account = AccountStore::GetInstance().GetAccount(addr, true);
    if (account) {
      code = StripEVM(account->GetCode());
    }
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
  }
  std::string result{"0x"};
  boost::algorithm::hex_lower(code.begin(), code.end(),
                              std::back_inserter(result));
  return result;
}

Json::Value EthRpcMethods::GetEthBlockNumber() {
  Json::Value ret;

  INC_CALLS(GetInvocationsCounter());

  try {
    const auto txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();

    auto const height = txBlock.GetHeader().GetBlockNum() ==
                                std::numeric_limits<uint64_t>::max()
                            ? 1
                            : txBlock.GetHeader().GetBlockNum();

    std::ostringstream returnVal;
    returnVal << "0x" << std::hex << height << std::dec;
    ret = returnVal.str();
  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " When getting block number!");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }

  return ret;
}

Json::Value EthRpcMethods::GetEthBlockByNumber(
    const std::string &blockNumberStr, const bool includeFullTransactions) {
  INC_CALLS(GetInvocationsCounter());

  try {
    TxBlock txBlock;

    if (!isSupportedTag(blockNumberStr)) {
      return Json::nullValue;
    } else if (blockNumberStr == "latest" ||    //
               blockNumberStr == "earliest" ||  //
               blockNumberStr == "pending" ||   //
               isNumber(blockNumberStr)) {
      // handle latest, earliest and block number requests
      if (blockNumberStr == "latest") {
        txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
      } else if (blockNumberStr == "pending") {
        txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();

        // Special case for pending... modify the last block to fake a pending
        // bloc
        auto const pending = m_sharedMediator.GetPendingTxns();

        std::vector<TxBodySharedPtr> transactions;

        for (auto const &tx : pending) {
          auto receipt = TransactionReceipt();
          receipt.update();
          TxBodySharedPtr item =
              std::make_shared<TransactionWithReceipt>(tx, receipt);
          transactions.push_back(item);
        }

        const auto dsBlock = m_sharedMediator.m_dsBlockChain.GetBlock(
            txBlock.GetHeader().GetDSBlockNum());

        auto toRet = JSONConversion::convertTxBlocktoEthJson(
            txBlock, dsBlock, transactions, includeFullTransactions);

        // Now modify the fields as if this block was in the future
        toRet["hash"] = Json::nullValue;
        toRet["logsBloom"] = Json::nullValue;
        toRet["nonce"] = Json::nullValue;
        toRet["number"] = Json::nullValue;

        return toRet;
      } else if (blockNumberStr == "earliest") {
        txBlock = m_sharedMediator.m_txBlockChain.GetBlock(0);
      } else if (isNumber(blockNumberStr)) {  // exact block number
        const uint64_t blockNum =
            std::strtoull(blockNumberStr.c_str(), nullptr, 0);
        txBlock = m_sharedMediator.m_txBlockChain.GetBlock(blockNum);
      }
    } else {
      // Not supported
      return Json::nullValue;
    }

    const TxBlock NON_EXISTING_TX_BLOCK{};
    if (txBlock == NON_EXISTING_TX_BLOCK) {
      return Json::nullValue;
    }

    return GetEthBlockCommon(txBlock, includeFullTransactions);
  } catch (const std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr
                                << ", includeFullTransactions: "
                                << includeFullTransactions);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockByHash(
    const std::string &inputHash, const bool includeFullTransactions) {
  INC_CALLS(GetInvocationsCounter());

  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);
    const TxBlock NON_EXISTING_TX_BLOCK{};
    if (txBlock == NON_EXISTING_TX_BLOCK) {
      return Json::nullValue;
    }
    return GetEthBlockCommon(txBlock, includeFullTransactions);

  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash
                                << ", includeFullTransactions: "
                                << includeFullTransactions);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockCommon(
    const TxBlock &txBlock, const bool includeFullTransactions) {
  INC_CALLS(GetInvocationsCounter());

  const auto dsBlock = m_sharedMediator.m_dsBlockChain.GetBlock(
      txBlock.GetHeader().GetDSBlockNum());

  std::vector<TxBodySharedPtr> transactions;

  // Gather either transaction hashes or full transactions
  const auto &microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const &mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }

    MicroBlockSharedPtr microBlockPtr;

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto &currTranHashes = microBlockPtr->GetTranHashes();

    for (const auto &transactionHash : currTranHashes) {
      TxBodySharedPtr transactionBodyPtr;
      if (!BlockStorage::GetBlockStorage().GetTxBody(transactionHash,
                                                     transactionBodyPtr)) {
        continue;
      }
      transactions.push_back(std::move(transactionBodyPtr));
    }
  }

  return JSONConversion::convertTxBlocktoEthJson(txBlock, dsBlock, transactions,
                                                 includeFullTransactions);
}

Json::Value EthRpcMethods::GetEthBalance(const std::string &address,
                                         const std::string &tag) {
  INC_CALLS(GetInvocationsCounter());

  if (isSupportedTag(tag)) {
    uint256_t ethBalance{0};
    try {
      auto ret = this->GetBalanceAndNonce(address);
      ethBalance.assign(ret["balance"].asString());
    } catch (const JsonRpcException &) {
      // default ethBalance.
    } catch (const std::runtime_error &e) {
      throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                             "Invalid account balance number");
    }
    uint256_t ethBalanceScaled;
    if (!SafeMath<uint256_t>::mul(ethBalance, EVM_ZIL_SCALING_FACTOR,
                                  ethBalanceScaled)) {
      throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                             "GetEthBalance overflow");
    }

    std::ostringstream strm;
    strm << "0x" << std::hex << ethBalanceScaled << std::dec;

    return strm.str();
  }
  throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                         "Unable To Process, invalid tag");

  return "";
}

uint256_t EthRpcMethods::GetEthGasPriceNum() const {
  INC_CALLS(GetInvocationsCounter());

  uint256_t gasPrice =
      m_sharedMediator.m_dsBlockChain.GetLastBlock().GetHeader().GetGasPrice();
  // Make gas price in wei
  gasPrice = (gasPrice * EVM_ZIL_SCALING_FACTOR) / GasConv::GetScalingFactor();

  // The following ensures we get 'at least' that high price as it was before
  // dividing by GasScalingFactor
  gasPrice += 1000000;
  return gasPrice;
}

Json::Value EthRpcMethods::GetEthGasPrice() const {
  INC_CALLS(GetInvocationsCounter());

  try {
    std::ostringstream strm;

    strm << "0x" << std::hex << GetEthGasPriceNum() << std::dec;
    return strm.str();
  } catch (const std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what());

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockTransactionCountByHash(
    const std::string &inputHash) {
  INC_CALLS(GetInvocationsCounter());

  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);

    std::ostringstream strm;
    strm << "0x" << std::hex << txBlock.GetHeader().GetNumTxs() << std::dec;

    return strm.str();

  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockTransactionCountByNumber(
    const std::string &blockNumberStr) {
  INC_CALLS(GetInvocationsCounter());

  try {
    TxBlock txBlock;

    if (blockNumberStr == "latest") {
      txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
    } else if (blockNumberStr == "earliest") {
      txBlock = m_sharedMediator.m_txBlockChain.GetBlock(0);
    } else if (blockNumberStr == "pending") {
      // Not supported
      return "0x0";
    } else {
      const uint64_t blockNum =
          std::strtoull(blockNumberStr.c_str(), nullptr, 0);
      txBlock = m_sharedMediator.m_txBlockChain.GetBlock(blockNum);
    }
    std::ostringstream strm;
    strm << "0x" << std::hex << txBlock.GetHeader().GetNumTxs() << std::dec;

    return strm.str();

  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionByBlockHashAndIndex(
    const std::string &inputHash, const std::string &indexStr) const {
  INC_CALLS(GetInvocationsCounter());

  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);
    const uint64_t index = std::strtoull(indexStr.c_str(), nullptr, 0);
    return GetEthTransactionFromBlockByIndex(txBlock, index);

  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionByBlockNumberAndIndex(
    const std::string &blockNumberStr, const std::string &indexStr) const {
  INC_CALLS(GetInvocationsCounter());

  try {
    TxBlock txBlock;
    if (blockNumberStr == "latest") {
      txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
    } else if (blockNumberStr == "earliest") {
      txBlock = m_sharedMediator.m_txBlockChain.GetBlock(0);
    } else if (blockNumberStr == "pending") {
      // Not supported
      return Json::nullValue;
    } else {
      const uint64_t blockNum =
          std::strtoull(blockNumberStr.c_str(), nullptr, 0);
      txBlock = m_sharedMediator.m_txBlockChain.GetBlock(blockNum);
    }
    const uint64_t index = std::strtoull(indexStr.c_str(), nullptr, 0);
    return GetEthTransactionFromBlockByIndex(txBlock, index);
  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionFromBlockByIndex(
    const TxBlock &txBlock, uint64_t index) const {
  INC_CALLS(GetInvocationsCounter());

  const TxBlock EMPTY_BLOCK;
  constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
  if (txBlock == EMPTY_BLOCK || index == WRONG_INDEX) {
    return Json::nullValue;
  }
  uint64_t processedIndexes = 0;
  MicroBlockSharedPtr microBlockPtr;
  boost::optional<uint64_t> indexInBlock;

  const auto &microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const &mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto &currTranHashes = microBlockPtr->GetTranHashes();

    if (processedIndexes + currTranHashes.size() > index) {
      // We found a block containing transaction
      indexInBlock = index - processedIndexes;
      break;
    } else {
      processedIndexes += currTranHashes.size();
    }
  }
  // Possibly out of range index or block with no transactions
  if (!indexInBlock) {
    return Json::nullValue;
  }

  TxBodySharedPtr transactionBodyPtr;
  const auto txHashes = microBlockPtr->GetTranHashes();
  if (!BlockStorage::GetBlockStorage().GetTxBody(txHashes[indexInBlock.value()],
                                                 transactionBodyPtr)) {
    return Json::nullValue;
  }

  return JSONConversion::convertTxtoEthJson(indexInBlock.value(),
                                            *transactionBodyPtr, txBlock);
}

Json::Value EthRpcMethods::GetEthTransactionReceipt(
    const std::string &txnhash) {
  INC_CALLS(GetInvocationsCounter());

  try {
    TxnHash argHash{txnhash};
    TxBodySharedPtr transactionBodyPtr;
    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxBody(argHash, transactionBodyPtr);
    if (!isPresent) {
      LOG_GENERAL(WARNING, "Unable to find transaction for given hash");
      return Json::nullValue;
    }

    const TxBlock EMPTY_BLOCK;
    auto txBlock = GetBlockFromTransaction(*transactionBodyPtr);
    if (txBlock == EMPTY_BLOCK) {
      LOG_GENERAL(WARNING, "Tx receipt requested but not found in any blocks. "
                               << txnhash);
      return Json::nullValue;
    }

    constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
    const auto transactionIndex =
        GetTransactionIndexFromBlock(txBlock, txnhash);
    if (transactionIndex == WRONG_INDEX) {
      LOG_GENERAL(WARNING, "Tx index requested but not found");
      return Json::nullValue;
    }

    auto const ethResult = JSONConversion::convertTxtoEthJson(
        transactionIndex, *transactionBodyPtr, txBlock);
    auto const zilResult = JSONConversion::convertTxtoJson(*transactionBodyPtr);

    auto receipt = zilResult["receipt"];

    std::string hashId = ethResult["hash"].asString();
    bool success = receipt["success"].asBool();
    std::string sender = ethResult["from"].asString();
    std::string toAddr = ethResult["to"].asString();
    std::string gasPrice = ethResult["gasPrice"].asString();
    std::string cumGas =
        (boost::format("0x%x") %
         GasConv::GasUnitsFromCoreToEth(
             transactionBodyPtr->GetTransactionReceipt().GetCumGas()))
            .str();

    const TxBlockHeader &txHeader = txBlock.GetHeader();
    const std::string blockNumber =
        (boost::format("0x%x") % txHeader.GetBlockNum()).str();
    const std::string blockHash =
        (boost::format("0x%x") % txBlock.GetBlockHash().hex()).str();

    Json::Value contractAddress =
        ethResult.get("contractAddress", Json::nullValue);

    auto logs =
        Eth::GetLogsFromReceipt(transactionBodyPtr->GetTransactionReceipt());

    logs = Eth::ConvertScillaEventsToEvm(logs);

    const auto baselogIndex =
        Eth::GetBaseLogIndexForReceiptInBlock(argHash, txBlock);

    Eth::DecorateReceiptLogs(logs, txnhash, blockHash, blockNumber,
                             transactionIndex, baselogIndex);
    const auto bloomLogs = Eth::GetBloomFromReceiptHex(
        transactionBodyPtr->GetTransactionReceipt());
    auto res = Eth::populateReceiptHelper(
        hashId, success, sender, toAddr, cumGas, gasPrice, blockHash,
        blockNumber, contractAddress, logs, bloomLogs, transactionIndex,
        transactionBodyPtr->GetTransaction());

    return res;
  } catch (const JsonRpcException &je) {
    throw je;
  } catch (exception &e) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           string("Unable To find hash for txn: ") + e.what());
  }

  return Json::nullValue;
}

std::string EthRpcMethods::EthNewFilter(const Json::Value &param) {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewEventFilter(param);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

std::string EthRpcMethods::EthNewBlockFilter() {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewBlockFilter();
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

std::string EthRpcMethods::EthNewPendingTransactionFilter() {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewPendingTxnFilter();
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

Json::Value EthRpcMethods::EthGetFilterChanges(const std::string &filter_id) {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetFilterChanges(filter_id);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

bool EthRpcMethods::EthUninstallFilter(const std::string &filter_id) {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  return api.UninstallFilter(filter_id);
}

Json::Value EthRpcMethods::EthGetFilterLogs(const std::string &filter_id) {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetFilterLogs(filter_id);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

Json::Value EthRpcMethods::EthGetLogs(const Json::Value &param) {
  INC_CALLS(GetInvocationsCounter());

  auto &api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetLogs(param);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

void EthRpcMethods::EnsureEvmAndLookupEnabled() {
  INC_CALLS(GetInvocationsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }
  if (!ENABLE_EVM) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "EVM mode disabled");
  }
}

TxBlock EthRpcMethods::GetBlockFromTransaction(
    const TransactionWithReceipt &transaction) const {
  INC_CALLS(GetInvocationsCounter());

  const TxBlock EMPTY_BLOCK;
  const auto txReceipt = transaction.GetTransactionReceipt();

  const Json::Value blockNumStr = txReceipt.GetJsonValue().get("epoch_num", "");

  try {
    if (!blockNumStr.isString() || blockNumStr.asString().empty()) {
      LOG_GENERAL(WARNING, "Block number is string or is empty!");
      return EMPTY_BLOCK;
    }
    const uint64_t blockNum =
        std::strtoull(blockNumStr.asCString(), nullptr, 0);
    const auto txBlock = m_sharedMediator.m_txBlockChain.GetBlock(blockNum);
    return txBlock;
  } catch (std::exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what()
                                << " while getting block number from receipt!");
    return EMPTY_BLOCK;
  }
}

uint64_t EthRpcMethods::GetTransactionIndexFromBlock(
    const TxBlock &txBlock, const std::string &txnhash) const {
  INC_CALLS(GetInvocationsCounter());

  TxnHash argHash{txnhash};
  const TxBlock EMPTY_BLOCK;
  constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
  if (txBlock == EMPTY_BLOCK) {
    return WRONG_INDEX;
  }

  uint64_t transactionIndex = 0;
  MicroBlockSharedPtr microBlockPtr;

  const auto &microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const &mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }
    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }
    const auto &tranHashes = microBlockPtr->GetTranHashes();
    for (size_t i = 0; i < tranHashes.size(); ++i, ++transactionIndex) {
      if (argHash == tranHashes[i]) {
        return transactionIndex;
      }
    }
  }

  return WRONG_INDEX;
}

// Given a transmitted RLP, return checksum-encoded original sender address
std::string EthRpcMethods::EthRecoverTransaction(
    const std::string &txnRpc) const {
  INC_CALLS(GetInvocationsCounter());

  auto const pubKeyBytes = RecoverECDSAPubKey(txnRpc, ETH_CHAINID);

  auto const asAddr = CreateAddr(pubKeyBytes);

  auto addrChksum = AddressChecksum::GetChecksummedAddressEth(
      DataConversion::Uint8VecToHexStrRet(asAddr.asBytes()));

  return DataConversion::AddOXPrefix(std::move(addrChksum));
}

Json::Value EthRpcMethods::GetEthBlockReceipts(const std::string &blockId) {
  // The easiest way to do this:
  // Get the block + transactions
  // Call TX receipt function

  INC_CALLS(GetInvocationsCounter());

  auto const block = GetEthBlockByHash(blockId, false);
  auto const txs = block["transactions"];

  Json::Value res = Json::arrayValue;

  for (const auto &tx : txs) {
    auto const receipt = GetEthTransactionReceipt(tx.asString());
    res.append(receipt);
  }

  return res;
}

Json::Value EthRpcMethods::GetDSLeaderTxnPool() {
  INC_CALLS(GetInvocationsCounter());

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  auto txns = m_sharedMediator.m_lookup->GetDSLeaderTxnPool();
  if (!txns) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }

  Json::Value res = Json::arrayValue;
  for (const auto &txn : *txns) {
    res.append(JSONConversion::convertTxtoJson(txn));
  }

  return res;
}

Json::Value EthRpcMethods::DebugTraceBlockByNumber(const std::string &blockNum,
                                                   const Json::Value &json) {
  auto blockByNumber = GetEthBlockByNumber(blockNum, false);

  Json::Value ret = Json::objectValue;
  Json::Value calls = Json::arrayValue;

  for (auto &tx : blockByNumber["transactions"]) {
    auto traced = DebugTraceTransaction(tx.asString(), json);
    calls.append(traced);
  }

  ret["calls"] = calls;
  return ret;
}

Json::Value EthRpcMethods::DebugTraceTransaction(const std::string &txHash,
                                                 const Json::Value &json) {
  if (!ARCHIVAL_LOOKUP_WITH_TX_TRACES) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           "The node is not configured to store Tx traces");
  }

  if (!TX_TRACES) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           "The node is not configured to generate Tx traces");
  }

  if (!json.isMember("tracer")) {
    LOG_GENERAL(WARNING, "Missing tracer field");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Missing tracer field");
  }

  std::string trace;

  try {
    TxnHash tranHash(txHash);

    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxTrace(tranHash, trace);

    if (!isPresent) {
      LOG_GENERAL(INFO, "Trace request failed! ");
      return Json::nullValue;
    }

    return extractTracer(json["tracer"].asString(), trace);
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << ". Input: " << txHash);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::OtterscanSearchTransactions(
    const std::string &address, unsigned long blockNumber,
    unsigned long pageSize, bool before) {
  if (!ARCHIVAL_LOOKUP_WITH_TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  if (!TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  // Records whether blockNumber was 0 on input
  bool blockNumberWasZero = !blockNumber;

  // if blocnumber is 0 and it's a before search, then we need to get the latest
  // block number
  if (blockNumber == 0 && before) {
    blockNumber = m_sharedMediator.m_txBlockChain.GetLastBlock()
                      .GetHeader()
                      .GetBlockNum();
  }

  try {
    bool wasMore = false;
    const auto res = BlockStorage::GetBlockStorage().GetOtterTxAddressMapping(
        address, blockNumber, pageSize, before, wasMore);

    Json::Value response = Json::objectValue;
    Json::Value txs = Json::arrayValue;
    Json::Value receipts = Json::arrayValue;

    for (const auto &hash : res) {
      // Get Tx result
      auto const txByHash = GetEthTransactionByHash(hash);
      auto txReceipt = GetEthTransactionReceipt(hash);

      // For some reason otterscan expects a timestamp in the receipts...
      auto const block =
          GetEthBlockByNumber(txReceipt["blockNumber"].asString(), false);
      txReceipt["timestamp"] = block["timestamp"];

      txs.append(txByHash);
      receipts.append(txReceipt);
    }

    response["txs"] = txs;
    response["receipts"] = receipts;

    // Otterscan docs:
    // These are the conditions for which these variables are set to true
    response["firstPage"] =
        (before && blockNumberWasZero) || (!before && !wasMore);
    response["lastPage"] =
        (!before && blockNumberWasZero) || (before && !wasMore);

    return response;
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << ". Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::OtterscanGetTransactionBySenderAndNonce(
    const std::string &address, uint64_t nonce) {
  if (!ARCHIVAL_LOOKUP_WITH_TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  if (!TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  try {
    const auto res = BlockStorage::GetBlockStorage().GetOtterAddressNonceLookup(
        address, nonce);

    // Perhaps this should just return empty array
    if (res.empty()) {
      LOG_GENERAL(INFO, "Otterscan addr nonce request failed! ");
      return Json::nullValue;
    }

    return res;
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << ". Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::OtterscanGetInternalOperations(
    const std::string &txHash, const std::string &tracer) {
  if (!ARCHIVAL_LOOKUP_WITH_TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  if (!TX_TRACES) {
    throw JsonRpcException(
        ServerBase::RPC_MISC_ERROR,
        "The node is not configured to store otter internal operations");
  }

  std::string trace;

  try {
    TxnHash tranHash(txHash);

    bool isPresent =
        BlockStorage::GetBlockStorage().GetOtterTrace(tranHash, trace);

    if (!isPresent) {
      LOG_GENERAL(INFO, "Otterscan trace request failed! ");
      return Json::nullValue;
    }

    return extractTracer(tracer, trace);
  } catch (exception &e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << ". Input: " << txHash);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::GetHeaderByNumber(const uint64_t blockNumber) {
  // Erigon headers are a subset of a full block - So just return the full
  // block.
  return EthRpcMethods::GetEthBlockByNumber(std::to_string(blockNumber), false);
}

bool EthRpcMethods::HasCode(const std::string &address,
                            const uint64_t /* blockNumber */) {
  // TODO: Respect block parameter - We can probably do this by finding the
  // contract creation transaction and comparing the block numbers.
  Address addr{address, Address::FromHex};
  unique_lock<shared_timed_mutex> lock(
      AccountStore::GetInstance().GetPrimaryMutex());
  const Account *account = AccountStore::GetInstance().GetAccount(addr, true);
  if (account) {
    return !account->GetCode().empty();
  } else {
    return false;
  }
}

Json::Value EthRpcMethods::GetBlockDetails(const uint64_t blockNumber) {
  Json::Value response;

   auto maybeBlock = m_sharedMediator.m_txBlockChain.MaybeGetBlock(blockNumber);
  if (maybeBlock) {
    auto txBlock = maybeBlock.value();
     bool isVacuous =
        CommonUtils::IsVacuousEpoch(txBlock.GetHeader().GetBlockNum());
     uint128_t rewards =
        (isVacuous ? txBlock.GetHeader().GetRewards() * EVM_ZIL_SCALING_FACTOR
         : 0);
     uint128_t fees =
        (isVacuous ? 0
         : txBlock.GetHeader().GetRewards() * EVM_ZIL_SCALING_FACTOR);
     try {
      auto jsonBlock = GetEthBlockCommon(txBlock, false);

       if (jsonBlock["gasLimit"].asString() == "0x0") jsonBlock["gasLimit"] = "0x1";

      jsonBlock.removeMember("transactions");
      jsonBlock["transactionCount"] = txBlock.GetHeader().GetNumTxs();
      jsonBlock["logsBloom"] = Json::nullValue;
      response["block"] = jsonBlock;

      response["issuance"]["blockReward"] = rewards.str();
      response["issuance"]["uncleReward"] = 0;
      response["issuance"]["issuance"] = rewards.str();

      response["totalFees"] = fees.str();
      return response;
    } catch (std::exception &e) {
      LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumber);
      throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
    }
  } else {
    return Json::nullValue;
  }
}

Json::Value EthRpcMethods::GetBlockTransactions(const uint64_t blockNumber,
                                                const uint32_t pageNumber,
                                                const uint32_t pageSize) {
  Json::Value response;

  auto txBlock = m_sharedMediator.m_txBlockChain.GetBlock(blockNumber);
  auto jsonBlock = GetEthBlockCommon(txBlock, true);

  auto transactions = jsonBlock["transactions"];
  jsonBlock["transactionCount"] = transactions.size();

  auto start = pageNumber * pageSize;
  auto end = std::min(transactions.size(), (pageNumber + 1) * pageSize);

  std::vector<Json::Value> receipts;
  for (Json::Value::ArrayIndex i = start; i < end; i++) {
    auto transaction = transactions[i];
    // TODO: Truncate input to 4 bytes (plus 0x) - Work out why the 0x is
    // optional

    auto receipt =
        EthRpcMethods::GetEthTransactionReceipt(transaction["hash"].asString());
    receipt["logs"] = Json::nullValue;
    receipt["logsBloom"] = Json::nullValue;
    receipts.push_back(receipt);
  }

  response["fullblock"] = jsonBlock;

  for (auto r : receipts) {
    response["receipts"].append(r);
  }

  return response;
}

Json::Value EthRpcMethods::GetContractCreator(const std::string &address) {
  Address addr{address, Address::FromHex};

  dev::h256 creationTxn =
      BlockStorage::GetBlockStorage().GetContractCreator(addr);

  if (creationTxn == dev::h256()) {
    return Json::nullValue;
  }

  TxBodySharedPtr txnBodyPtr;
  bool isPresent =
      BlockStorage::GetBlockStorage().GetTxBody(creationTxn, txnBodyPtr);
  if (!isPresent) {
    LOG_GENERAL(WARNING, "Contract creator transaction doesn't exist");
    return Json::nullValue;
  }
  const TransactionWithReceipt &txnBody = *txnBodyPtr;

  Json::Value response = Json::objectValue;

  response["hash"] = "0x" + creationTxn.hex();
  // FIXME: This is wrong for deployer contracts.
  // "For deployer contracts, i.e., the contract is created as a result of a
  // method call, this corresponds to the address of the contract who created
  // it."
  response["creator"] = "0x" + txnBody.GetTransaction().GetSenderAddr().hex();

  return response;
}
