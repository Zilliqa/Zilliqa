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
#include <Schnorr.h>
#include <jsonrpccpp/common/exception.h>
#include <boost/format.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <ethash/keccak.hpp>
#include <stdexcept>
#include "JSONConversion.h"
#include "LookupServer.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "json/value.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libEth/Eth.h"
#include "libEth/Filters.h"
#include "libMessage/Messenger.h"
#include "libNetwork/Blacklist.h"
#include "libNetwork/Guard.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Peer.h"
#include "libPOW/pow.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libRemoteStorageDB/RemoteStorageDB.h"
#include "libServer/AddressChecksum.h"
#include "libUtils/AddressConversion.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/GasConv.h"
#include "libUtils/JsonUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"
#include "libUtils/TimeUtils.h"

// These two violate our own standards.
using namespace jsonrpc;
using namespace std;

namespace {

bool isNumber(const std::string& str) {
  char* endp;
  strtoull(str.c_str(), &endp, 0);
  return (str.size() > 0 && endp != nullptr && *endp == '\0');
}

bool isSupportedTag(const std::string& tag) {
  return tag == "latest" || tag == "earliest" || tag == "pending" ||
         isNumber(tag);
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

struct EthRpcMethods::ApiKeys {
  std::string from;
  std::string to;
  std::string value;
  std::string gas;
  std::string data;
};

void EthRpcMethods::Init(LookupServer* lookupServer) {
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
      jsonrpc::Procedure("eth_blockNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthBlockNumberI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBalance", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthBalanceI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getBlockByNumber", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_BOOLEAN, NULL),
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
                         "param01", jsonrpc::JSON_STRING, NULL),
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
                         "param01", jsonrpc::JSON_STRING, "param02",
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthTransactionByBlockNumberAndIndexI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_gasPrice", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthGasPriceI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getCode", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
      &EthRpcMethods::GetEthCodeI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_estimateGas", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_OBJECT,
                         NULL),
      &EthRpcMethods::GetEthEstimateGasI);

  m_lookupServer->bindAndAddExternalMethod(
      jsonrpc::Procedure("eth_getTransactionCount", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, "param01", jsonrpc::JSON_STRING,
                         "param02", jsonrpc::JSON_STRING, NULL),
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
                         "param01", jsonrpc::JSON_STRING, nullptr),
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
                         NULL),
      &EthRpcMethods::GetEthBlockReceiptsI);
}

