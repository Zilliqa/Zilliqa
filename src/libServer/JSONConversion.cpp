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

#include <array>
#include <string>
#include <vector>

#include <Schnorr.h>
#include "AddressChecksum.h"
#include "JSONConversion.h"
#include "Server.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

unsigned int JSON_TRAN_OBJECT_SIZE = 10;

const Json::Value JSONConversion::convertMicroBlockInfoArraytoJson(
    const vector<MicroBlockInfo>& v) {
  Json::Value mbInfosJson = Json::arrayValue;
  for (auto const& i : v) {
    Json::Value t_mbInfoJson;
    t_mbInfoJson["MicroBlockHash"] = i.m_microBlockHash.hex();
    t_mbInfoJson["MicroBlockTxnRootHash"] = i.m_txnRootHash.hex();
    t_mbInfoJson["MicroBlockShardId"] = i.m_shardId;
    mbInfosJson.append(t_mbInfoJson);
  }
  return mbInfosJson;
}

const Json::Value JSONConversion::convertTxBlocktoJson(const TxBlock& txblock) {
  Json::Value ret;
  Json::Value ret_head;
  Json::Value ret_body;

  const TxBlockHeader& txheader = txblock.GetHeader();

  ret_head["Version"] = txheader.GetVersion();
  ret_head["GasLimit"] = to_string(txheader.GetGasLimit());
  ret_head["GasUsed"] = to_string(txheader.GetGasUsed());
  ret_head["Rewards"] = txheader.GetRewards().str();
  ret_head["PrevBlockHash"] = txheader.GetPrevHash().hex();
  ret_head["BlockNum"] = to_string(txheader.GetBlockNum());
  ret_head["Timestamp"] = to_string(txblock.GetTimestamp());

  ret_head["MbInfoHash"] = txheader.GetMbInfoHash().hex();
  ret_head["StateRootHash"] = txheader.GetStateRootHash().hex();
  ret_head["StateDeltaHash"] = txheader.GetStateDeltaHash().hex();
  ret_head["NumTxns"] = txheader.GetNumTxs();
  ret_head["NumMicroBlocks"] =
      static_cast<uint32_t>(txblock.GetMicroBlockInfos().size());

  ret_head["MinerPubKey"] = static_cast<string>(txheader.GetMinerPubKey());
  ret_head["DSBlockNum"] = to_string(txheader.GetDSBlockNum());

  std::string HeaderSignStr;
  if (!DataConversion::SerializableToHexStr(txblock.GetCS2(), HeaderSignStr)) {
    return ret;  // empty ret
  }
  ret_body["HeaderSign"] = HeaderSignStr;
  ret_body["BlockHash"] = txblock.GetBlockHash().hex();

  ret_body["MicroBlockInfos"] =
      convertMicroBlockInfoArraytoJson(txblock.GetMicroBlockInfos());

  ret["header"] = ret_head;
  ret["body"] = ret_body;

  return ret;
}

const Json::Value JSONConversion::convertRawTxBlocktoJson(
    const TxBlock& txblock) {
  Json::Value ret;
  bytes raw;
  string rawstr;

  if (!txblock.Serialize(raw, 0)) {
    LOG_GENERAL(WARNING, "Raw TxBlock conversion failed");
    return ret;
  }

  if (!DataConversion::Uint8VecToHexStr(raw, rawstr)) {
    LOG_GENERAL(WARNING, "Raw TxBlock conversion failed");
    return ret;
  }

  ret["data"] = rawstr;
  return ret;
}

const Json::Value JSONConversion::convertDSblocktoJson(const DSBlock& dsblock) {
  Json::Value ret;
  Json::Value ret_header;
  Json::Value ret_sign;

  const DSBlockHeader& dshead = dsblock.GetHeader();
  string retSigstr;
  if (!DataConversion::SerializableToHexStr(dsblock.GetCS2(), retSigstr)) {
    return ret;
  }
  ret_sign = retSigstr;

  ret_header["Difficulty"] = dshead.GetDifficulty();
  ret_header["PrevHash"] = dshead.GetPrevHash().hex();
  ret_header["LeaderPubKey"] = static_cast<string>(dshead.GetLeaderPubKey());
  ret_header["BlockNum"] = to_string(dshead.GetBlockNum());

  ret_header["DifficultyDS"] = dshead.GetDSDifficulty();
  ret_header["GasPrice"] = dshead.GetGasPrice().str();
  ret_header["PoWWinners"] = Json::Value(Json::arrayValue);

  for (const auto& dswinner : dshead.GetDSPoWWinners()) {
    ret_header["PoWWinners"].append(static_cast<string>(dswinner.first));
  }
  ret_header["Timestamp"] = to_string(dsblock.GetTimestamp());
  ret["header"] = ret_header;

  ret["signature"] = ret_sign;

  return ret;
}

