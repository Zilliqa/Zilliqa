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

#include <boost/filesystem.hpp>
#include "common/Constants.h"

#include "ScillaTestUtil.h"

using namespace boost::multiprecision;

bool ScillaTestUtil::ParseJsonFile(Json::Value &j, std::string filename) {
  if (!boost::filesystem::is_regular_file(filename)) {
    return false;
  }

  std::ifstream in(filename, std::ios::binary);
  std::string fstr;

  fstr = {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};

  Json::CharReaderBuilder builder;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string errors;

  return reader->parse(fstr.c_str(), fstr.c_str() + fstr.size(), &j, &errors);
}

// Get ScillaTest for contract "name" and test numbered "i".
bool ScillaTestUtil::GetScillaTest(ScillaTest &t, std::string contrName,
                                   unsigned int i) {
  if (SCILLA_ROOT.empty()) {
    return false;
  }

  // TODO: Does this require a separate entry in constants.xml?
  std::string testDir = SCILLA_ROOT + "/tests/contracts/" + contrName;
  if (!boost::filesystem::is_directory(testDir)) {
    return false;
  }

  if (!boost::filesystem::is_regular_file(testDir + "/contract.scilla")) {
    return false;
  }

  std::ifstream in(testDir + "/contract.scilla", std::ios::binary);
  t.code = {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};

  std::string is(std::to_string(i));

  // Get all JSONs
  return !(
      !ParseJsonFile(t.init, testDir + "/init.json") ||
      !ParseJsonFile(t.state, testDir + "/state_" + is + ".json") ||
      !ParseJsonFile(t.blockchain, testDir + "/blockchain_" + is + ".json") ||
      !ParseJsonFile(t.expOutput, testDir + "/output_" + is + ".json") ||
      !ParseJsonFile(t.message, testDir + "/message_" + is + ".json"));
}

// Get _balance from output state of interpreter, from OUTPUT_JSON.
// Return 0 on failure.
uint128_t ScillaTestUtil::GetBalanceFromOutput(void) {
  Json::Value iOutput;
  if (!ScillaTestUtil::ParseJsonFile(iOutput, OUTPUT_JSON)) {
    LOG_GENERAL(WARNING, "Unable to parse output of interpreter.");
    return 0;
  }

  // Get balance as given by the interpreter.
  uint128_t oBal = 0;
  Json::Value states = iOutput["states"];
  for (auto &state : states) {
    if (state["vname"] == "_balance") {
      oBal = atoi(state["value"].asCString());
    }
  }

  return oBal;
}

// Return BLOCKNUMBER in Json. Return 0 if not found.
uint64_t ScillaTestUtil::GetBlockNumberFromJson(Json::Value &blockchain) {
  // Get blocknumber from blockchain.json
  uint64_t bnum = 0;
  for (auto &it : blockchain) {
    if (it["vname"] == "BLOCKNUMBER") {
      bnum = atoi(it["value"].asCString());
    }
  }

  return bnum;
}

// Return the _amount in message.json. Remove that and _sender.
uint64_t ScillaTestUtil::PrepareMessageData(Json::Value &message, bytes &data) {
  uint64_t amount = atoi(message["_amount"].asCString());
  // Remove _amount and _sender as they will be automatically inserted.
  message.removeMember("_amount");
  message.removeMember("_sender");

  std::string msgStr = JSONUtils::convertJsontoStr(message);
  data = bytes(msgStr.begin(), msgStr.end());

  return amount;
}

// Remove _creation_block field from init JSON.
bool ScillaTestUtil::RemoveCreationBlockFromInit(Json::Value &init) {
  int creation_block_index = -1;
  for (auto it = init.begin(); it != init.end(); it++) {
    if ((*it)["vname"] == "_creation_block") {
      creation_block_index = it - init.begin();
    }
  }
  // Remove _creation_block from init.json as it will be inserted
  // automatically.
  if (creation_block_index >= 0) {
    Json::Value dummy;
    init.removeIndex(Json::ArrayIndex(creation_block_index), &dummy);
    return true;
  }

  return false;
}
