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

#include "libData/BlockData/Block.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"

class JSONConversion {
 public:
  // converts a uint32_t array to JSON array containing shard ids
  static const Json::Value convertMicroBlockInfoArraytoJson(
      const std::vector<MicroBlockInfo>& v);
  // converts a TxBlock to JSON object
  static const Json::Value convertTxBlocktoJson(const TxBlock& txblock);
  // converts raw TxBlock to JSON object (for staking)
  static const Json::Value convertRawTxBlocktoJson(const TxBlock& txblock);
  // converts a DSBlocck to JSON object
  static const Json::Value convertDSblocktoJson(const DSBlock& dsblock);
  // converts raw DSBlock to JSON object (for staking)
  static const Json::Value convertRawDSBlocktoJson(const DSBlock& dsblock);
  // converts a JSON to Tx
  static const Transaction convertJsontoTx(const Json::Value& _json);
  // check if a Json is a valid Tx
  static bool checkJsonTx(const Json::Value& _json);
  // check is string address is a valid address
  static bool checkStringAddress(const std::string& address);
  // Convert a json array of strings to a vector of strings
  static const std::vector<std::string> convertJsonArrayToVector(
      const Json::Value& _json);
  // Convert a Tx to JSON object
  static const Json::Value convertTxtoJson(const TransactionWithReceipt& twr,
                                           bool isSoftConfirmed = false);
  // Convert a node to json
  static const Json::Value convertNode(const PairOfNode& node);
  // conver a node with reputation to json
  static const Json::Value convertNode(
      const std::tuple<PubKey, Peer, uint16_t>& node);
  // Convert Deque of Node to Json
  static const Json::Value convertDequeOfNode(const DequeOfNode& nodes);
};

#endif  // ZILLIQA_SRC_LIBSERVER_JSONCONVERSION_H_
