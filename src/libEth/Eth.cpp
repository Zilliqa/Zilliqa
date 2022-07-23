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
#include "depends/common/RLP.h"
#include "jsonrpccpp/server.h"
#include "libUtils/DataConversion.h"

using namespace jsonrpc;

Json::Value populateReceiptHelper(std::string const& txnhash) {
  Json::Value ret;

  ret["transactionHash"] = txnhash;
  ret["blockHash"] =
      "0x0000000000000000000000000000000000000000000000000000000000000000";
  ret["blockNumber"] = "0x429d3b";
  ret["contractAddress"] = nullptr;
  ret["cumulativeGasUsed"] = "0x64b559";
  ret["from"] = "0x999";  // todo: fill
  ret["gasUsed"] = "0xcaac";
  ret["logs"].append(Json::Value());
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
  ret["status"] = nullptr;
  ret["to"] = "0x888";                // todo: fill
  ret["transactionIndex"] = "0x777";  // todo: fill

  return ret;
}

Json::Value populateBlockHelper() {
  Json::Value ret;

  ret["difficulty"] = "0x3ff800000";
  ret["extraData"] = "0x476574682f76312e302e302f6c696e75782f676f312e342e32";
  ret["gasLimit"] = "0x1388";
  ret["gasUsed"] = "0x0";
  ret["hash"] =
      "0x88e96d4537bea4d9c05d12549907b32561d3bf31f45aae734cdc119f13406cb6";
  ret["logsBloom"] =
      "0x00000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "0000000000000000000000000000000000000000000000000000000000000000000000"
      "000000000000000000000000";
  ret["miner"] = "0x05a56e2d52c817161883f50c441c3228cfe54d9f";
  ret["mixHash"] =
      "0x969b900de27b6ac6a67742365dd65f55a0526c41fd18e1b16f1a1215c2e66f59";
  ret["nonce"] = "0x539bd4979fef1ec4";
  ret["number"] = "0x1";
  ret["parentHash"] =
      "0xd4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3";
  ret["receiptsRoot"] =
      "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
  ret["sha3Uncles"] =
      "0x1dcc4de8dec75d7aab85b567b6ccd41ad312451b948a7413f0a142fd40d49347";
  ret["size"] = "0x219";
  ret["stateRoot"] =
      "0xd67e4d450343046425ae4271474353857ab860dbc0a1dde64b41b5cd3a532bf3";
  ret["timestamp"] = "0x55ba4224";
  ret["totalDifficulty"] = "0x7ff800000";
  ret["transactions"] = Json::arrayValue;
  ret["transactionsRoot"] =
      "0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421";
  ret["uncles"] = Json::arrayValue;

  return ret;
}

// Given a RLP message, parse out the fields and return a EthFields object
EthFields parseRawTxFields(std::string const& message) {
  EthFields ret;

  bytes asBytes;
  DataConversion::HexStrToUint8Vec(message, asBytes);

  dev::RLP rlpStream1(asBytes);
  int i = 0;
  // todo: checks on size of rlp stream etc.

  ret.version = 65538;

  // RLP TX contains: nonce, gasPrice, gasLimit, to, value, data, v,r,s
  for (auto it = rlpStream1.begin(); it != rlpStream1.end();) {
    auto byteIt = (*it).operator bytes();

    switch (i) {
      case 0:
        ret.nonce = uint32_t(*it);
        break;
      case 1:
        ret.gasPrice = uint128_t(*it);
        break;
      case 2:
        ret.gasLimit = uint64_t(*it);
        break;
      case 3:
        ret.toAddr = byteIt;
        break;
      case 4:
        ret.amount = uint128_t(*it);
        break;
      case 5:
        ret.data = byteIt;
        break;
      case 6:  // V - only needed for pub sig recovery
        break;
      case 7:  // R
        ret.signature.insert(ret.signature.end(), byteIt.begin(), byteIt.end());
        break;
      case 8:  // S
        ret.signature.insert(ret.signature.end(), byteIt.begin(), byteIt.end());
        break;
      default:
        LOG_GENERAL(WARNING, "too many fields received in rlp!");
    }

    i++;
    it++;
  }

  return ret;
}
