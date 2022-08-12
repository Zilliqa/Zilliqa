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

Json::Value populateReceiptHelper(std::string const& txnhash, bool success, const std::string &from, const std::string &to, const std::string &gasUsed, const std::string &blockHash) {
  Json::Value ret;

  ret["transactionHash"] = txnhash;
  ret["blockHash"] = blockHash;
  ret["blockNumber"] = "0x429d3b";
  ret["contractAddress"] = "";
  ret["cumulativeGasUsed"] = gasUsed; // todo: figure this out
  ret["from"] = from;
  ret["gasUsed"] = gasUsed;
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
  ret["status"] = success;
  ret["to"] = to;                // todo: fill
  ret["transactionIndex"] = "0x0";  // todo: fill

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
        ret.code = byteIt;
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


//"result": {
//     transactionHash: '0xb903239f8543d04b5dc1ba6579132b143087c68db1b2168786408fcbce568238',
//     transactionIndex:  '0x1', // 1
//     blockNumber: '0xb', // 11
//     blockHash: '0xc6ef2fc5426d6ad6fd9e2a26abeab0aa2411b7ab17f30a99d3cb96aed1d1055b',
//     cumulativeGasUsed: '0x33bc', // 13244
//     gasUsed: '0x4dc', // 1244
//     contractAddress: '0xb60e8dd61c5d32be8058bb8eb970870f07233155', // or null, if none was created
//     logs: [{
//         // logs as returned by getFilterLogs, etc.
//     }, ...],
//     logsBloom: "0x00...0", // 256 byte bloom filter
//     status: '0x1'
//  }