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

#ifndef ZILLIQA_SRC_LIBETH_ETH_H_
#define ZILLIQA_SRC_LIBETH_ETH_H_

#include <json/value.h>
#include <cstdint>
#include "common/BaseType.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/TransactionReceipt.h"

class Account;
class Transaction;
class TransactionReceipt;
class TxBlock;
using TxnHash = dev::h256;

namespace Eth {

using LogBloom = dev::h2048;

// https://eips.ethereum.org/EIPS/eip-170
constexpr auto MAX_EVM_CONTRACT_SIZE_BYTES = 24576;

struct EthFields {
  uint32_t version{};
  uint64_t nonce{};
  zbytes toAddr;
  uint128_t amount;
  uint128_t gasPrice;
  uint64_t gasLimit{};
  zbytes code;
  zbytes data;
  zbytes signature;
  AccessList accessList;
  uint128_t maxPriorityFeePerGas;
  uint128_t maxFeePerGas;
  uint32_t signature_validation;
};

Json::Value populateReceiptHelper(
    std::string const &txnhash, bool success, const std::string &from,
    const std::string &to, const std::string &gasUsed,
    const std::string &gasPrice, const std::string &blockHash,
    const std::string &blockNumber, const Json::Value &contractAddress,
    const Json::Value &logs, const Json::Value &logsBloom,
    const Json::Value &transactionIndex, const Transaction &txn);

EthFields parseEip2930Transaction(zbytes const& asBytes);
EthFields parseEip1559Transaction(zbytes const& asBytes);
EthFields parseRawTxFields(std::string const &message);

bool ValidateEthTxn(const Transaction &tx, const Address &fromAddr,
                    const Account *sender, const uint128_t &gasPrice,
                    uint64_t minGasLimit);
void DecorateReceiptLogs(Json::Value &logsArrayFromEvm,
                         const std::string &txHash,
                         const std::string &blockHash,
                         const std::string &blockNum,
                         const Json::Value &transactionIndex,
                         uint32_t logIndex);

Json::Value ConvertScillaEventsToEvm(const Json::Value &evmEvents);
std::string ConvertScillaEventToEthAbi(const std::string &event);

LogBloom GetBloomFromReceipt(const TransactionReceipt &receipt);
Json::Value GetBloomFromReceiptHex(const TransactionReceipt &receipt);

Json::Value GetLogsFromReceipt(const TransactionReceipt &receipt);

LogBloom BuildBloomForLogObject(const Json::Value &logObject);
LogBloom BuildBloomForLogs(const Json::Value &logsArray);
uint32_t GetBaseLogIndexForReceiptInBlock(const TxnHash &txnHash,
                                          const TxBlock &block);

Transaction GetTxFromFields(Eth::EthFields const &fields, zbytes const &pubKey,
                            std::string &hash);

}  // namespace Eth

#endif  // ZILLIQA_SRC_LIBETH_ETH_H_