std::string EthRpcMethods::CreateTransactionEth(
    Eth::EthFields const& fields, zbytes const& pubKey,
    const unsigned int num_shards, const uint128_t& gasPrice,
    const CreateTransactionTargetFunc& targetFunc) {
  LOG_MARKER();

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  if (Mediator::m_disableTxns) {
    LOG_GENERAL(INFO, "Txns disabled - rejecting new txn");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }

  Address toAddr{fields.toAddr};
  zbytes data;
  zbytes code;
  if (IsNullAddress(toAddr)) {
    code = ToEVM(fields.code);
  } else {
    data = DataConversion::StringToCharArray(
        DataConversion::Uint8VecToHexStrRet(fields.code));
  }
  Transaction tx{fields.version,
                 fields.nonce,
                 Address(fields.toAddr),
                 PubKey(pubKey, 0),
                 fields.amount,
                 fields.gasPrice,
                 fields.gasLimit,
                 code,  // either empty or stripped EVM-less code
                 data,  // either empty or un-hexed byte-stream
                 Signature(fields.signature, 0)};

  const std::string ret = DataConversion::AddOXPrefix(tx.GetTranID().hex());

  try {
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

      if (!Eth::ValidateEthTxn(tx, fromAddr, sender, gasPrice)) {
        LOG_GENERAL(WARNING, "failed to validate TX!");
        return ret;
      }

      toAccountExist = (toAccount != nullptr);
      toAccountIsContract = toAccountExist && toAccount->isContract();
    }

    const unsigned int shard = Transaction::GetShardIndex(fromAddr, num_shards);
    unsigned int mapIndex = shard;
    bool priority = false;
    switch (Transaction::GetTransactionType(tx)) {
      case Transaction::ContractType::NON_CONTRACT:
        if (ARCHIVAL_LOOKUP) {
          mapIndex = SEND_TYPE::ARCHIVAL_SEND_SHARD;
        }
        if (toAccountExist) {
          if (toAccountIsContract) {
            throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                                   "Contract account won't accept normal txn");
            return ret;
          }
        }

        break;
      case Transaction::ContractType::CONTRACT_CREATION: {
        auto check =
            CheckContractTxnShards(priority, shard, tx, num_shards,
                                   toAccountExist, toAccountIsContract);
        mapIndex = check.second;
      } break;
      case Transaction::ContractType::CONTRACT_CALL: {
        auto check =
            CheckContractTxnShards(priority, shard, tx, num_shards,
                                   toAccountExist, toAccountIsContract);
        mapIndex = check.second;
      } break;
      case Transaction::ContractType::ERROR:
        throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                               "Code is empty and To addr is null");
        break;
      default:
        throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                               "Txn type unexpected");
    }
    if (m_sharedMediator.m_lookup->m_sendAllToDS) {
      if (ARCHIVAL_LOOKUP) {
        mapIndex = SEND_TYPE::ARCHIVAL_SEND_DS;
      } else {
        mapIndex = num_shards;
      }
    }
    if (!targetFunc(tx, mapIndex)) {
      throw JsonRpcException(ServerBase::RPC_DATABASE_ERROR,
                             "Txn could not be added as database exceeded "
                             "limit or the txn was already present");
    }
  } catch (const JsonRpcException& je) {
    LOG_GENERAL(INFO, "[Error]" << je.what() << " Input: N/A");
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: N/A");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
  return ret;
}

std::pair<std::string, unsigned int> EthRpcMethods::CheckContractTxnShards(
    bool priority, unsigned int shard, const Transaction& tx,
    unsigned int num_shards, bool toAccountExist, bool toAccountIsContract) {
  unsigned int mapIndex = shard;
  std::string resultStr;

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

  Address affectedAddress =
      (Transaction::GetTransactionType(tx) == Transaction::CONTRACT_CREATION)
          ? Account::GetAddressForContract(tx.GetSenderAddr(), tx.GetNonce(),
                                           tx.GetVersionIdentifier())
          : tx.GetToAddr();

  unsigned int to_shard =
      Transaction::GetShardIndex(affectedAddress, num_shards);
  // Use m_sendSCCallsToDS as initial setting
  bool sendToDs = priority || m_sharedMediator.m_lookup->m_sendSCCallsToDS;
  if ((to_shard == shard) && !sendToDs) {
    if (tx.GetGasLimitZil() > SHARD_MICROBLOCK_GAS_LIMIT) {
      throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                             "txn gas limit exceeding shard maximum limit");
    }
    if (ARCHIVAL_LOOKUP) {
      mapIndex = SEND_TYPE::ARCHIVAL_SEND_SHARD;
    }
    resultStr =
        "Contract Creation/Call Txn, Shards Match of the sender "
        "and receiver";
  } else {
    if (tx.GetGasLimitZil() > DS_MICROBLOCK_GAS_LIMIT) {
      throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                             "txn gas limit exceeding ds maximum limit");
    }
    if (ARCHIVAL_LOOKUP) {
      mapIndex = SEND_TYPE::ARCHIVAL_SEND_DS;
    } else {
      mapIndex = num_shards;
    }
    resultStr = "Contract Creation/Call Txn, Sent To Ds";
  }
  return make_pair(resultStr, mapIndex);
}

