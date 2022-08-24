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

Json::Value populateReceiptHelper(std::string const &txnhash, bool success,
                                  const std::string &from,
                                  const std::string &to,
                                  const std::string &gasUsed,
                                  const std::string &blockHash,
                                  const std::string &blockNumber) {
  Json::Value ret;

  ret["transactionHash"] = txnhash;
  ret["blockHash"] = blockHash;
  ret["blockNumber"] = blockNumber;
  ret["contractAddress"] = "0x0000000000000000000000000000000000000000";
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
  ret["to"] = to;                   // todo: fill
  ret["transactionIndex"] = "0x0";  // todo: fill

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

  return ret;
}
