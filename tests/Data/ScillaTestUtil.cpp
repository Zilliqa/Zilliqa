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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libPersistence/ScillaMessage.pb.h"
#pragma GCC diagnostic pop
#include "libUtils/DataConversion.h"

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

uint64_t ScillaTestUtil::GetFileSize(const std::string &filename) {
  if (SCILLA_ROOT.empty()) {
    LOG_GENERAL(WARNING, "SCILLA_ROOT is empty.");
    return 0;
  }

  std::string filepath = SCILLA_FILES + "/" + filename;

  if (!boost::filesystem::exists(filepath)) {
    LOG_GENERAL(WARNING, filename << " does not exist!");
    return 0;
  }

  return boost::filesystem::file_size(filepath);
}

namespace {

bool GetScillaTestHelper(const std::string &version, bool isLibrary,
                         const std::string &contrName, std::string &testDir,
                         std::string &scillaSourceFile,
                         ScillaTestUtil::ScillaTest &t) {
  if (SCILLA_ROOT.empty()) {
    return false;
  }

  if (ENABLE_SCILLA_MULTI_VERSION) {
    testDir = SCILLA_ROOT + "/" + version + "/tests/runner/" + contrName;
    scillaSourceFile = SCILLA_ROOT + "/" + version + "/tests/contracts/" +
                       contrName + (isLibrary ? ".scillib" : ".scilla");
  } else {
    testDir = SCILLA_ROOT + "/tests/runner/" + contrName;
    scillaSourceFile = SCILLA_ROOT + "/tests/contracts/" + contrName +
                       (isLibrary ? ".scillib" : ".scilla");
  }

  LOG_GENERAL(INFO, "ScillaTestUtil::testDir: " << testDir << "\n");

  if (!boost::filesystem::is_directory(testDir)) {
    return false;
  }

  if (!boost::filesystem::is_regular_file(scillaSourceFile)) {
    return false;
  }

  std::ifstream in(scillaSourceFile, std::ios::binary);
  t.code = {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};

  return true;
}

template <typename T>
bool parseInteger(const Json::Value &bj, T &balance) {
  if (!bj.isString()) {
    LOG_GENERAL(INFO, "_balance must be a string");
    return false;
  }
  try {
    balance = boost::lexical_cast<T>(bj.asString());
  } catch (...) {
    LOG_GENERAL(INFO, "_balance must be a valid numerical string");
    return false;
  }
  return true;
}

}  // namespace

// Get ScillaTest for contract "name" and test numbered "i".
// "version" is used only if ENABLE_SCILLA_MULTI_VERSION is set.
bool ScillaTestUtil::GetScillaTest(ScillaTest &t, const std::string &contrName,
                                   unsigned int i, const std::string &version,
                                   bool isLibrary) {
  std::string testDir, scillaSourceFile;

  if (!GetScillaTestHelper(version, isLibrary, contrName, testDir,
                           scillaSourceFile, t)) {
    return false;
  }

  std::string is(std::to_string(i));

  // Get all JSONs
  return isLibrary
             ? (ParseJsonFile(t.init, testDir + "/init.json") &&
                ParseJsonFile(t.blockchain,
                              testDir + "/blockchain_" + is + ".json") &&
                ParseJsonFile(t.expOutput, testDir + "/init_output.json"))
             : (ParseJsonFile(t.init, testDir + "/init.json") &&
                ParseJsonFile(t.state, testDir + "/state_" + is + ".json") &&
                ParseJsonFile(t.blockchain,
                              testDir + "/blockchain_" + is + ".json") &&
                ParseJsonFile(t.expOutput,
                              testDir + "/output_" + is + ".json") &&
                ParseJsonFile(t.message, testDir + "/message_" + is + ".json"));
}

