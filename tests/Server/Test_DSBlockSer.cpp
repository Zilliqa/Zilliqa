/*
 * Copyright (C) 2021 Zilliqa
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

#include <json/json.h>
#include "libData/BlockData/Block/DSBlock.h"
#include "libData/BlockData/Block/TxBlock.h"
#include "libUtils/IPConverter.h"

#define BOOST_TEST_MODULE json_ds_serialization
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(json_ds_serialization)

vector<bool> convertJsonBitmapToVector(Json::Value _json) {
  vector<bool> ret;

  for (const auto& bool_val : _json) {
    ret.emplace_back(bool_val.asBool());
  }

  return ret;
}

template <class Block>
const Json::Value convertBlockSerializedtoJson(const Block& block) {
  Json::Value ret;
  bytes raw;
  string rawstr;

  if (!block.Serialize(raw, 0)) {
    LOG_GENERAL(WARNING, "Ser conversion failed");
    return ret;
  }

  if (!DataConversion::Uint8VecToHexStr(raw, rawstr)) {
    LOG_GENERAL(WARNING, "Ser conversion failed");
    return ret;
  }

  ret = rawstr;
  return ret;
}

string strip_0x(string hex) {
  if (hex.substr(0, 2) == "0x") {
    hex.erase(0, 2);
  }

  return hex;
}

DSBlock convert_to_dsblock(const Json::Value& dsblock_json) {
  auto& dsblock_header = dsblock_json["header"];
  uint8_t dsDifficulty = dsblock_header["DifficultyDS"].asUInt();
  LOG_GENERAL(INFO,
              "leader pubkey:" << dsblock_header["LeaderPubKey"].asString());
  PubKey dsPubkey = PubKey::GetPubKeyFromString(
      strip_0x(dsblock_header["LeaderPubKey"].asString()));
  uint8_t difficulty = dsblock_header["Difficulty"].asUInt();

  uint64_t blocknum =
      strtoull(dsblock_header["BlockNum"].asString().c_str(), NULL, 0);
  uint64_t epochnum =
      strtoull(dsblock_header["EpochNum"].asString().c_str(), NULL, 0);
  uint128_t gasPrice(dsblock_header["GasPrice"].asString());
  const auto& swinfo_json_zil = dsblock_header["SWInfo"]["Zilliqa"];
  const auto& swinfo_json_scilla = dsblock_header["SWInfo"]["Zilliqa"];
  uint64_t zil_epoch = strtoull(swinfo_json_zil[3].asString().c_str(), NULL, 0);
  uint64_t scilla_epoch =
      strtoull(swinfo_json_scilla[3].asString().c_str(), NULL, 0);
  SWInfo swinfo(swinfo_json_zil[0].asUInt(), swinfo_json_zil[1].asUInt(),
                swinfo_json_zil[2].asUInt(), zil_epoch,
                swinfo_json_zil[4].asUInt(), swinfo_json_scilla[0].asUInt(),
                swinfo_json_scilla[1].asUInt(), swinfo_json_scilla[2].asUInt(),
                scilla_epoch, swinfo_json_scilla[4].asUInt());

  map<PubKey, Peer> powDSWinners;
  uint counter = 0;

  for (auto const& powDSWinner_json : dsblock_header["PoWWinners"]) {
    uint128_t ip;
    IPConverter::ToNumericalIPFromStr(
        dsblock_header["PoWWinnersIP"][counter]["IP"].asString(), ip);
    uint32_t port = dsblock_header["PoWWinnersIP"][counter]["port"].asUInt();
    powDSWinners.emplace(
        PubKey::GetPubKeyFromString(strip_0x(powDSWinner_json.asString())),
        Peer(ip, port));
    counter++;
  }

  vector<PubKey> removeDSNodePubkeys;

  for (auto const& memberEjected : dsblock_header["MembersEjected"]) {
    removeDSNodePubkeys.emplace_back(
        PubKey::GetPubKeyFromString(strip_0x(memberEjected.asString())));
  }

  DSBlockHashSet hashset;
  hashset.m_shardingHash =
      ShardingHash(dsblock_header["ShardingHash"].asString());
  std::fill(begin(hashset.m_reservedField), std::end(hashset.m_reservedField),
            0);

  GovDSShardVotesMap govProposalMap;
  for (auto const& govProposal_json : dsblock_header["Governance"]) {
    uint32_t temp_gov_proposal = govProposal_json["ProposalId"].asUInt();
    auto& govProposal_vote_id = govProposalMap[temp_gov_proposal];
    for (auto const& govProposal_vote_id_ds_json :
         govProposal_json["DSVotes"]) {
      uint32_t voteValue = govProposal_vote_id_ds_json["VoteValue"].asUInt();
      uint32_t voteCount = govProposal_vote_id_ds_json["VoteCount"].asUInt();
      govProposal_vote_id.first.emplace(voteValue, voteCount);
    }
    for (auto const& govProposal_vote_id_shard_json :
         govProposal_json["ShardVotes"]) {
      uint32_t voteValue = govProposal_vote_id_shard_json["VoteValue"].asUInt();
      uint32_t voteCount = govProposal_vote_id_shard_json["VoteCount"].asUInt();
      govProposal_vote_id.second.emplace(voteValue, voteCount);
    }
  }

  uint32_t version = dsblock_header["Version"].asUInt();
  CommitteeHash comHash(dsblock_header["CommitteeHash"].asString());

  uint64_t timestamp =
      strtoull(dsblock_header["Timestamp"].asString().c_str(), NULL, 0);
  BlockHash prev_blockhash(dsblock_header["PrevHash"].asString());

  DSBlockHeader dsblockheader(dsDifficulty, difficulty, dsPubkey, blocknum,
                              epochnum, gasPrice, swinfo, powDSWinners,
                              removeDSNodePubkeys, hashset, govProposalMap,
                              version, comHash, prev_blockhash);

  bytes cs1_ser;
  bytes cs2_ser;
  if (!DataConversion::HexStrToUint8Vec(dsblock_json["CS1"].asString(),
                                        cs1_ser)) {
    LOG_GENERAL(INFO, "Failed HexStrToUint8Vec");
  }
  if (!DataConversion::HexStrToUint8Vec(dsblock_json["signature"].asString(),
                                        cs2_ser)) {
    LOG_GENERAL(INFO, "Failed HexStrToUint8Vec");
  }
  Signature cs1(cs1_ser, 0);
  Signature cs2(cs2_ser, 0);
  vector<bool> b1 = convertJsonBitmapToVector(dsblock_json["B1"]);
  vector<bool> b2 = convertJsonBitmapToVector(dsblock_json["B2"]);

  CoSignatures cosig(cs1, b1, cs2, b2);
  DSBlock dsblock(dsblockheader, CoSignatures());
  dsblock.SetTimestamp(timestamp);
  dsblock.SetCoSignatures(cosig);

  return dsblock;
}

vector<MicroBlockInfo> convert_mbinfos_json_into_cpp_object(
    const Json::Value& mbinfos_json) {
  vector<MicroBlockInfo> mbinfos;

  for (const auto& mbinfo_json : mbinfos_json) {
    MicroBlockInfo mbinfo;
    mbinfo.m_microBlockHash =
        BlockHash(mbinfo_json["MicroBlockHash"].asString());
    mbinfo.m_txnRootHash =
        TxnHash(mbinfo_json["MicroBlockTxnRootHash"].asString());
    mbinfo.m_shardId = mbinfo_json["MicroBlockShardId"].asUInt();
    mbinfos.emplace_back(mbinfo);
  }

  return mbinfos;
}

TxBlock convert_to_txblock(const Json::Value& txblock_json) {
  const auto& txblock_header = txblock_json["header"];
  const auto& txblock_body = txblock_json["body"];
  uint64_t gaslimit = strtoull(txblock_header["GasLimit"].asCString(), NULL, 0);
  uint64_t gasused = strtoull(txblock_header["GasUsed"].asCString(), NULL, 0);
  uint128_t rewards(txblock_header["Rewards"].asString());
  uint128_t txnfees(txblock_header["TxnFees"].asString());
  rewards += txnfees;
  uint64_t blocknum = strtoull(txblock_header["BlockNum"].asCString(), NULL, 0);
  TxBlockHashSet txblockhashset;
  txblockhashset.m_mbInfoHash =
      MBInfoHash(txblock_header["MbInfoHash"].asString());
  txblockhashset.m_stateDeltaHash =
      StateHash(txblock_header["StateDeltaHash"].asString());
  txblockhashset.m_stateRootHash =
      StateHash(txblock_header["StateRootHash"].asString());
  uint32_t numTxns = txblock_header["NumTxns"].asUInt();
  PubKey minerPubkey = PubKey::GetPubKeyFromString(
      strip_0x(txblock_header["MinerPubKey"].asString()));
  uint64_t dsBlockNum =
      strtoull(txblock_header["DSBlockNum"].asCString(), NULL, 0);
  uint32_t version = txblock_header["Version"].asUInt();
  CommitteeHash commHash(txblock_header["CommitteeHash"].asString());
  BlockHash prevblockhash(txblock_header["PrevBlockHash"].asString());

  TxBlockHeader txblockheader(gaslimit, gasused, rewards, blocknum,
                              txblockhashset, numTxns, minerPubkey, dsBlockNum,
                              version, commHash, prevblockhash);

  bytes cs1_ser;
  bytes cs2_ser;
  if (!DataConversion::HexStrToUint8Vec(txblock_body["CS1"].asString(),
                                        cs1_ser)) {
    LOG_GENERAL(INFO, "Failed HexStrToUint8Vec");
  }
  if (!DataConversion::HexStrToUint8Vec(txblock_body["HeaderSign"].asString(),
                                        cs2_ser)) {
    LOG_GENERAL(INFO, "Failed HexStrToUint8Vec");
  }

  Signature cs1(cs1_ser, 0);
  Signature cs2(cs2_ser, 0);

  vector<bool> b1 = convertJsonBitmapToVector(txblock_body["B1"]);
  vector<bool> b2 = convertJsonBitmapToVector(txblock_body["B2"]);

  auto microblockinfos =
      convert_mbinfos_json_into_cpp_object(txblock_body["MicroBlockInfos"]);

  TxBlock txblock(txblockheader, microblockinfos, CoSignatures());
  CoSignatures cosig(cs1, b1, cs2, b2);
  txblock.SetCoSignatures(cosig);
  uint64_t timestamp =
      strtoull(txblock_header["Timestamp"].asCString(), NULL, 0);
  txblock.SetTimestamp(timestamp);

  return txblock;
}

Json::Value convert_json_file_to_json_object(const string& filePath) {
  Json::Value _json;
  std::ifstream json_file(filePath.c_str(), std::ifstream::binary);
  json_file >> _json;

  return _json;
}

BOOST_AUTO_TEST_CASE(serialize_and_verify_dsblock) {
  INIT_STDOUT_LOGGER();

  const auto& dsblock_json = convert_json_file_to_json_object("dsblock.json");

  const auto& orig = dsblock_json["serialized"]["data"].asString();
  const auto dsblock = convert_to_dsblock(dsblock_json);

  string dsblock_str = convertBlockSerializedtoJson(dsblock).asString();
  string dsblock_header_str =
      convertBlockSerializedtoJson(dsblock.GetHeader()).asString();

  LOG_GENERAL(INFO, "Serialized string :" << dsblock_str);
  LOG_GENERAL(INFO, "Original length: " << orig.size() << endl
                                        << "Length: " << dsblock_str.size());
  LOG_GENERAL(INFO, "Header Ds String" << dsblock_header_str);

  BOOST_CHECK_EQUAL(dsblock_str, orig);
}

BOOST_AUTO_TEST_CASE(serialize_and_verify_txblock) {
  INIT_STDOUT_LOGGER();
  const auto& txblock_json = convert_json_file_to_json_object("txblock.json");
  const auto& orig = txblock_json["serialized"]["data"].asString();
  const auto txblock = convert_to_txblock(txblock_json);

  string txblock_str = convertBlockSerializedtoJson(txblock).asString();
  string txblock_header_str =
      convertBlockSerializedtoJson(txblock.GetHeader()).asString();

  LOG_GENERAL(INFO, "Serialized string: " << txblock_str);
  LOG_GENERAL(INFO, "Original length: " << orig.size() << endl
                                        << "Length: " << txblock_str.size());
  LOG_GENERAL(INFO, "Header Tx String: " << txblock_header_str);

  BOOST_CHECK_EQUAL(txblock_str, orig);
}

BOOST_AUTO_TEST_SUITE_END()