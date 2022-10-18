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

#include "ScillaIPCServer.h"
#include <jsonrpccpp/server/connectors/unixdomainsocketserver.h>
#include <boost/format.hpp>
#include <sstream>
#include "common/Constants.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#include "libUtils/DataConversion.h"
#include "libUtils/GasConv.h"
#include "websocketpp/base64/base64.hpp"

using namespace std;
using namespace Contract;
using namespace jsonrpc;

using websocketpp::base64_decode;
using websocketpp::base64_encode;

/// Duplicated method, move to utils and share with EthRpcMethod.cpp
enum class TagType : uint8_t {
  LATEST_TAG,
  PENDING_TAG,
  EARLIEST_TAG,
  BLOCK_NUMBER_TAG,
  INVALID_TAG
};

static bool isNumber(const std::string &str) {
  char *endp;
  strtoull(str.c_str(), &endp, 0);
  return (str.size() > 0 && endp != nullptr && *endp == '\0');
}

static bool isSupportedTag(const std::string &tag) {
  return tag == "latest" || tag == "earliest" || tag == "pending" ||
         isNumber(tag);
}

static TagType supportedTag(const std::string &tag) {
  if (tag == "latest") {
    return TagType::LATEST_TAG;
  }

  if (tag == "earliest") {
    return TagType::EARLIEST_TAG;
  }

  if (tag == "pending") {
    return TagType::PENDING_TAG;
  }

  if (isNumber(tag)) {
    return TagType::BLOCK_NUMBER_TAG;
  }
  return TagType::INVALID_TAG;
}

///
enum class QueryId : uint8_t {
  BLOCKNUMBER,
  BLOCKHASH,
  BLOCKCOINBASE,
  BLOCKTIMESTAMP,
  BLOCKDIFFICULTY,
  BLOCKGASLIMIT,
  BLOCKGASPRICE
};

static std::map<std::string, QueryId> queryMap{
    {"BLOCKNUMBER", QueryId::BLOCKNUMBER},
    {"BLOCKHASH", QueryId::BLOCKHASH},
    {"BLOCKCOINBASE", QueryId::BLOCKCOINBASE},
    {"BLOCKTIMESTAMP", QueryId::BLOCKTIMESTAMP},
    {"BLOCKDIFFICULTY", QueryId::BLOCKDIFFICULTY},
    {"BLOCKGASLIMIT", QueryId::BLOCKGASLIMIT},
    {"BLOCKGASPRICE", QueryId::BLOCKGASPRICE}  //
};

static TxBlockSharedPtr GetLatestTxBlock() {
  TxBlockSharedPtr txBlockSharedPtr;
  if (not BlockStorage::GetBlockStorage().GetLatestTxBlock(txBlockSharedPtr) ||
      not txBlockSharedPtr) {
    LOG_GENERAL(WARNING, "Could not get latest tx block");
  }
  return txBlockSharedPtr;
}

static TxBlockSharedPtr GetFirstTxBlock() {
  TxBlockSharedPtr txBlockSharedPtr;
  if (not BlockStorage::GetBlockStorage().GetFirstTxBlock(txBlockSharedPtr) ||
      not txBlockSharedPtr) {
    LOG_GENERAL(WARNING, "Could not get first tx block");
  }
  return txBlockSharedPtr;
}

static TxBlockSharedPtr GetTxBlockByNumber(const uint64_t blockNumber) {
  TxBlockSharedPtr txBlockSharedPtr;
  if (not BlockStorage::GetBlockStorage().GetTxBlock(blockNumber,
                                                     txBlockSharedPtr) ||
      not txBlockSharedPtr) {
    LOG_GENERAL(WARNING, "Could not get tx block by number " << blockNumber);
  }
  return txBlockSharedPtr;
}

