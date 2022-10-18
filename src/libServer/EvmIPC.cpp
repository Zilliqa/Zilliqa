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

#include "EvmIPC.h"
#include <boost/format.hpp>
#include "libPersistence/BlockStorage.h"
#include "libUtils/GasConv.h"
#include "libUtils/Logger.h"

/// Duplicated method, move to utils and share with EthRpcMethod.cpp
enum class TagType : uint8_t {
  LATEST_TAG,
  PENDING_TAG,
  EARLIEST_TAG,
  BLOCK_NUMBER_TAG,
  INVALID_TAG
};

static bool isNumber(const std::string& str) {
  char* endp;
  strtoull(str.c_str(), &endp, 0);
  return (str.size() > 0 && endp != nullptr && *endp == '\0');
}

static bool isSupportedTag(const std::string& tag) {
  return tag == "latest" || tag == "earliest" || tag == "pending" ||
         isNumber(tag);
}

static TagType supportedTag(const std::string& tag) {
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

static std::map<std::string, QueryId> _queryMap{
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

static bool latestTagBlockChainInfo(const QueryId queryId, std::string& value) {
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
                                      std::string& value) {
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
      const TxBlockSharedPtr txBlockSharedPtr = GetFirstTxBlock();
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
                                     std::string& value) {
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
                                        std::string& value) {
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

bool EvmIPC::fetchExternalStateValueEvm(const std::string& addr,  //
                                        const std::string&,       // query,
                                        std::string&,             // value,
                                        bool&,                    // found,
                                        std::string&              // type
) {
  LOG_GENERAL(DEBUG, "fetchExternalStateValueEvm, Contract address:" << addr);
  AccountStore::GetInstance().PrintTrie();

  const auto txBlockSharedPtr = GetFirstTxBlock();
  if (txBlockSharedPtr) {
    const std::vector<MicroBlockInfo>& microBlockInfos =
        txBlockSharedPtr->GetMicroBlockInfos();
    for (const auto& microBlockInfo : microBlockInfos) {
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

  return true;
}

bool EvmIPC::fetchBlockchainInfoEvm(const std::string& queryName,
                                    const std::string& blockTag,
                                    std::string& value) {
  const auto queryMapIter = _queryMap.find(queryName);
  if (queryMapIter == _queryMap.end()) {
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
    case TagType::PENDING_TAG: {
      pendingTagBlockChainInfo(queryMapIter->second, value);
      break;
    };
    case TagType::BLOCK_NUMBER_TAG: {
      const uint64_t blockNumber = boost::lexical_cast<uint64_t>(blockTag);
      blockChainInfoByBlockNumber(queryMapIter->second, blockNumber, value);
      break;
    };
    case TagType::INVALID_TAG: {
      return false;
    }
  }

  return true;
}
