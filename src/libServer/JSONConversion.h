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

#ifndef ZILLIQA_SRC_LIBSERVER_JSONCONVERSION_H_
#define ZILLIQA_SRC_LIBSERVER_JSONCONVERSION_H_

#include <json/json.h>
#include <array>
#include <vector>

#include "libBlockchain/Block.h"
#include "libBlockchain/BlockHashSet.h"
#include "libData/AccountData/TransactionReceipt.h"

class JSONConversion {
  using TxBodySharedPtr = std::shared_ptr<TransactionWithReceipt>;

 public:
  // converts a uint32_t array to JSON array containing shard ids
  static const Json::Value convertMicroBlockInfoArraytoJson(
      const std::vector<MicroBlockInfo>& v);
  // convert a boolean vector to a json boolean vector
  static const Json::Value convertBooleanVectorToJson(
      const std::vector<bool>& B);
  // converts a TxBlock to JSON object
  static const Json::Value convertTxBlocktoJson(const TxBlock& txblock,
                                                bool verbose = false);
  // converts a TxBlock to JSON object (Eth style)
  static const Json::Value convertTxBlocktoEthJson(
      const TxBlock& txblock, const DSBlock& dsBlock,
      const std::vector<TxBodySharedPtr>& transactions,
      bool includeFullTransactions = false);
  // converts raw TxBlock to JSON object (for staking)
  static const Json::Value convertRawTxBlocktoJson(const TxBlock& txblock);
  // converts a DSBlocck to JSON object
  static const Json::Value convertDSblocktoJson(const DSBlock& dsblock,
                                                bool verbose = false);
  // converts raw DSBlock to JSON object (for staking)
  static const Json::Value convertRawDSBlocktoJson(const DSBlock& dsblock);
  // converts a JSON to Tx
  static const Transaction convertJsontoTx(const Json::Value& _json);
  // check if a Json is a valid Tx
  static bool checkJsonTx(const Json::Value& _json);

  static Address checkJsonGetEthCall(const Json::Value& _json,
                                     const std::string& toKey);
  // check is string address is a valid address
  static bool checkStringAddress(const std::string& address);
  // Convert a json array of strings to a vector of strings
  static const std::vector<std::string> convertJsonArrayToVector(
      const Json::Value& _json);
  // Convert a Tx to JSON object
  static const Json::Value convertTxtoJson(const TransactionWithReceipt& twr,
                                           bool isSoftConfirmed = false);
  // Convert Tx (without reciept) to JSON object
  static const Json::Value convertTxtoJson(
      const Transaction& txn, bool useHexEncodingForCodeData = false);
  // Convert Tx to ETH-like JSON Object
  static const Json::Value convertTxtoEthJson(uint64_t txindex,
                                              const TransactionWithReceipt& txn,
                                              const TxBlock& txblock);
  static Json::Value convertPendingTxtoEthJson(const Transaction& txn);

  static const Json::Value convertAccessList(const AccessList& accessList);
  // Convert a node to json
  static const Json::Value convertNode(const PairOfNode& node);
  // conver a node with reputation to json
  static const Json::Value convertNode(
      const std::tuple<PubKey, Peer, uint16_t>& node);
  // Convert Deque of Node to Json
  static const Json::Value convertDequeOfNode(const DequeOfNode& nodes);
  // Convert Json to keys for getting merkle proof
  static const std::vector<std::pair<std::string, std::vector<std::string>>>
  convertJsonArrayToKeys(const Json::Value& _json);
  // Convert Software Info to Json
  static const Json::Value convertSWInfotoJson(const SWInfo& swInfo);
};

#endif  // ZILLIQA_SRC_LIBSERVER_JSONCONVERSION_H_