static DSBlockSharedPtr GetLatestDSBlock() {
  DSBlockSharedPtr dsBlockSharedPtr;
  if ((not BlockStorage::GetBlockStorage().GetLatestDSBlock(
          dsBlockSharedPtr)) ||
      (not dsBlockSharedPtr)) {
    LOG_GENERAL(WARNING, "Could not get latest DS block");
  }

  return dsBlockSharedPtr;
}

static DSBlockSharedPtr GetFirstDSBlock() {
  DSBlockSharedPtr dsBlockSharedPtr;
  if ((not BlockStorage::GetBlockStorage().GetFirstDSBlock(dsBlockSharedPtr)) ||
      (not dsBlockSharedPtr)) {
    LOG_GENERAL(WARNING, "Could not get first DS block");
  }
  return dsBlockSharedPtr;
}

static DSBlockSharedPtr GetDSBlockByNumber(const uint64_t blockNumber) {
  DSBlockSharedPtr dsBlockSharedPtr;
  if ((not BlockStorage::GetBlockStorage().GetDSBlock(blockNumber,
                                                      dsBlockSharedPtr)) ||
      (not dsBlockSharedPtr)) {
    LOG_GENERAL(WARNING, "Could not get DS block by number " << blockNumber);
  }
  return dsBlockSharedPtr;
}