const Json::Value JSONConversion::convertRawDSBlocktoJson(
    const DSBlock& dsblock) {
  Json::Value ret;
  bytes raw;
  string rawstr;

  if (!dsblock.Serialize(raw, 0)) {
    LOG_GENERAL(WARNING, "Raw DSBlock conversion failed");
    return ret;
  }

  if (!DataConversion::Uint8VecToHexStr(raw, rawstr)) {
    LOG_GENERAL(WARNING, "Raw DSBlock conversion failed");
    return ret;
  }

  ret["data"] = rawstr;
  return ret;
}

const Transaction JSONConversion::convertJsontoTx(const Json::Value& _json) {
  uint32_t version = _json["version"].asUInt();

  string nonce_str = _json["nonce"].asString();
  uint64_t nonce = strtoull(nonce_str.c_str(), NULL, 0);

  string toAddr_str = _json["toAddr"].asString();
  string lower_case_addr;
  if (!AddressChecksum::VerifyChecksumAddress(toAddr_str, lower_case_addr)) {
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "To Address checksum does not match");
  }
  bytes toAddr_ser;
  if (!DataConversion::HexStrToUint8Vec(lower_case_addr, toAddr_ser)) {
    LOG_GENERAL(WARNING, "json containing invalid hex str for toAddr");
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "Invalid Hex Str for toAddr");
  }

  Address toAddr(toAddr_ser);

  string amount_str = _json["amount"].asString();
  uint128_t amount(amount_str);

  string gasPrice_str = _json["gasPrice"].asString();
  uint128_t gasPrice(gasPrice_str);
  string gasLimit_str = _json["gasLimit"].asString();
  uint64_t gasLimit = strtoull(gasLimit_str.c_str(), NULL, 0);

  string pubKey_str = _json["pubKey"].asString();
  bytes pubKey_ser;
  if (!DataConversion::HexStrToUint8Vec(pubKey_str, pubKey_ser)) {
    LOG_GENERAL(WARNING, "json cointaining invalid hex str for pubkey");
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "Invalid Hex Str for PubKey");
  }
  PubKey pubKey(pubKey_ser, 0);

  string sign_str = _json["signature"].asString();
  bytes sign;
  if (!DataConversion::HexStrToUint8Vec(sign_str, sign)) {
    LOG_GENERAL(WARNING, "json cointaining invalid hex str for sign");
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "Invalid Hex Str for Signature");
  }

  bytes code, data;

  code = DataConversion::StringToCharArray(_json["code"].asString());
  data = DataConversion::StringToCharArray(_json["data"].asString());

  Transaction tx1(version, nonce, toAddr, pubKey, amount, gasPrice, gasLimit,
                  code, data, Signature(sign, 0));
  LOG_GENERAL(INFO, "Tx converted");

  return tx1;
}

bool JSONConversion::checkStringAddress(const std::string& address) {
  return ((address.size() == ACC_ADDR_SIZE * 2 + 2) &&
          (address.substr(0, 2) == "0x"));
}

