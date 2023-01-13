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

#include "Eth.h"
#include <boost/format.hpp>
#include <boost/range.hpp>
#include <ethash/keccak.hpp>
#include "common/Constants.h"
#include "depends/common/RLP.h"
#include "json/value.h"
#include "jsonrpccpp/server.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountData/Account.h"
#include "libPersistence/BlockStorage.h"
#include "libServer/Server.h"
#include "libUtils/DataConversion.h"
#include "libUtils/SafeMath.h"

using namespace jsonrpc;

const char *ZEROES_HASH =
    "0x0000000000000000000000000000000000000000000000000000000000000";

namespace Eth {

Json::Value populateReceiptHelper(
    std::string const &txnhash, bool success, const std::string &from,
    const std::string &to, const std::string &gasUsed,
    const std::string &blockHash, const std::string &blockNumber,
    const Json::Value &contractAddress, const Json::Value &logs,
    const Json::Value &logsBloom, const Json::Value &transactionIndex,
    const Transaction &tx) {
  Json::Value ret;

  ret["transactionHash"] = txnhash;
  ret["blockHash"] = blockHash;
  ret["blockNumber"] = blockNumber;
  ret["contractAddress"] = contractAddress;
  ret["cumulativeGasUsed"] = gasUsed.empty() ? "0x0" : gasUsed;
  ret["from"] = from;
  ret["gasUsed"] = gasUsed.empty() ? "0x0" : gasUsed;
  ret["logs"] = logs;
  ret["logsBlooooooom"] = logsBloom;
  ret["root"] =
      "0x0000000000000000000000000000000000000000000000000000000000001010";
  ret["status"] = success ? "0x1" : "0x0";
  if (to.empty()) {
    ret["to"] = Json::Value();
  } else {
    ret["to"] = to;
  }
  ret["transactionIndex"] = (boost::format("0x%x") % transactionIndex).str();

  std::string sig{tx.GetSignature()};
  ret["v"] = GetV(tx.GetCoreInfo(), ETH_CHAINID, sig);
  ret["r"] = GetR(sig);
  ret["s"] = GetS(sig);

  return ret;
}

// Given a RLP message, parse out the fields and return a EthFields object
EthFields parseRawTxFields(std::string const &message) {
  EthFields ret;

  zbytes asBytes;
  DataConversion::HexStrToUint8Vec(message, asBytes);

  dev::RLP rlpStream1(asBytes,
                      dev::RLP::FailIfTooBig | dev::RLP::FailIfTooSmall);

  if (rlpStream1.isNull()) {
    LOG_GENERAL(WARNING, "Failed to parse RLP stream in raw TX! " << message);
    return {};
  }

  int i = 0;
  // todo: checks on size of rlp stream etc.

  ret.version = DataConversion::Pack(CHAIN_ID, 2);

  // RLP TX contains: nonce, gasPrice, gasLimit, to, value, data, v,r,s
  for (auto it = rlpStream1.begin(); it != rlpStream1.end();) {
    auto byteIt = (*it).operator zbytes();

    switch (i) {
      case 0:
        ret.nonce = uint32_t(*it);
        break;
      case 1:
        ret.gasPrice = uint128_t{*it};
        break;
      case 2:
        ret.gasLimit = uint64_t{*it};
        break;
      case 3:
        ret.toAddr = byteIt;
        break;
      case 4:
        ret.amount = uint128_t(*it);
        break;
      case 5:
        ret.code = byteIt;
        break;
      case 6:  // V - only needed for pub sig recovery
        break;
      case 7:  // R
      {
        zbytes b = dev::toBigEndian(dev::u256(*it));
        ret.signature.insert(ret.signature.end(), b.begin(), b.end());
      } break;
      case 8:  // S
      {
        zbytes b = dev::toBigEndian(dev::u256(*it));
        ret.signature.insert(ret.signature.end(), b.begin(), b.end());
      } break;
      default:
        LOG_GENERAL(WARNING, "too many fields received in rlp!");
    }

    i++;
    it++;
  }

  // Because of the way Zil handles nonces, we increment the nonce here
  ret.nonce++;

  return ret;
}

bool ValidateEthTxn(const Transaction &tx, const Address &fromAddr,
                    const Account *sender, const uint128_t &gasPriceWei,
                    uint64_t minGasLimit) {
  if (DataConversion::UnpackA(tx.GetVersion()) != CHAIN_ID) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "CHAIN_ID incorrect");
  }

  if (!tx.VersionCorrect()) {
    throw JsonRpcException(
        ServerBase::RPC_VERIFY_REJECTED,
        "Transaction version incorrect! Expected:" +
            std::to_string(TRANSACTION_VERSION) + " Actual:" +
            std::to_string(DataConversion::UnpackB(tx.GetVersion())));
  }

  // While checking the contract size, account for Hex representation
  // with the 'EVM' prefix.
  if (tx.GetCode().size() > 2 * MAX_EVM_CONTRACT_SIZE_BYTES + 3) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "Code size is too large");
  }

  if (tx.GetGasPriceWei() < gasPriceWei) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "GasPrice " +
                               tx.GetGasPriceWei().convert_to<std::string>() +
                               " lower than minimum allowable " +
                               gasPriceWei.convert_to<std::string>());
  }

  if (tx.GetGasLimitEth() < minGasLimit) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "GasLimit " + std::to_string(tx.GetGasLimitEth()) +
                               " lower than minimum allowable " +
                               std::to_string(minGasLimit));
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
                           "The sender of the txn doesn't exist");
  }

  if (sender->GetNonce() >= tx.GetNonce()) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Nonce (" + std::to_string(tx.GetNonce()) +
                               ") lower than current (" +
                               std::to_string(sender->GetNonce()) + ")");
  }

  // Check if transaction amount is valid
  uint256_t gasDepositWei = 0;
  if (!SafeMath<uint256_t>::mul(tx.GetGasLimitZil(), tx.GetGasPriceWei(),
                                gasDepositWei)) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "tx.GetGasLimitZil() * tx.GetGasPrice() overflow!");
  }

  uint256_t debt = 0;
  if (!SafeMath<uint256_t>::add(gasDepositWei, tx.GetAmountWei(), debt)) {
    throw JsonRpcException(
        ServerBase::RPC_INVALID_PARAMETER,
        "tx.GetGasLimit() * tx.GetGasPrice() + tx.GetAmountWei() overflow!");
  }

  const uint256_t accountBalance =
      uint256_t{sender->GetBalance()} * EVM_ZIL_SCALING_FACTOR;
  if (accountBalance < debt) {
    throw JsonRpcException(ServerBase::RPC_INVALID_PARAMETER,
                           "Insufficient funds in source account, wants: " +
                               debt.convert_to<std::string>() + ", but has: " +
                               accountBalance.convert_to<std::string>());
  }

  return true;
}