static bool latestTagBlockChainInfo(const QueryId queryId, std::string &value) {
  switch (queryId) {
    case QueryId::BLOCKNUMBER: {
      const TxBlockSharedPtr txBlockSharedPtr = GetLatestTxBlock();
      if (txBlockSharedPtr) {
        value = (boost::format("0x%x") %
                 txBlockSharedPtr->GetHeader().GetBlockNum())
                    .str();
        return true;
      }
      break;
    }
    case QueryId::BLOCKHASH: {
      const TxBlockSharedPtr txBlockSharedPtr = GetLatestTxBlock();
      if (txBlockSharedPtr) {
        value = txBlockSharedPtr->GetBlockHash().hex();
        return true;
      }
      break;
    }
    case QueryId::BLOCKCOINBASE: {
      break;
    }
    case QueryId::BLOCKTIMESTAMP: {
      const TxBlockSharedPtr txBlockSharedPtr = GetLatestTxBlock();
      if (txBlockSharedPtr) {
        value = std::to_string(txBlockSharedPtr->GetTimestamp() /
                               1'000'000U);  // in seconds
        return true;
      }
      break;
    }
    case QueryId::BLOCKDIFFICULTY: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetLatestDSBlock();
      if (dsBlockSharedPtr) {
        value = "0x" +
                std::to_string(dsBlockSharedPtr->GetHeader().GetDifficulty());
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASLIMIT: {
      const TxBlockSharedPtr txBlockSharedPtr = GetLatestTxBlock();
      if (txBlockSharedPtr) {
        value = std::to_string(GasConv::GasUnitsFromCoreToEth(
            txBlockSharedPtr->GetHeader().GetGasLimit()));
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASPRICE: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetLatestDSBlock();
      if (dsBlockSharedPtr) {
        const uint256_t gasPrice =
            ((dsBlockSharedPtr->GetHeader().GetGasPrice() *
              EVM_ZIL_SCALING_FACTOR) /
             GasConv::GetScalingFactor()) +
            EVM_ZIL_SCALING_FACTOR;
        value = gasPrice.str();
        return true;
      }
      break;
    }
  }
  return false;
}

static bool earliestTagBlockChainInfo(const QueryId queryId,
                                      std::string &value) {
  switch (queryId) {
    case QueryId::BLOCKNUMBER: {
      const TxBlockSharedPtr txBlockSharedPtr = GetFirstTxBlock();
      if (txBlockSharedPtr) {
        value = (boost::format("0x%x") %
                 txBlockSharedPtr->GetHeader().GetBlockNum())
                    .str();
        return true;
      }
      break;
    }
    case QueryId::BLOCKHASH: {
      break;
    }
    case QueryId::BLOCKCOINBASE: {
      break;
    }
    case QueryId::BLOCKTIMESTAMP: {
      const TxBlockSharedPtr txBlockSharedPtr = GetFirstTxBlock();
      if (txBlockSharedPtr) {
        value = std::to_string(txBlockSharedPtr->GetTimestamp() /
                               1'000'000U);  // in seconds
        return true;
      }
      break;
    }
    case QueryId::BLOCKDIFFICULTY: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetFirstDSBlock();
      if (dsBlockSharedPtr) {
        value = "0x" +
                std::to_string(dsBlockSharedPtr->GetHeader().GetDifficulty());
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASLIMIT: {
      const TxBlockSharedPtr txBlockSharedPtr = GetFirstTxBlock();
      if (txBlockSharedPtr) {
        value = std::to_string(GasConv::GasUnitsFromCoreToEth(
            txBlockSharedPtr->GetHeader().GetGasLimit()));
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASPRICE: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetFirstDSBlock();
      if (dsBlockSharedPtr) {
        const uint256_t gasPrice =
            ((dsBlockSharedPtr->GetHeader().GetGasPrice() *
              EVM_ZIL_SCALING_FACTOR) /
             GasConv::GetScalingFactor()) +
            EVM_ZIL_SCALING_FACTOR;
        value = gasPrice.str();
        return true;
      }
      break;
    }
  }
  return false;
}

static bool pendingTagBlockChainInfo(const QueryId queryId,
                                     std::string &value) {
  switch (queryId) {
    case QueryId::BLOCKNUMBER: {
      value = "";  // not yet supported
      break;
    }
    case QueryId::BLOCKHASH: {
      break;
    }
    case QueryId::BLOCKCOINBASE: {
      break;
    }
    case QueryId::BLOCKTIMESTAMP: {
      break;
    }
    case QueryId::BLOCKDIFFICULTY: {
      break;
    }
    case QueryId::BLOCKGASLIMIT: {
      break;
    }
    case QueryId::BLOCKGASPRICE: {
      break;
    }
  }
  return false;
}

static bool blockChainInfoByBlockNumber(const QueryId queryId,
                                        const uint64_t blockNumber,
                                        std::string &value) {
  switch (queryId) {
    case QueryId::BLOCKNUMBER: {
      const TxBlockSharedPtr txBlockSharedPtr = GetTxBlockByNumber(blockNumber);
      if (txBlockSharedPtr) {
        value = (boost::format("0x%x") %
                 txBlockSharedPtr->GetHeader().GetBlockNum())
                    .str();
        return true;
      }
      break;
    }
    case QueryId::BLOCKHASH: {
      const TxBlockSharedPtr txBlockSharedPtr = GetTxBlockByNumber(blockNumber);
      if (txBlockSharedPtr) {
        value = txBlockSharedPtr->GetBlockHash().hex();
        return true;
      }
      break;
    }
    case QueryId::BLOCKCOINBASE: {
      break;
    }
    case QueryId::BLOCKTIMESTAMP: {
      const TxBlockSharedPtr txBlockSharedPtr = GetTxBlockByNumber(blockNumber);
      if (txBlockSharedPtr) {
        value = std::to_string(txBlockSharedPtr->GetTimestamp() /
                               1'000'000U);  // in seconds
        return true;
      }
      break;
    }
    case QueryId::BLOCKDIFFICULTY: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetDSBlockByNumber(blockNumber);
      if (dsBlockSharedPtr) {
        value = "0x" +
                std::to_string(dsBlockSharedPtr->GetHeader().GetDifficulty());
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASLIMIT: {
      const TxBlockSharedPtr txBlockSharedPtr = GetTxBlockByNumber(blockNumber);
      if (txBlockSharedPtr) {
        value = std::to_string(GasConv::GasUnitsFromCoreToEth(
            txBlockSharedPtr->GetHeader().GetGasLimit()));
        return true;
      }
      break;
    }
    case QueryId::BLOCKGASPRICE: {
      const DSBlockSharedPtr dsBlockSharedPtr = GetDSBlockByNumber(blockNumber);
      if (dsBlockSharedPtr) {
        const uint256_t gasPrice =
            ((dsBlockSharedPtr->GetHeader().GetGasPrice() *
              EVM_ZIL_SCALING_FACTOR) /
             GasConv::GetScalingFactor()) +
            EVM_ZIL_SCALING_FACTOR;
        value = gasPrice.str();
        return true;
      }
    } break;
  }
  return false;
}

ScillaIPCServer::ScillaIPCServer(AbstractServerConnector &conn)
    : AbstractServer<ScillaIPCServer>(conn, JSONRPC_SERVER_V2) {
  // These JSON signatures match that of the actual functions below.
  bindAndAddMethod(Procedure("fetchStateValue", PARAMS_BY_NAME, JSON_OBJECT,
                             "query", JSON_STRING, NULL),
                   &ScillaIPCServer::fetchStateValueI);

  bindAndAddMethod(
      Procedure("fetchExternalStateValue", PARAMS_BY_NAME, JSON_OBJECT, "addr",
                JSON_STRING, "query", JSON_STRING, NULL),
      &ScillaIPCServer::fetchExternalStateValueI);

  bindAndAddMethod(Procedure("updateStateValue", PARAMS_BY_NAME, JSON_STRING,
                             "query", JSON_STRING, "value", JSON_STRING, NULL),
                   &ScillaIPCServer::updateStateValueI);

  bindAndAddMethod(
      Procedure("fetchExternalStateValueB64", PARAMS_BY_NAME, JSON_OBJECT,
                "addr", JSON_STRING, "query", JSON_STRING, NULL),
      &ScillaIPCServer::fetchExternalStateValueB64I);

  bindAndAddMethod(
      Procedure("fetchBlockchainInfo", PARAMS_BY_NAME, JSON_STRING,
                "query_name", JSON_STRING, "query_args", JSON_STRING, NULL),
      &ScillaIPCServer::fetchBlockchainInfoI);
}

void ScillaIPCServer::setBCInfoProvider(const ScillaBCInfo &bcInfo) {
  m_BCInfo = bcInfo;
}

void ScillaIPCServer::fetchStateValueI(const Json::Value &request,
                                       Json::Value &response) {
  std::string value;
  bool found;
  if (!fetchStateValue(request["query"].asString(), value, found)) {
    throw JsonRpcException("Fetching state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(value));
}

void ScillaIPCServer::fetchExternalStateValueI(const Json::Value &request,
                                               Json::Value &response) {
  std::string value, type;
  bool found{false};
  if (!fetchExternalStateValue(request["addr"].asString(),
                               request["query"].asString(), value, found,
                               type)) {
    throw JsonRpcException("Fetching external state value failed");
  }

  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(value));
  response.append(Json::Value(type));
}

void ScillaIPCServer::fetchExternalStateValueB64I(const Json::Value &request,
                                                  Json::Value &response) {
  LOG_GENERAL(DEBUG, "fetchExternalStateValueB64I request:" << request);

  const string query = base64_decode(request["query"].asString());
  std::string value;
  std::string type;
  bool found{false};
#if 0
  if (!fetchExternalStateValueEvm(request["addr"].asString(), query, value,
                                  found, type)) {
    throw JsonRpcException("Fetching external state value failed");
  }
#else
  if (!fetchExternalStateValue(request["addr"].asString(), query, value, found,
                               type)) {
    throw JsonRpcException("Fetching external state value failed");
  }
#endif
  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(found));
  response.append(Json::Value(base64_encode(value)));
  response.append(Json::Value(type));
  LOG_GENERAL(DEBUG, "fetchExternalStateValueB64I response:" << response);
}

void ScillaIPCServer::updateStateValueI(const Json::Value &request,
                                        Json::Value &response) {
  if (not updateStateValue(request["query"].asString(),
                           request["value"].asString())) {
    throw JsonRpcException("Updating state value failed");
  }

  // We have nothing to return. A null response is expected in the client.
  response.clear();
}

void ScillaIPCServer::fetchBlockchainInfoI(const Json::Value &request,
                                           Json::Value &response) {
  LOG_GENERAL(DEBUG, "fetchBlockchainInfoI request:" << request);

  std::string value;
  if (not fetchBlockchainInfoEvm(
          request["query_name"].asString(),  //
          "latest",  // todo get the block tag from the argument via the evm-ds
                     // //request["query_args"].asString(),  //"latest",
          value)) {
    throw JsonRpcException("Fetching blockchain info failed");
  }
  // Prepare the result and finish.
  response.clear();
  response.append(Json::Value(true));
  response.append(Json::Value(value));
  LOG_GENERAL(DEBUG, "fetchBlockchainInfoI response:" << response);
}

bool ScillaIPCServer::fetchStateValue(const string &query, string &value,
                                      bool &found) {
  bytes destination;

  if (!ContractStorage::GetContractStorage().FetchStateValue(
          m_BCInfo.getCurContrAddr(), DataConversion::StringToCharArray(query),
          0, destination, 0, found)) {
    return false;
  }

  string value_new = DataConversion::CharArrayToString(destination);
  value.swap(value_new);
  return true;
}

bool ScillaIPCServer::fetchExternalStateValueEvm(const std::string &addr,  //
                                                 const string &query,      //
                                                 string &value,            //
                                                 bool &found,              //
                                                 string &type) {
#if 0
  LOG_GENERAL(DEBUG, "fetchExternalStateValueEvm, Contract address:" << addr);
  AccountStore::GetInstance().PrintTrie();

  const auto txBlockSharedPtr = GetFirstTxBlock();
  if (txBlockSharedPtr) {
    const std::vector<MicroBlockInfo> &microBlockInfos =
        txBlockSharedPtr->GetMicroBlockInfos();
    for (const auto &microBlockInfo : microBlockInfos) {
      TxBodySharedPtr txBodySharedPtr;
      if (BlockStorage::GetBlockStorage().GetTxBody(
              microBlockInfo.m_txnRootHash, txBodySharedPtr) &&
          txBodySharedPtr) {
        //
        if (txBodySharedPtr->GetTransaction().GetToAddr()) {
          //
        }
      }
    }

    txBlockSharedPtr->GetHeader().GetStateRootHash();
    AccountStore::GetInstance().GetStateRootHash();

    // const Account *account =
    //    AccountStore::GetInstance().GetLatestAccountState();
    // if (account) {
    //  if (account->GetAddress() == Address(addr)) {
    //    LOG_GENERAL(DEBUG, "Found address " << account->GetAddress());
    //  }
    //}
  }
#else
  // std::deque<TxBlockSharedPtr> transactions;
//
// if (not BlockStorage::GetBlockStorage().GetAllTxBlocks(transactions)) {
//  return false;
//}
//
// for (const auto &transaction : transactions) {
//  const auto &microBlockInfos = transaction->GetMicroBlockInfos();
//
//  for (auto const &mbInfo : microBlockInfos) {
//    TxBodySharedPtr txBody;
//    if (BlockStorage::GetBlockStorage().GetTxBody(mbInfo.m_txnRootHash,
//                                                  txBody) &&  //
//        txBody) {
//      LOG_GENERAL(
//          DEBUG, "Transaction toAddr:" << txBody->GetTransaction().GetToAddr()
//                                       << ", Address:" << addr);
//    }
//  }
//}
#endif
  bytes destination;
  if (!ContractStorage::GetContractStorage().FetchExternalStateValue(
          m_BCInfo.getCurContrAddr(),                //
          Address(addr),                             //
          DataConversion::StringToCharArray(query),  //
          0,                                         //
          destination,                               //
          0,                                         //
          found,                                     //
          type)) {
    return false;
  }

  value = DataConversion::CharArrayToString(destination);

  return true;
}

bool ScillaIPCServer::fetchExternalStateValue(const std::string &addr,  //
                                              const string &query,      //
                                              string &value,            //
                                              bool &found,              //
                                              string &type) {
  bytes destination;
  if (!ContractStorage::GetContractStorage().FetchExternalStateValue(
          m_BCInfo.getCurContrAddr(),                //
          Address(addr),                             //
          DataConversion::StringToCharArray(query),  //
          0,                                         //
          destination,                               //
          0,                                         //
          found,                                     //
          type)) {
    return false;
  }

  value = DataConversion::CharArrayToString(destination);

  return true;
}

bool ScillaIPCServer::updateStateValue(const string &query,
                                       const string &value) {
  return ContractStorage::GetContractStorage().UpdateStateValue(
      m_BCInfo.getCurContrAddr(), DataConversion::StringToCharArray(query), 0,
      DataConversion::StringToCharArray(value), 0);
}

bool ScillaIPCServer::fetchBlockchainInfo(const std::string &query_name,
                                          const std::string &query_args,
                                          std::string &value) {
  if (query_name == "BLOCKNUMBER") {
    value = std::to_string(m_BCInfo.getCurBlockNum());
    return true;
  } else if (query_name == "TIMESTAMP") {
    uint64_t blockNum = 0;
    try {
      blockNum = stoull(query_args);
    } catch (...) {
      LOG_GENERAL(WARNING, "Unable to convert to uint64: " << query_args);
      return false;
    }

    TxBlockSharedPtr txBlockSharedPtr;
    if (not BlockStorage::GetBlockStorage().GetTxBlock(blockNum,
                                                       txBlockSharedPtr) ||  //
        not txBlockSharedPtr) {
      LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
      return false;
    }

    value = std::to_string(txBlockSharedPtr->GetTimestamp());
    return true;
  } else if (query_name == "CHAINID") {
    value = std::to_string(CHAIN_ID);
    return true;
  }

  // For queries that include the block number.
  uint64_t blockNum = 0;
  if ((query_name == "BLOCKHASH") ||
      (query_name ==
       "TIMESTAMP")) {  // FIXME: timestamp is never called here, remove
    try {
      blockNum = stoull(query_args);
    } catch (...) {
      LOG_GENERAL(WARNING, "Unable to convert to uint64: " << query_args);
      return false;
    }
  } else {
    blockNum = m_BCInfo.getCurBlockNum();
    if (blockNum > 0) {
      // We need to look at the previous block,
      // as the current block is incomplete at the moment
      // of transaction execution. It is complete at eth_call time,
      // but still look at previous block to keep behavior consistent.
      blockNum -= 1;
    }
  }

  TxBlockSharedPtr txBlockSharedPtr;
  if (query_name == "BLOCKHASH" || /*query_name == "BLOCKCOINBASE" ||*/
      query_name == "BLOCKTIMESTAMP" || query_name == "BLOCKDIFFICULTY" ||
      query_name == "BLOCKGASLIMIT") {
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum,
                                                    txBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum tx block " << blockNum);
      return false;
    }
  }

  // TODO: this will always return the value 0 so far, as we need the real
  // DS block.
  blockNum = m_BCInfo.getCurDSBlockNum();
  DSBlockSharedPtr dsBlockSharedPtr;
  if (/*query_name == "BLOCKCOINBASE" ||*/ query_name == "BLOCKDIFFICULTY" ||
      query_name == "BLOCKGASPRICE") {
    if ((!BlockStorage::GetBlockStorage().GetDSBlock(blockNum,
                                                     dsBlockSharedPtr)) ||
        (not dsBlockSharedPtr)) {
      LOG_GENERAL(WARNING, "Could not get blockNum DS block " << blockNum);
      return false;
    }
  }

  if (not txBlockSharedPtr) {
    LOG_GENERAL(WARNING, "Smart pointers work better when Initialized ");
    return false;
  }

  if (query_name == "BLOCKHASH") {
    value = txBlockSharedPtr->GetBlockHash().hex();
  } else if (query_name == "BLOCKNUMBER") {
    value = std::to_string(blockNum);
  } else if (query_name == "BLOCKTIMESTAMP") {
    value = std::to_string(txBlockSharedPtr->GetTimestamp() /
                           1000000);  // in seconds
  } else if (query_name == "BLOCKDIFFICULTY") {
    value = std::to_string(dsBlockSharedPtr->GetHeader().GetDifficulty());
  } else if (query_name == "BLOCKGASLIMIT") {
    value = std::to_string(GasConv::GasUnitsFromCoreToEth(
        txBlockSharedPtr->GetHeader().GetGasLimit()));
  } else if (query_name == "BLOCKGASPRICE") {
    const uint256_t gasPrice =
        (dsBlockSharedPtr->GetHeader().GetGasPrice() * EVM_ZIL_SCALING_FACTOR) /
            GasConv::GetScalingFactor() +
        EVM_ZIL_SCALING_FACTOR;
    std::ostringstream s;
    s << gasPrice;
    value = s.str();
  } else {
    LOG_GENERAL(WARNING, "Invalid query_name: " << query_name);
    return false;
  }
  return true;
}

bool ScillaIPCServer::fetchBlockchainInfoEvm(const std::string &queryName,
                                             const std::string &blockTag,
                                             std::string &value) {
  const auto queryMapIter = queryMap.find(queryName);
  if (queryMapIter == queryMap.end()) {
    LOG_GENERAL(WARNING, "Unkown query:" << queryName);
    return false;
  }

  if (not isSupportedTag(blockTag)) {
    LOG_GENERAL(WARNING, "Unsupported block tag");
    return false;
  }

  const auto tagType = supportedTag(blockTag);

  switch (tagType) {
    case TagType::LATEST_TAG: {
      return latestTagBlockChainInfo(queryMapIter->second, value);
    };
    case TagType::EARLIEST_TAG: {
      earliestTagBlockChainInfo(queryMapIter->second, value);
      break;
    };
    case TagType::BLOCK_NUMBER_TAG: {
      const uint64_t blockNumber = boost::lexical_cast<uint64_t>(blockTag);
      blockChainInfoByBlockNumber(queryMapIter->second, blockNumber, value);
      break;
    };
    case TagType::PENDING_TAG: {
      pendingTagBlockChainInfo(queryMapIter->second, value);
      break;
    };
    case TagType::INVALID_TAG: {
      return false;
    }
  }

  return true;
}

//  AccountStore::GetInstance().GetAccountTempAtomic(Address(addr));
//
//  if (not account) {
//    account = AccountStore::GetInstance().GetAccount(Address(addr));
//  }
//
//  if (account) {
//    const auto fn =
//        [&account](const TxBlockSharedPtr &txBlockSharedPtr) -> bool  //
//    {
//      const auto storageRoot = account->GetStorageRoot();
//
//      if (txBlockSharedPtr->GetHeader().GetStateRootHash() == storageRoot) {
//        // LOG_GENERAL(DEBUG, "Block number "
//        //                       <<
//        txBlockSharedPtr->GetHeader().GetBlockNum()
//        //                       << " at storage root " << storageRoot);
//
//        TxBodySharedPtr txBody;
//        if (BlockStorage::GetBlockStorage().GetTxBody(
//                txBlockSharedPtr->GetBlockHash(),
//                txBody) &&
//            txBody)  //
//        {
//          LOG_GENERAL(DEBUG,
//                      "To address: " <<
//                      txBody->GetTransaction().GetToAddr());
//        }
//      }
//      return true;
//    };
//
//    BlockStorage::GetBlockStorage().GetTxBlockAll(fn);
//  }