Json::Value EthRpcMethods::GetBalanceAndNonce(const string& address) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
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
      throw JsonRpcException(ServerBase::RPC_INVALID_ADDRESS_OR_KEY,
                             "Account is not created");
    }

    return ret;
  } catch (const JsonRpcException& je) {
    LOG_GENERAL(INFO, "[Error] getting balance" << je.GetMessage());
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

string EthRpcMethods::GetEthCallZil(const Json::Value& _json) {
  return this->GetEthCallImpl(
      _json, {"fromAddr", "toAddr", "amount", "gasLimit", "data"});
}

string EthRpcMethods::GetEthCallEth(const Json::Value& _json,
                                    const string& block_or_tag) {
  if (!isSupportedTag(block_or_tag)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMS,
                           "Unsupported block or tag in eth_call");
  }
  return this->GetEthCallImpl(_json, {"from", "to", "value", "gas", "data"});
}

string EthRpcMethods::GetEthCallImpl(const Json::Value& _json,
                                     const ApiKeys& apiKeys) {
  LOG_MARKER();
  LOG_GENERAL(DEBUG, "GetEthCall:" << _json);
  const auto& addr = JSONConversion::checkJsonGetEthCall(_json, apiKeys.to);
  zbytes code{};
  auto success{false};
  {
    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());
    Account* contractAccount =
        AccountStore::GetInstance().GetAccount(addr, true);
    if (contractAccount == nullptr) {
      throw JsonRpcException(ServerBase::RPC_INVALID_PARAMS,
                             "Account does not exist");
    }
    code = contractAccount->GetCode();
  }

  evmproj::CallResponse response;
  try {
    Address fromAddr;
    if (_json.isMember(apiKeys.from)) {
      fromAddr = Address(_json[apiKeys.from].asString());
    }

    uint64_t amount{0};
    if (_json.isMember(apiKeys.value)) {
      const auto amount_str = _json[apiKeys.value].asString();
      amount = strtoull(amount_str.c_str(), NULL, 0);
    }

    // for now set total gas as twice the ds gas limit
    uint64_t gasRemained =
        GasConv::GasUnitsFromCoreToEth(2 * DS_MICROBLOCK_GAS_LIMIT);
    if (_json.isMember(apiKeys.gas)) {
      const auto gasLimit_str = _json[apiKeys.gas].asString();
      gasRemained =
          min(gasRemained, (uint64_t)stoull(gasLimit_str.c_str(), nullptr, 0));
    }

    string data = _json[apiKeys.data].asString();
    if (data.size() >= 2 && data[0] == '0' && data[1] == 'x') {
      data = data.substr(2);
    }

    const EvmCallParameters params{
        addr.hex(), fromAddr.hex(), DataConversion::CharArrayToString(code),
        data,       gasRemained,    amount};

    if (AccountStore::GetInstance().ViewAccounts(params, response) &&
        response.Success()) {
      success = true;
    }

    if (LOG_SC) {
      LOG_GENERAL(INFO, "Called Evm, response:" << response);
    }

  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "Error: " << e.what());
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to process");
  }

  if (!success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, response.ExitReason());
  }

  return "0x" + response.ReturnedBytes();
}

std::string EthRpcMethods::GetWeb3ClientVersion() {
  LOG_MARKER();
  return "Zilliqa/v8.2";
}

string EthRpcMethods::GetWeb3Sha3(const Json::Value& _json) {
  LOG_MARKER();
  zbytes input = DataConversion::HexStrToUint8VecRet(_json.asString());
  return POW::BlockhashToHexString(
      ethash::keccak256(input.data(), input.size()));
}

Json::Value EthRpcMethods::GetEthUncleCount() {
  LOG_MARKER();
  // There's no concept of longest chain hence there will be no uncles
  // Return 0 instead
  return Json::Value{"0x0"};
}

Json::Value EthRpcMethods::GetEthUncleBlock() {
  LOG_MARKER();
  // There's no concept of longest chain hence there will be no uncles
  // Return null instead
  return Json::nullValue;
}

Json::Value EthRpcMethods::GetEthMining() {
  LOG_MARKER();

  return Json::Value(false);
}