void DecorateReceiptLogs(Json::Value &logsArrayFromEvm,
                         const std::string &txHash,
                         const std::string &blockHash,
                         const std::string &blockNum,
                         const Json::Value &transactionIndex,
                         uint32_t logIndex) {
  for (auto &logEntry : logsArrayFromEvm) {
    logEntry["removed"] = false;
    logEntry["transactionIndex"] = transactionIndex;
    logEntry["transactionHash"] = txHash;
    logEntry["blockHash"] = blockHash;
    logEntry["blockNumber"] = blockNum;
    logEntry["logIndex"] = (boost::format("0x%x") % logIndex).str();
    ++logIndex;
  }
}

LogBloom GetBloomFromReceipt(const TransactionReceipt &receipt) {
  const auto logs = GetLogsFromReceipt(receipt);
  return BuildBloomForLogs(logs);
}

Json::Value GetBloomFromReceiptHex(const TransactionReceipt &receipt) {
  return std::string{"0x"} + GetBloomFromReceipt(receipt).hex();
}

Json::Value GetLogsFromReceipt(const TransactionReceipt &receipt) {
  const Json::Value logs =
      receipt.GetJsonValue().get("event_logs", Json::arrayValue);
  return logs;
}

LogBloom BuildBloomForLogObject(const Json::Value &logObject) {
  const std::string addressStr =
      logObject.get("address", Json::nullValue).asString();

  if (addressStr.empty()) {
    return {};
  }

  Address address{addressStr};
  const auto topicsArray = logObject.get("topics", Json::arrayValue);

  std::vector<dev::h256> topics;

  for (const auto &topic : topicsArray) {
    topics.push_back(dev::h256{topic.asString()});
  }

  const auto addressHash =
      ethash::keccak256(address.ref().data(), address.ref().size());
  dev::h256 addressBloom{dev::zbytesConstRef{boost::begin(addressHash.bytes),
                                             boost::size(addressHash.bytes)}};

  LogBloom bloom;
  bloom.shiftBloom<3>(addressBloom);

  for (const auto &topic : topics) {
    const auto topicHash =
        ethash::keccak256(topic.ref().data(), topic.ref().size());
    dev::h256 topicBloom{dev::zbytesConstRef{boost::begin(topicHash.bytes),
                                             boost::size(topicHash.bytes)}};
    bloom.shiftBloom<3>(topicBloom);
  }

  return bloom;
}

LogBloom BuildBloomForLogs(const Json::Value &logsArray) {
  LogBloom bloom;
  for (const auto &logEntry : logsArray) {
    const auto single = BuildBloomForLogObject(logEntry);
    bloom |= single;
  }
  return bloom;
}

uint32_t GetBaseLogIndexForReceiptInBlock(const TxnHash &txnHash,
                                          const TxBlock &block) {
  uint32_t logIndex = 0;
  MicroBlockSharedPtr microBlockPtr;

  const auto &microBlockInfos = block.GetMicroBlockInfos();
  for (auto const &mbInfo : microBlockInfos) {
    if (mbInfo.m_txnRootHash == TxnHash{}) {
      continue;
    }
    if (!BlockStorage::GetBlockStorage().GetMicroBlock(mbInfo.m_microBlockHash,
                                                       microBlockPtr)) {
      continue;
    }

    const auto &tranHashes = microBlockPtr->GetTranHashes();
    for (const auto &transactionHash : tranHashes) {
      TxBodySharedPtr transactionBodyPtr;
      if (!BlockStorage::GetBlockStorage().GetTxBody(transactionHash,
                                                     transactionBodyPtr)) {
        continue;
      }

      if (transactionBodyPtr->GetTransaction().GetTranID() == txnHash) {
        return logIndex;
      }

      const auto &receipt = transactionBodyPtr->GetTransactionReceipt();
      const auto currLogs = GetLogsFromReceipt(receipt);

      logIndex += currLogs.size();
    }
  }

  return logIndex;
}

// Common code to both the isolated server and lookup server - that is,
// parse the fields into a TX and get its hash
Transaction GetTxFromFields(Eth::EthFields const &fields, zbytes const &pubKey,
                            std::string &hash) {
  hash = ZEROES_HASH;

  Address toAddr{fields.toAddr};
  zbytes data;
  zbytes code;
  if (IsNullAddress(toAddr)) {
    code = ToEVM(fields.code);
  } else {
    data = fields.code;
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

  hash = DataConversion::AddOXPrefix(tx.GetTranID().hex());

  return tx;
}

}  // namespace Eth
