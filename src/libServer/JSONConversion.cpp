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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <string>
#include <vector>

#include "JSONConversion.h"
#include "libCrypto/Schnorr.h"
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

  ret_head["Type"] = txheader.GetType();
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

  ret_body["HeaderSign"] =
      DataConversion::SerializableToHexStr(txblock.GetCS2());

  ret_body["MicroBlockInfos"] =
      convertMicroBlockInfoArraytoJson(txblock.GetMicroBlockInfos());

  ret["header"] = ret_head;
  ret["body"] = ret_body;

  return ret;
}

const Json::Value JSONConversion::convertDSblocktoJson(const DSBlock& dsblock) {
  Json::Value ret;
  Json::Value ret_header;
  Json::Value ret_sign;

  const DSBlockHeader& dshead = dsblock.GetHeader();

  ret_sign = DataConversion::SerializableToHexStr(dsblock.GetCS2());

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

const Transaction JSONConversion::convertJsontoTx(const Json::Value& _json) {
  uint32_t version = _json["version"].asUInt();

  string nonce_str = _json["nonce"].asString();
  uint64_t nonce = strtoull(nonce_str.c_str(), NULL, 0);

  string toAddr_str = _json["toAddr"].asString();
  bytes toAddr_ser = DataConversion::HexStrToUint8Vec(toAddr_str);
  Address toAddr(toAddr_ser);

  string amount_str = _json["amount"].asString();
  uint128_t amount(amount_str);

  string gasPrice_str = _json["gasPrice"].asString();
  uint128_t gasPrice(gasPrice_str);
  string gasLimit_str = _json["gasLimit"].asString();
  uint64_t gasLimit = strtoull(gasLimit_str.c_str(), NULL, 0);

  string pubKey_str = _json["pubKey"].asString();
  bytes pubKey_ser = DataConversion::HexStrToUint8Vec(pubKey_str);
  PubKey pubKey(pubKey_ser, 0);

  string sign_str = _json["signature"].asString();
  bytes sign = DataConversion::HexStrToUint8Vec(sign_str);

  bytes code, data;

  code = DataConversion::StringToCharArray(_json["code"].asString());
  data = DataConversion::StringToCharArray(_json["data"].asString());

  Transaction tx1(version, nonce, toAddr, pubKey, amount, gasPrice, gasLimit,
                  code, data, Signature(sign, 0));
  LOG_GENERAL(INFO, "Tx converted");

  return tx1;
}

bool JSONConversion::checkJsonTx(const Json::Value& _json) {
  bool ret = true;

  ret = ret && _json.isObject();
  ret = ret && (_json.size() == JSON_TRAN_OBJECT_SIZE);
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
      return false;
    }
    if (_json["amount"].isString()) {
      try {
        uint128_t amount(_json["amount"].asString());
      } catch (exception& e) {
        LOG_GENERAL(INFO, "Fault in amount " << e.what());
        return false;
      }
    } else {
      LOG_GENERAL(INFO, "Amount not string");
      return false;
    }
    if (!_json["version"].isIntegral()) {
      LOG_GENERAL(INFO, "Fault in version");
      return false;
    }
    if (_json["pubKey"].asString().size() != PUB_KEY_SIZE * 2) {
      LOG_GENERAL(INFO,
                  "PubKey size wrong " << _json["pubKey"].asString().size());
      return false;
    }
    if (_json["signature"].asString().size() != TRAN_SIG_SIZE * 2) {
      LOG_GENERAL(INFO, "signature size wrong "
                            << _json["signature"].asString().size());
      return false;
    }
    if (_json["toAddr"].asString().size() != ACC_ADDR_SIZE * 2) {
      LOG_GENERAL(
          INFO, "To Address size wrong " << _json["toAddr"].asString().size());
      return false;
    }
  } else {
    LOG_GENERAL(INFO, "Json Data Object has missing components");
  }

  return ret;
}

const Json::Value JSONConversion::convertTxtoJson(
    const TransactionWithReceipt& twr) {
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

  return _json;
}