std::string EthRpcMethods::GetEthCoinbase() {
  LOG_MARKER();
  return "0x0000000000000000000000000000000000000000";
}

Json::Value EthRpcMethods::GetNetListening() {
  LOG_MARKER();
  return Json::Value(true);
}

std::string EthRpcMethods::GetNetPeerCount() {
  LOG_MARKER();
  return "0x0";
}

std::string EthRpcMethods::GetProtocolVersion() {
  LOG_MARKER();
  return "0x41";  // Similar to Infura, Alchemy
}

std::string EthRpcMethods::GetEthChainId() {
  LOG_MARKER();
  return (boost::format("0x%x") % ETH_CHAINID).str();
}

Json::Value EthRpcMethods::GetEthSyncing() {
  LOG_MARKER();
  return Json::Value(false);
}

Json::Value EthRpcMethods::GetEmptyResponse() {
  LOG_MARKER();
  const Json::Value expectedResponse = Json::arrayValue;
  return expectedResponse;
}

Json::Value EthRpcMethods::GetEthTransactionByHash(
    const std::string& transactionHash) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }
  try {
    TxBodySharedPtr transactioBodyPtr;
    TxnHash tranHash(transactionHash);
    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxBody(tranHash, transactioBodyPtr);
    if (!isPresent) {
      return Json::nullValue;
    }

    const TxBlock EMPTY_BLOCK;
    const auto txBlock = GetBlockFromTransaction(*transactioBodyPtr);
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
                                              *transactioBodyPtr, txBlock);
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << transactionHash);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }
}

Json::Value EthRpcMethods::GetEthStorageAt(std::string const& address,
                                           std::string const& position,
                                           std::string const& /*blockNum*/) {
  LOG_MARKER();

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
    shared_lock<shared_timed_mutex> lock(
        AccountStore::GetInstance().GetPrimaryMutex());

    const Account* account = AccountStore::GetInstance().GetAccount(addr, true);

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

    auto res = root["_evm_storage"][zeroes];
    zbytes resAsStringBytes;

    for (const auto& item : res.asString()) {
      resAsStringBytes.push_back(item);
    }

    auto const resAsStringHex =
        std::string("0x") +
        DataConversion::Uint8VecToHexStrRet(resAsStringBytes);

    return resAsStringHex;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << address);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthCode(std::string const& address,
                                      std::string const& /*blockNum*/) {
  LOG_MARKER();

  // Strip off "0x" if exists
  auto addressCopy = address;

  if (addressCopy.size() >= 2 && addressCopy[0] == '0' &&
      addressCopy[1] == 'x') {
    addressCopy.erase(0, 2);
  }

  auto code = m_lookupServer->GetSmartContractCode(addressCopy);
  auto codeStr = code["code"].asString();

  // Erase 'EVM' from beginning, put '0x'
  if (codeStr.size() > 3) {
    codeStr[1] = '0';
    codeStr[2] = 'x';
    codeStr.erase(0, 1);
  }

  return codeStr;
}

Json::Value EthRpcMethods::GetEthBlockNumber() {
  Json::Value ret;

  try {
    const auto txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();

    auto const height = txBlock.GetHeader().GetBlockNum() ==
                                std::numeric_limits<uint64_t>::max()
                            ? 1
                            : txBlock.GetHeader().GetBlockNum();

    std::ostringstream returnVal;
    returnVal << "0x" << std::hex << height << std::dec;
    ret = returnVal.str();
  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " When getting block number!");
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }

  return ret;
}