bool JSONConversion::checkJsonTx(const Json::Value& _json) {
  bool ret = true;

  ret = ret && _json.isObject();
  ret = ret && (_json.size() == JSON_TRAN_OBJECT_SIZE ||
                _json.size() == JSON_TRAN_OBJECT_SIZE + 1);
  if (ret && (_json.size() == JSON_TRAN_OBJECT_SIZE + 1)) {
    ret = ret && _json.isMember("priority");
  }
  ret = ret && _json.isMember("nonce");
  ret = ret && _json.isMember("toAddr");
  ret = ret && _json.isMember("amount");
  ret = ret && _json.isMember("pubKey");
  ret = ret && _json.isMember("signature");
  ret = ret && _json.isMember("version");
  ret = ret && _json.isMember("code");
  ret = ret && _json.isMember("data");

  if (ret) {
    if (!_json["nonce"].isIntegral()) {
      LOG_GENERAL(INFO, "Fault in nonce");
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Nonce is not integral");
    }
    if (_json["amount"].isString()) {
      try {
        uint128_t amount(_json["amount"].asString());
      } catch (exception& e) {
        LOG_GENERAL(INFO, "Fault in amount " << e.what());
        throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                        "Amount invalid string");
      }
    } else {
      LOG_GENERAL(INFO, "Amount not string");
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Amount invalid string");
    }
    if (!_json["version"].isIntegral()) {
      LOG_GENERAL(INFO, "Fault in version");
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Version not integral");
    }
    if (_json["pubKey"].asString().size() != PUB_KEY_SIZE * 2) {
      LOG_GENERAL(INFO,
                  "PubKey size wrong " << _json["pubKey"].asString().size());
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Invalid PubKey Size");
    }
    if (_json["signature"].asString().size() != TRAN_SIG_SIZE * 2) {
      LOG_GENERAL(INFO, "signature size wrong "
                            << _json["signature"].asString().size());
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Invalid Signature size");
    }
    string lower_case_addr;
    if (!AddressChecksum::VerifyChecksumAddress(_json["toAddr"].asString(),
                                                lower_case_addr)) {
      LOG_GENERAL(INFO,
                  "To Address checksum wrong " << _json["toAddr"].asString());
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "To Addr checksum wrong");
    }
    if ((_json.size() == JSON_TRAN_OBJECT_SIZE + 1) &&
        !_json["priority"].isBool()) {
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Priority should be boolean");
    }
  } else {
    LOG_GENERAL(INFO, "Json Data Object has missing components");
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "Missing components in Json Data Object");
  }

  return ret;
}

const vector<string> JSONConversion::convertJsonArrayToVector(
    const Json::Value& _json) {
  if (!_json.isArray()) {
    throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                    "Expected Array type");
  }

  vector<string> vec;

  for (const auto& ele : _json) {
    ostringstream streamObj;
    if (!ele.isString()) {
      throw jsonrpc::JsonRpcException(Server::RPC_INVALID_PARAMETER,
                                      "Every array value should be a string");
    }
    streamObj << quoted(ele.asString());
    vec.emplace_back(streamObj.str());
  }
  return vec;
}

const Json::Value JSONConversion::convertTxtoJson(
    const TransactionWithReceipt& twr, bool isSoftConfirmed) {
  Json::Value _json;

  _json["ID"] = twr.GetTransaction().GetTranID().hex();
  _json["version"] = to_string(twr.GetTransaction().GetVersion());
  _json["nonce"] = to_string(twr.GetTransaction().GetNonce());
  _json["toAddr"] = twr.GetTransaction().GetToAddr().hex();
  _json["senderPubKey"] =
      static_cast<string>(twr.GetTransaction().GetSenderPubKey());
  _json["amount"] = twr.GetTransaction().GetAmount().str();
  _json["signature"] = static_cast<string>(twr.GetTransaction().GetSignature());
  _json["receipt"] = twr.GetTransactionReceipt().GetJsonValue();
  _json["gasPrice"] = twr.GetTransaction().GetGasPrice().str();
  _json["gasLimit"] = to_string(twr.GetTransaction().GetGasLimit());

  if (!twr.GetTransaction().GetCode().empty()) {
    _json["code"] =
        DataConversion::CharArrayToString(twr.GetTransaction().GetCode());
  }
  if (!twr.GetTransaction().GetData().empty()) {
    _json["data"] =
        DataConversion::CharArrayToString(twr.GetTransaction().GetData());
  }

  if (isSoftConfirmed) {
    _json["softconfirm"] = true;
  }

  return _json;
}

const Json::Value JSONConversion::convertNode(const PairOfNode& node) {
  Json::Value _json;
  _json["PubKey"] = static_cast<string>(node.first);
  _json["NetworkInfo"] = static_cast<string>(node.second);

  return _json;
}

const Json::Value JSONConversion::convertNode(
    const std::tuple<PubKey, Peer, uint16_t>& node) {
  Json::Value _json;

  _json["PubKey"] = static_cast<string>(get<SHARD_NODE_PUBKEY>(node));
  _json["NetworkInfo"] = static_cast<string>(get<SHARD_NODE_PEER>(node));
  return _json;
}

const Json::Value JSONConversion::convertDequeOfNode(const DequeOfNode& nodes) {
  Json::Value _json = Json::arrayValue;

  for (const auto& node : nodes) {
    Json::Value temp = convertNode(node);
    _json.append(temp);
  }
  return _json;
}