bool ScillaTestUtil::GetScillaDeployment(ScillaTest &t,
                                         const std::string &contrName,
                                         const std::string &estatej_file,
                                         const std::string &initj_file,
                                         const std::string &blockchainj_file,
                                         const std::string &outputj_file,
                                         const std::string &version) {
  std::string testDir, scillaSourceFile;

  if (!GetScillaTestHelper(version, false, contrName, testDir, scillaSourceFile,
                           t)) {
    return false;
  }

  return ParseJsonFile(t.init, testDir + "/" + initj_file) &&
         (estatej_file == "" ||
          ParseJsonFile(t.state, testDir + "/" + estatej_file)) &&
         ParseJsonFile(t.blockchain, testDir + "/" + blockchainj_file) &&
         ParseJsonFile(t.expOutput, testDir + "/" + outputj_file);
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

// Return the _amount in message.json. Remove _amount, _sender and _origin.
uint64_t ScillaTestUtil::PrepareMessageData(Json::Value &message, bytes &data) {
  LOG_GENERAL(INFO, JSONUtils::GetInstance().convertJsontoStr(message));
  uint64_t amount;
  try {
    amount = message["_amount"].asUInt();
  } catch (...) {
    amount = boost::lexical_cast<uint64_t>(message["_amount"].asString());
  }
  // Remove _amount, _sender and _origin as they will be automatically inserted.
  message.removeMember("_amount");
  message.removeMember("_sender");
  message.removeMember("_origin");

  std::string msgStr = JSONUtils::GetInstance().convertJsontoStr(message);
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

// Remove _this_address field from init JSON.
bool ScillaTestUtil::RemoveThisAddressFromInit(Json::Value &init) {
  int this_address_index = -1;
  for (auto it = init.begin(); it != init.end(); it++) {
    if ((*it)["vname"] == "_this_address") {
      this_address_index = it - init.begin();
    }
  }
  // Remove _creation_block from init.json as it will be inserted
  // automatically.
  if (this_address_index >= 0) {
    Json::Value dummy;
    init.removeIndex(Json::ArrayIndex(this_address_index), &dummy);
    return true;
  }

  return false;
}

// Parse a state JSON into C++ maps that can straight away be stored
// in the state storage.
// TODO: This function relies on `Json::isArray()` to determine if
// a Scilla value is a map. This is incorrect because Scilla lists
// can also be encoded as JSON arrays. For non-empty JSON arrays
// we can look further to determine if its a map or not (but is not
// done below), for empty JSON arrays, That's impossible. So use
// the mapdepths mapping instead.
bool ScillaTestUtil::parseStateJSON(
    const Address &contrAddr, const Json::Value &state,
    const std::unordered_map<Address, std::unordered_map<std::string, int>>
        &mapdepths,
    std::map<Address, std::map<std::string, bytes>> &state_entries,
    std::unordered_map<Address, uint128_t> &balances,
    std::unordered_map<Address, uint64_t> &nonces) {
  if (!state.isArray()) {
    LOG_GENERAL(INFO, "State JSON must be an array");
    return false;
  }

  std::function<bool(const std::string &index, const Json::Value &s,
                     std::map<std::string, bytes> &state_entries)>
      mapHandler = [&mapHandler](const std::string &index, const Json::Value &s,
                                 std::map<std::string, bytes> &state_entries) {
        if (!s.isArray()) {
          return false;
        }
        if (s.size() > 0) {
          for (const auto &v : s) {
            if (!v.isMember("key") || !v.isMember("val")) {
              return false;
            }

            std::string t_index =
                index + JSONUtils::GetInstance().convertJsontoStr(v["key"]) +
                SCILLA_INDEX_SEPARATOR;
            if (v["val"].isArray()) {
              if (!mapHandler(t_index, v["val"], state_entries)) {
                return false;
              }
            } else {
              state_entries.emplace(
                  t_index,
                  DataConversion::StringToCharArray(
                      JSONUtils::GetInstance().convertJsontoStr(v["val"])));
            }
          }
        } else {
          ProtoScillaVal emptyPBMap;
          emptyPBMap.mutable_mval()->mutable_m();
          std::string dst;
          if (!emptyPBMap.SerializeToString(&dst)) {
            return false;
          }
          state_entries.emplace(index, DataConversion::StringToCharArray(dst));
        }
        return true;
      };

  bool hasMap = false;

  for (const auto &sv : state) {
    if (!sv.isObject() || !sv.isMember("vname") || !sv["vname"].isString() ||
        !sv.isMember("value")) {
      LOG_GENERAL(INFO, "parseStateJSON: Invalid JSON format");
      return false;
    }

    // They are handled specially and do not need an entry.
    if (sv["vname"] == "_this_address") continue;

    if (sv["vname"] == "_balance") {
      uint128_t balance;
      if (!parseInteger<uint128_t>(sv["value"], balance)) {
        return false;
      }
      balances[contrAddr] = balance;
    } else if (sv["vname"] == "_nonce") {
      uint64_t nonce;
      if (!parseInteger<uint64_t>(sv["value"], nonce)) {
        return false;
      }
      nonces[contrAddr] = nonce;
    } else if (sv["vname"] == "_external") {
      if (!sv["value"].isArray()) {
        LOG_GENERAL(WARNING, "_external fields must be specified as an array");
        return false;
      }
      for (const auto &estate : sv["value"]) {
        if (!estate.isObject() || !estate.isMember("address") ||
            !estate.isMember("state") || !estate["address"].isString() ||
            !estate["state"].isArray()) {
          LOG_GENERAL(WARNING, "_external field specified incorrectly");
          return false;
        }
        Address eAddr(estate["address"].asString());
        if (!parseStateJSON(eAddr, estate["state"], mapdepths, state_entries,
                            balances, nonces)) {
          return false;
        }
      }
    } else {
      std::string bindex = contrAddr.hex() + SCILLA_INDEX_SEPARATOR;
      auto vindex = bindex + sv["vname"].asString() + SCILLA_INDEX_SEPARATOR;
      if (sv["value"].isArray()) {
        if (!mapHandler(vindex, sv["value"], state_entries[contrAddr])) {
          LOG_GENERAL(WARNING, "state format is invalid");
          return false;
        }
      } else {
        state_entries[contrAddr].emplace(
            vindex,
            DataConversion::StringToCharArray(
                JSONUtils::GetInstance().convertJsontoStr(sv["value"])));
      }
      auto tindex = bindex + TYPE_INDICATOR + SCILLA_INDEX_SEPARATOR +
                    sv["vname"].asString() + SCILLA_INDEX_SEPARATOR;
      state_entries[contrAddr].emplace(
          tindex, DataConversion::StringToCharArray(sv["type"].asString()));
      auto mdindex = bindex + MAP_DEPTH_INDICATOR + SCILLA_INDEX_SEPARATOR +
                     sv["vname"].asString() + SCILLA_INDEX_SEPARATOR;
      auto itr = mapdepths.find(contrAddr);
      if (itr != mapdepths.end()) {
        auto itr2 = itr->second.find(sv["vname"].asString());
        if (itr2 != itr->second.end()) {
          if (itr2->second > 0) {
            hasMap = true;
          }
          state_entries[contrAddr].emplace(
              mdindex,
              DataConversion::StringToCharArray(std::to_string(itr2->second)));
        } else {
          LOG_GENERAL(WARNING,
                      "mapdepth not found for " + sv["vname"].asString());
          return false;
        }
      } else {
        LOG_GENERAL(WARNING,
                    "mapdepth not found for " + sv["vname"].asString());
        return false;
      }
    }
  }

  auto hmindex = contrAddr.hex() + SCILLA_INDEX_SEPARATOR + HAS_MAP_INDICATOR +
                 SCILLA_INDEX_SEPARATOR;
  state_entries[contrAddr].emplace(
      hmindex, DataConversion::StringToCharArray(hasMap ? "true" : "false"));

  return true;
}

// Similar to parseStateJSON, this function relies on Json::isArray()
// to check if a value is map. TODO: Use mapdepths instead.
bool ScillaTestUtil::TransformStateJsonFormat(const Json::Value &input,
                                              Json::Value &output) {
  if (!input.isArray()) {
    LOG_GENERAL(WARNING, "Input state JSON must be an array");
    return false;
  }

  std::function<bool(const Json::Value &input, Json::Value &output)>
      mapTransformer = [&mapTransformer](const Json::Value &input,
                                         Json::Value &output) -> bool {
    if (!input.isArray()) {
      LOG_GENERAL(WARNING, "Input map JSON must be an array");
      return false;
    }
    if (input.empty()) {
      output = Json::objectValue;
      return true;
    }
    for (const auto &im : input) {
      if (!im.isMember("key") || !im["key"].isString() || !im.isMember("val")) {
        LOG_GENERAL(WARNING, "Invalid map entry in input");
        return false;
      }
      if (im["val"].isArray()) {
        Json::Value om;
        if (!mapTransformer(im["val"], om)) {
          return false;
        }
        output[im["key"].asString()] = om;
      } else {
        output[im["key"].asString()] = im["val"];
      }
    }
    return true;
  };

  for (const auto &is : input) {
    if (!is.isMember("vname") || !is["vname"].isString() ||
        !is.isMember("value")) {
      LOG_GENERAL(WARNING, "Invalid state variable in input");
      return false;
    }
    if (is["value"].isArray()) {
      // Map value
      Json::Value om;
      if (!mapTransformer(is["value"], om)) {
        return false;
      }
      output[is["vname"].asString()] = om;
    } else {
      output[is["vname"].asString()] = is["value"];
    }
  }

  return true;
}