Json::Value EthRpcMethods::GetEthBlockByNumber(
    const std::string& blockNumberStr, const bool includeFullTransactions) {
  try {
    TxBlock txBlock;

    if (!isSupportedTag(blockNumberStr)) {
      return Json::nullValue;
    } else if (blockNumberStr == "latest" ||    //
               blockNumberStr == "earliest" ||  //
               isNumber(blockNumberStr)) {
      // handle latest, earliest and block number requests
      if (blockNumberStr == "latest") {
        txBlock = m_sharedMediator.m_txBlockChain.GetLastBlock();
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
  } catch (const std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr
                                << ", includeFullTransactions: "
                                << includeFullTransactions);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockByHash(
    const std::string& inputHash, const bool includeFullTransactions) {
  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);
    const TxBlock NON_EXISTING_TX_BLOCK{};
    if (txBlock == NON_EXISTING_TX_BLOCK) {
      return Json::nullValue;
    }
    return GetEthBlockCommon(txBlock, includeFullTransactions);

  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash
                                << ", includeFullTransactions: "
                                << includeFullTransactions);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockCommon(
    const TxBlock& txBlock, const bool includeFullTransactions) {
  const auto dsBlock = m_sharedMediator.m_dsBlockChain.GetBlock(
      txBlock.GetHeader().GetDSBlockNum());

  std::vector<TxBodySharedPtr> transactions;
  std::vector<TxnHash> transactionHashes;

  // Gather either transaction hashes or full transactions
  const auto& microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const& mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }

    MicroBlockSharedPtr microBlockPtr;

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto& currTranHashes = microBlockPtr->GetTranHashes();
    if (!includeFullTransactions) {
      transactionHashes.insert(transactionHashes.end(), currTranHashes.begin(),
                               currTranHashes.end());
      continue;
    }
    for (const auto& transactionHash : currTranHashes) {
      TxBodySharedPtr transactionBodyPtr;
      if (!BlockStorage::GetBlockStorage().GetTxBody(transactionHash,
                                                     transactionBodyPtr)) {
        continue;
      }
      transactions.push_back(std::move(transactionBodyPtr));
    }
  }

  return JSONConversion::convertTxBlocktoEthJson(txBlock, dsBlock, transactions,
                                                 transactionHashes,
                                                 includeFullTransactions);
}

