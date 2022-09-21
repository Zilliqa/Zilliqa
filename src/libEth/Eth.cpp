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
#include "common/Constants.h"
#include "depends/common/RLP.h"
#include "json/value.h"
#include "jsonrpccpp/server.h"
#include "libData/AccountData/Transaction.h"
#include "libServer/Server.h"
#include "libUtils/DataConversion.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"

using namespace jsonrpc;

namespace Eth {

Json::Value populateReceiptHelper(std::string const &txnhash, bool success,
                                  const std::string &from,
                                  const std::string &to,
                                  const std::string &gasUsed,
                                  const std::string &blockHash,
                                  const std::string &blockNumber,
                                  const Json::Value &contractAddress) {
  Json::Value ret;

  ret["transactionHash"] = txnhash;
  ret["blockHash"] = blockHash;
  ret["blockNumber"] = blockNumber;
  ret["contractAddress"] = contractAddress;
  ret["cumulativeGasUsed"] = gasUsed.empty() ? "0x0" : gasUsed;
  ret["from"] = from;
  ret["gasUsed"] = gasUsed.empty() ? "0x0" : gasUsed;
  ret["logs"] = Json::arrayValue;
  ret["logsBloom"] =
      "0x0000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000";
  ret["root"] =
      "0x0000000000000000000000000000000000000000000000000000000000001010";
  ret["status"] = success ? "0x1" : "0x0";
  ret["to"] = to;
  ret["transactionIndex"] = "0x0";

  return ret;
}

// Given a RLP message, parse out the fields and return a EthFields object
EthFields parseRawTxFields(std::string const &message) {
  EthFields ret;

  bytes asBytes;
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
    auto byteIt = (*it).operator bytes();

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
        bytes b = dev::toBigEndian(dev::u256(*it));
        ret.signature.insert(ret.signature.end(), b.begin(), b.end());
      } break;
      case 8:  // S
      {
        bytes b = dev::toBigEndian(dev::u256(*it));
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
                    const Account *sender, const uint128_t &gasPriceWei) {
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

  if (tx.GetCode().size() > MAX_EVM_CONTRACT_SIZE_BYTES) {
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

  if (tx.GetGasLimitEth() < MIN_ETH_GAS) {
    throw JsonRpcException(ServerBase::RPC_VERIFY_REJECTED,
                           "GasLimit " + std::to_string(tx.GetGasLimitEth()) +
                               " lower than minimum allowable " +
                               std::to_string(MIN_ETH_GAS));
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

}  // namespace Eth