Json::Value EthRpcMethods::GetEthBalance(const std::string& address,
                                         const std::string& tag) {
  if (isSupportedTag(tag)) {
    uint256_t ethBalance{0};
    try {
      auto ret = this->GetBalanceAndNonce(address);
      ethBalance.assign(ret["balance"].asString());
    } catch (const JsonRpcException&) {
      // default ethBalance.
    } catch (const std::runtime_error& e) {
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

Json::Value EthRpcMethods::GetEthGasPrice() const {
  try {
    uint256_t gasPrice = m_sharedMediator.m_dsBlockChain.GetLastBlock()
                             .GetHeader()
                             .GetGasPrice();
    // Make gas price in wei
    gasPrice =
        (gasPrice * EVM_ZIL_SCALING_FACTOR) / GasConv::GetScalingFactor();

    // The following ensures we get 'at least' that high price as it was before
    // dividing by GasScalingFactor
    gasPrice += 1000000;
    std::ostringstream strm;

    strm << "0x" << std::hex << gasPrice << std::dec;
    return strm.str();
  } catch (const std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what());

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockTransactionCountByHash(
    const std::string& inputHash) {
  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);

    std::ostringstream strm;
    strm << "0x" << std::hex << txBlock.GetHeader().GetNumTxs() << std::dec;

    return strm.str();

  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthBlockTransactionCountByNumber(
    const std::string& blockNumberStr) {
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

  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionByBlockHashAndIndex(
    const std::string& inputHash, const std::string& indexStr) const {
  try {
    const BlockHash blockHash{inputHash};
    const auto txBlock =
        m_sharedMediator.m_txBlockChain.GetBlockByHash(blockHash);
    const uint64_t index = std::strtoull(indexStr.c_str(), nullptr, 0);
    return GetEthTransactionFromBlockByIndex(txBlock, index);

  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << inputHash);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionByBlockNumberAndIndex(
    const std::string& blockNumberStr, const std::string& indexStr) const {
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
  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << blockNumberStr);

    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable To Process");
  }
}

Json::Value EthRpcMethods::GetEthTransactionFromBlockByIndex(
    const TxBlock& txBlock, uint64_t index) const {
  const TxBlock EMPTY_BLOCK;
  constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
  if (txBlock == EMPTY_BLOCK || index == WRONG_INDEX) {
    return Json::nullValue;
  }
  uint64_t processedIndexes = 0;
  MicroBlockSharedPtr microBlockPtr;
  boost::optional<uint64_t> indexInBlock;

  const auto& microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const& mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }

    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto& currTranHashes = microBlockPtr->GetTranHashes();

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

  TxBodySharedPtr transactioBodyPtr;
  const auto txHashes = microBlockPtr->GetTranHashes();
  if (!BlockStorage::GetBlockStorage().GetTxBody(txHashes[indexInBlock.value()],
                                                 transactioBodyPtr)) {
    return Json::nullValue;
  }

  return JSONConversion::convertTxtoEthJson(indexInBlock.value(),
                                            *transactioBodyPtr, txBlock);
}

Json::Value EthRpcMethods::GetEthTransactionReceipt(
    const std::string& txnhash) {
  try {
    TxnHash argHash{txnhash};
    TxBodySharedPtr transactioBodyPtr;
    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxBody(argHash, transactioBodyPtr);
    if (!isPresent) {
      LOG_GENERAL(WARNING, "Unable to find transaction for given hash");
      return Json::nullValue;
    }

    const TxBlock EMPTY_BLOCK;
    auto txBlock = GetBlockFromTransaction(*transactioBodyPtr);
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
        transactionIndex, *transactioBodyPtr, txBlock);
    auto const zilResult = JSONConversion::convertTxtoJson(*transactioBodyPtr);

    auto receipt = zilResult["receipt"];

    std::string hashId = ethResult["hash"].asString();
    bool success = receipt["success"].asBool();
    std::string sender = ethResult["from"].asString();
    std::string toAddr = ethResult["to"].asString();
    std::string cumGas =
        (boost::format("0x%x") %
         GasConv::GasUnitsFromCoreToEth(
             transactioBodyPtr->GetTransactionReceipt().GetCumGas()))
            .str();

    const TxBlockHeader& txHeader = txBlock.GetHeader();
    const std::string blockNumber =
        (boost::format("0x%x") % txHeader.GetBlockNum()).str();
    const std::string blockHash =
        (boost::format("0x%x") % txBlock.GetBlockHash().hex()).str();

    Json::Value contractAddress =
        ethResult.get("contractAddress", Json::nullValue);

    auto logs =
        Eth::GetLogsFromReceipt(transactioBodyPtr->GetTransactionReceipt());

    const auto baselogIndex =
        Eth::GetBaseLogIndexForReceiptInBlock(argHash, txBlock);

    Eth::DecorateReceiptLogs(logs, txnhash, blockHash, blockNumber,
                             transactionIndex, baselogIndex);
    const auto bloomLogs =
        Eth::GetBloomFromReceiptHex(transactioBodyPtr->GetTransactionReceipt());
    auto res = Eth::populateReceiptHelper(
        hashId, success, sender, toAddr, cumGas, blockHash, blockNumber,
        contractAddress, logs, bloomLogs, transactionIndex,
        transactioBodyPtr->GetTransaction());

    return res;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR,
                           string("Unable To find hash for txn: ") + e.what());
  }

  return Json::nullValue;
}

std::string EthRpcMethods::EthNewFilter(const Json::Value& param) {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewEventFilter(param);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

std::string EthRpcMethods::EthNewBlockFilter() {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewBlockFilter();
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

std::string EthRpcMethods::EthNewPendingTransactionFilter() {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.InstallNewPendingTxnFilter();
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.result);
  }
  return result.result;
}

Json::Value EthRpcMethods::EthGetFilterChanges(const std::string& filter_id) {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetFilterChanges(filter_id);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

bool EthRpcMethods::EthUninstallFilter(const std::string& filter_id) {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  return api.UninstallFilter(filter_id);
}

Json::Value EthRpcMethods::EthGetFilterLogs(const std::string& filter_id) {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetFilterLogs(filter_id);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

Json::Value EthRpcMethods::EthGetLogs(const Json::Value& param) {
  auto& api = m_sharedMediator.m_filtersAPICache->GetFilterAPI();
  auto result = api.GetLogs(param);
  if (!result.success) {
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, result.error);
  }
  return result.result;
}

void EthRpcMethods::EnsureEvmAndLookupEnabled() {
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
    const TransactionWithReceipt& transaction) const {
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
  } catch (std::exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what()
                                << " while getting block number from receipt!");
    return EMPTY_BLOCK;
  }
}

uint64_t EthRpcMethods::GetTransactionIndexFromBlock(
    const TxBlock& txBlock, const std::string& txnhash) const {
  TxnHash argHash{txnhash};
  const TxBlock EMPTY_BLOCK;
  constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
  if (txBlock == EMPTY_BLOCK) {
    return WRONG_INDEX;
  }

  uint64_t transactionIndex = 0;
  MicroBlockSharedPtr microBlockPtr;

  const auto& microBlockInfos = txBlock.GetMicroBlockInfos();
  for (auto const& mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }
    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }
    const auto& tranHashes = microBlockPtr->GetTranHashes();
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
    const std::string& txnRpc) const {
  auto const pubKeyBytes = RecoverECDSAPubKey(txnRpc, ETH_CHAINID);

  auto const asAddr = CreateAddr(pubKeyBytes);

  auto addrChksum = AddressChecksum::GetChecksummedAddressEth(
      DataConversion::Uint8VecToHexStrRet(asAddr.asBytes()));

  return DataConversion::AddOXPrefix(std::move(addrChksum));
}

Json::Value EthRpcMethods::GetEthBlockReceipts(const std::string& blockId) {
  // The easiest way to do this:
  // Get the block + transactions
  // Call TX receipt function

  auto const block = GetEthBlockByHash(blockId, false);
  auto const txs = block["transactions"];

  Json::Value res = Json::arrayValue;

  for (const auto& tx : txs) {
    auto const receipt = GetEthTransactionReceipt(tx.asString());
    res.append(receipt);
  }

  return res;
}

Json::Value EthRpcMethods::DebugTraceTransaction(const std::string& txHash) {
  std::cout << "RECVD trace request for " << txHash << std::endl;

  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(ServerBase::RPC_INVALID_REQUEST,
                           "Sent to a non-lookup");
  }

  try {
    std::string trace;
    TxnHash tranHash(txHash);

    bool isPresent =
        BlockStorage::GetBlockStorage().GetTxTrace(tranHash, trace);

    std::cout << "got trace retrieval: " << trace << std::endl;

    if (!isPresent) {
      return Json::nullValue;
    }


   // const TxBlock EMPTY_BLOCK;
   // const auto txBlock = GetBlockFromTransaction(*transactioBodyPtr);
   // if (txBlock == EMPTY_BLOCK) {
   //   LOG_GENERAL(WARNING, "Unable to get the TX from a minted block!");
   //   return Json::nullValue;
   // }

   // constexpr auto WRONG_INDEX = std::numeric_limits<uint64_t>::max();
   // auto transactionIndex =
   //     GetTransactionIndexFromBlock(txBlock, transactionHash);
   // if (transactionIndex == WRONG_INDEX) {
   //   return Json::nullValue;
   // }

   // return JSONConversion::convertTxtoEthJson(transactionIndex,
                                              //*transactioBodyPtr, txBlock);
  } catch (exception& e) {
    LOG_GENERAL(INFO, "[Error]" << e.what() << " Input: " << txHash);
    throw JsonRpcException(ServerBase::RPC_MISC_ERROR, "Unable to Process");
  }

  return "";
}
