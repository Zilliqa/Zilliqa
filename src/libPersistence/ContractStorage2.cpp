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

#include "ContractStorage2.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "ScillaMessage.pb.h"
#pragma GCC diagnostic pop

#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace ZilliqaMessage;

namespace Contract {

ContractStorage2::ContractStorage2()
    : m_codeDB("contractCode"),
      m_initDataDB("contractInitState2"),
      m_stateDataDB("contractStateData2"),
      mp_stateDataDB(make_shared<LevelDB>("contractStateData2Trie")),
      mp_stateDataMap(make_shared<std::unordered_map<dev::h256, bytes>>()),
      mp_indexToBeDeleted(make_shared<std::set<dev::h256>>()),
      tp_stateDataMap(make_shared<std::unordered_map<dev::h256, bytes>>()),
      tp_indexToBeDeleted(make_shared<std::set<dev::h256>>()),
      m_tempADMap(make_shared<DefaultAddDeleteMap>(tp_stateDataMap,
                                                   tp_indexToBeDeleted)),
      m_permADMap(make_shared<RevertableAddDeleteMap>(mp_stateDataMap,
                                                      mp_indexToBeDeleted)),
      m_levelDBMap(make_shared<LevelDBMap>(mp_stateDataDB)),
      m_permOM(m_permADMap, m_levelDBMap),
      m_tempOM(m_tempADMap, m_permADMap, m_levelDBMap),
      m_permTrie(&m_permOM),
      m_tempTrie(&m_tempOM) {}

// Code
// ======================================

bool ContractStorage2::PutContractCode(const dev::h160& address,
                                       const bytes& code) {
  lock_guard<mutex> g(m_codeMutex);
  return m_codeDB.Insert(address.hex(), code) == 0;
}

bool ContractStorage2::PutContractCodeBatch(
    const unordered_map<string, string>& batch) {
  lock_guard<mutex> g(m_codeMutex);
  return m_codeDB.BatchInsert(batch);
}

bytes ContractStorage2::GetContractCode(const dev::h160& address) {
  lock_guard<mutex> g(m_codeMutex);
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage2::DeleteContractCode(const dev::h160& address) {
  lock_guard<mutex> g(m_codeMutex);
  return m_codeDB.DeleteKey(address.hex()) == 0;
}

// InitData
// ========================================
bool ContractStorage2::PutInitData(const dev::h160& address,
                                   const bytes& initData) {
  lock_guard<mutex> g(m_initDataMutex);
  return m_initDataDB.Insert(address.hex(), initData) == 0;
}

bool ContractStorage2::PutInitDataBatch(
    const unordered_map<string, string>& batch) {
  lock_guard<mutex> g(m_initDataMutex);
  return m_initDataDB.BatchInsert(batch);
}

bytes ContractStorage2::GetInitData(const dev::h160& address) {
  lock_guard<mutex> g(m_initDataMutex);
  return DataConversion::StringToCharArray(m_initDataDB.Lookup(address.hex()));
}

bool ContractStorage2::DeleteInitData(const dev::h160& address) {
  lock_guard<mutex> g(m_initDataMutex);
  return m_initDataDB.DeleteKey(address.hex()) == 0;
}
// State
// ========================================
template <class T>
bool SerializeToArray(const T& protoMessage, bytes& dst,
                      const unsigned int offset) {
  if ((offset + protoMessage.ByteSize()) > dst.size()) {
    dst.resize(offset + protoMessage.ByteSize());
  }

  return protoMessage.SerializeToArray(dst.data() + offset,
                                       protoMessage.ByteSize());
}

string ContractStorage2::GenerateStorageKey(const dev::h160& addr,
                                            const string& vname,
                                            const vector<string>& indices) {
  LOG_MARKER();
  string ret = addr.hex();
  if (!vname.empty()) {
    ret += SCILLA_INDEX_SEPARATOR + vname + SCILLA_INDEX_SEPARATOR;
    for (const auto& index : indices) {
      ret += index + SCILLA_INDEX_SEPARATOR;
    }
  }
  return ret;
}

const size_t addr_separator_size =
    (ACC_ADDR_SIZE * 2) + 1 /*SCILLA_INDEX_SEPARATOR*/;

string ContractStorage2::RemoveAddrFromKey(const std::string& key) {
  string ret;

  try {
    ret = key.substr(addr_separator_size);
  } catch (std::out_of_range&) {
    ret = "";
  }

  return ret;
}

bool ContractStorage2::FetchStateValue(const dev::h160& addr, const bytes& src,
                                       unsigned int s_offset, bytes& dst,
                                       unsigned int d_offset, bool& foundVal,
                                       bool getType, string& type) {
  LOG_MARKER();

  foundVal = true;

  if (s_offset > src.size()) {
    LOG_GENERAL(WARNING, "Invalid src data and offset, data size "
                             << src.size() << ", offset " << s_offset);
  }
  if (d_offset > dst.size()) {
    LOG_GENERAL(WARNING, "Invalid dst data and offset, data size "
                             << dst.size() << ", offset " << d_offset);
  }

  ProtoScillaQuery query;
  query.ParseFromArray(src.data() + s_offset, src.size() - s_offset);

  if (!query.IsInitialized()) {
    LOG_GENERAL(WARNING, "Parse bytes into ProtoScillaQuery failed");
    return false;
  }

  if (LOG_SC) {
    LOG_GENERAL(INFO, "query for fetch: " << query.DebugString());
  }

  if (query.name() == CONTRACT_ADDR_INDICATOR ||
      query.name() == SCILLA_VERSION_INDICATOR ||
      query.name() == MAP_DEPTH_INDICATOR || query.name() == TYPE_INDICATOR ||
      query.name() == HAS_MAP_INDICATOR) {
    LOG_GENERAL(WARNING, "invalid query: " << query.name());
    return false;
  }

  if (getType) {
    std::map<std::string, bytes> t_type;
    std::string type_key =
        GenerateStorageKey(addr, TYPE_INDICATOR, {query.name()});
    FetchStateDataForKey(t_type, type_key, true);
    if (t_type.empty()) {
      LOG_GENERAL(WARNING, "Failed to fetch type for addr: "
                               << addr.hex() << " vname: " << query.name());
      return false;
    }
    try {
      type = DataConversion::CharArrayToString(t_type[type_key]);
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "invalid type: " << type_key << endl << e.what());
    }
  }

  string key = addr.hex() + SCILLA_INDEX_SEPARATOR + query.name() +
               SCILLA_INDEX_SEPARATOR;

  ProtoScillaVal value;

  for (const auto& index : query.indices()) {
    key += index + SCILLA_INDEX_SEPARATOR;
  }

  if ((unsigned int)query.indices().size() > query.mapdepth()) {
    LOG_GENERAL(WARNING, "indices is deeper than map depth");
    return false;
  }

  lock_guard<mutex> g(m_stateDataMutex);

  auto d_found = t_indexToBeDeleted.find(key);
  if (d_found != t_indexToBeDeleted.end()) {
    // ignore the deleted empty placeholder
    if ((unsigned int)query.indices().size() == query.mapdepth()) {
      foundVal = false;
      return true;
    }
  }

  d_found = m_indexToBeDeleted.find(key);
  if (d_found != m_indexToBeDeleted.end() &&
      t_stateDataMap.find(key) == t_stateDataMap.end()) {
    // ignore the deleted empty placeholder
    if ((unsigned int)query.indices().size() == query.mapdepth()) {
      foundVal = false;
      return true;
    }
  }

  if ((unsigned int)query.indices().size() == query.mapdepth()) {
    // result will not be a map and can be just fetched into the store
    bytes bval;
    bool found = false;

    const auto& t_found = t_stateDataMap.find(key);
    if (t_found != t_stateDataMap.end()) {
      bval = t_found->second;
      found = true;
    }
    if (!found) {
      const auto& m_found = m_stateDataMap.find(key);
      if (m_found != m_stateDataMap.end()) {
        bval = m_found->second;
        found = true;
      }
    }
    if (!found) {
      if (m_stateDataDB.Exists(key)) {
        if (query.ignoreval()) {
          return true;
        }
        bval = DataConversion::StringToCharArray(m_stateDataDB.Lookup(key));
      } else {
        if (query.mapdepth() == 0) {
          // for non-map value, should be existing in db otherwise error
          return false;
        } else {
          // for in-map value, it's okay if cannot find
          foundVal = false;
          return true;
        }
      }
    }

    value.set_bval(bval.data(), bval.size());
    if (LOG_SC) {
      LOG_GENERAL(INFO, "value to fetch 1: " << value.DebugString());
    }
    return SerializeToArray(value, dst, 0);
  }

  // We're fetching a Map value. Need to iterate level-db lexicographically
  // first fetch from t_data, then m_data, lastly db
  auto p = t_stateDataMap.lower_bound(key);

  unordered_map<string, bytes> entries;

  while (p != t_stateDataMap.end() &&
         p->first.compare(0, key.size(), key) == 0) {
    if (query.ignoreval()) {
      return true;
    }
    entries.emplace(p->first, p->second);
    ++p;
  }

  p = m_stateDataMap.lower_bound(key);

  while (p != m_stateDataMap.end() &&
         p->first.compare(0, key.size(), key) == 0) {
    if (query.ignoreval()) {
      return true;
    }
    auto exist = entries.find(p->first);
    if (exist == entries.end()) {
      entries.emplace(p->first, p->second);
    }
    ++p;
  }

  std::unique_ptr<leveldb::Iterator> it(
      m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions()));

  it->Seek({key});
  if (!it->Valid() || it->key().ToString().compare(0, key.size(), key) != 0) {
    // no entry
    if (entries.empty()) {
      foundVal = false;
      /// if querying the var without indices but still failed
      /// maybe trying to fetching an invalid vname
      /// as empty map will always have
      /// an empty serialized ProtoScillaVal placeholder
      /// so shouldn't be empty normally
      return !query.indices().empty();
    }
  } else {
    if (query.ignoreval()) {
      return true;
    }
    // found entries
    for (; it->Valid() && it->key().ToString().compare(0, key.size(), key) == 0;
         it->Next()) {
      auto exist = entries.find(it->key().ToString());
      if (exist == entries.end()) {
        bytes val(it->value().data(), it->value().data() + it->value().size());
        entries.emplace(it->key().ToString(), val);
      }
    }
  }

  set<string>::iterator isDeleted;

  uint32_t counter = 0;

  for (const auto& entry : entries) {
    isDeleted = t_indexToBeDeleted.find(entry.first);
    if (isDeleted != t_indexToBeDeleted.end()) {
      continue;
    }
    isDeleted = m_indexToBeDeleted.find(entry.first);
    if (isDeleted != m_indexToBeDeleted.end() &&
        t_stateDataMap.find(entry.first) == t_stateDataMap.end()) {
      continue;
    }

    counter++;

    std::vector<string> indices;
    // remove the prefixes, as shown below surrounded by []
    // [address.vname.index0.index1.(...).]indexN0.indexN1.(...).indexNn
    if (!boost::starts_with(entry.first, key)) {
      LOG_GENERAL(WARNING, "Key is not a prefix of stored entry");
      return false;
    }
    if (entry.first.size() > key.size()) {
      string key_non_prefix =
          entry.first.substr(key.size(), entry.first.size());
      boost::split(indices, key_non_prefix,
                   bind1st(std::equal_to<char>(), SCILLA_INDEX_SEPARATOR));
    }
    if (indices.size() > 0 && indices.back().empty()) indices.pop_back();

    ProtoScillaVal* t_value = &value;
    for (const auto& index : indices) {
      t_value = &(t_value->mutable_mval()->mutable_m()->operator[](index));
    }
    if (query.indices().size() + indices.size() < query.mapdepth()) {
      // Assert that we have a protobuf encoded empty map.
      ProtoScillaVal emap;
      emap.ParseFromArray(entry.second.data(), entry.second.size());
      if (!emap.has_mval() || !emap.mval().m().empty()) {
        LOG_GENERAL(WARNING,
                    "Expected protobuf encoded empty map since entry has fewer "
                    "keys than mapdepth");
        return false;
      }
      t_value->mutable_mval()->mutable_m();  // Create empty map.
    } else {
      t_value->set_bval(entry.second.data(), entry.second.size());
    }
  }

  if (!counter) {
    foundVal = false;
    return true;
  }

  if (LOG_SC) {
    LOG_GENERAL(INFO, "value to fetch 2: " << value.DebugString());
  }
  return SerializeToArray(value, dst, 0);
}

bool ContractStorage2::FetchExternalStateValue(
    const dev::h160& caller, const dev::h160& target, const bytes& src,
    unsigned int s_offset, bytes& dst, unsigned int d_offset, bool& foundVal,
    string& type, uint32_t caller_version) {
  // get caller version if not available
  if (caller_version == std::numeric_limits<uint32_t>::max()) {
    std::map<std::string, bytes> t_caller_version;
    string version_key =
        GenerateStorageKey(caller, SCILLA_VERSION_INDICATOR, {});
    FetchStateDataForKey(t_caller_version, version_key, true);
    if (t_caller_version.empty()) {
      return false;
    }
    try {
      caller_version = std::stoul(
          DataConversion::CharArrayToString(t_caller_version[version_key]));
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "invalid caller_version " << version_key << endl
                                                     << e.what());
      return false;
    }
  }

  // get target version if not available
  std::map<std::string, bytes> t_target_version;
  string version_key = GenerateStorageKey(target, SCILLA_VERSION_INDICATOR, {});
  FetchStateDataForKey(t_target_version, version_key, true);
  if (t_target_version.empty()) {
    return false;
  }

  uint32_t target_version;
  try {
    target_version = std::stoul(
        DataConversion::CharArrayToString(t_target_version[version_key]));
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "invalid target_version: " << version_key << endl
                                                    << e.what());
    return false;
  }

  if (target_version != caller_version) {
    LOG_GENERAL(WARNING, "Caller(" << caller_version << ") target("
                                   << target_version << ") version mismatch");
    return false;
  }

  // get value
  return FetchStateValue(target, src, s_offset, dst, d_offset, foundVal, true,
                         type);
}

void ContractStorage2::DeleteByPrefix(const string& prefix) {
  lock_guard<mutex> g(m_stateDataMutex);

  auto p = t_stateDataMap.lower_bound(prefix);
  while (p != t_stateDataMap.end() &&
         p->first.compare(0, prefix.size(), prefix) == 0) {
    p_indexToBeDeleted.emplace(p->first, true);  // for reverting
    t_indexToBeDeleted.emplace(p->first);
    ++p;
  }

  p = m_stateDataMap.lower_bound(prefix);
  while (p != m_stateDataMap.end() &&
         p->first.compare(0, prefix.size(), prefix) == 0) {
    p_indexToBeDeleted.emplace(p->first, true);  // for reverting
    t_indexToBeDeleted.emplace(p->first);
    ++p;
  }

  std::unique_ptr<leveldb::Iterator> it(
      m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions()));

  it->Seek({prefix});
  if (!it->Valid() ||
      it->key().ToString().compare(0, prefix.size(), prefix) != 0) {
    // no entry
  } else {
    for (; it->Valid() &&
           it->key().ToString().compare(0, prefix.size(), prefix) == 0;
         it->Next()) {
      p_indexToBeDeleted.emplace(it->key().ToString(), true);  // for reverting
      t_indexToBeDeleted.emplace(it->key().ToString());
    }
  }
}

void ContractStorage2::DeleteByIndex(const string& index) {
  lock_guard<mutex> g(m_stateDataMutex);

  if (t_stateDataMap.find(index) != t_stateDataMap.end() ||
      m_stateDataMap.find(index) != m_stateDataMap.end() ||
      m_stateDataDB.Exists(index)) {
    p_indexToBeDeleted.emplace(index, true);  // for reverting
    t_indexToBeDeleted.emplace(index);
  }
}

void UnquoteString(string& input) {
  if (input.empty()) {
    return;
  }
  if (input.front() == '"') {
    input.erase(0, 1);
  }
  if (input.back() == '"') {
    input.pop_back();
  }
}

void ContractStorage2::InsertValueToStateJson(Json::Value& _json, string key,
                                              string value, bool unquote,
                                              bool nokey) {
  if (unquote) {
    // unquote key
    UnquoteString(key);
  }

  Json::Value j_value;

  if (JSONUtils::GetInstance().convertStrtoJson(value, j_value) &&
      (j_value.type() == Json::arrayValue ||
       j_value.type() == Json::objectValue)) {
    if (nokey) {
      _json = j_value;
    } else {
      if (unquote && !nokey) {
        UnquoteString(value);
      }
      _json[key] = j_value;
    }
  } else {
    if (nokey) {
      _json = j_value;
    } else {
      if (unquote && !nokey) {
        UnquoteString(value);
      }
      _json[key] = value;
    }
  }
}

bool ContractStorage2::FetchStateJsonForContract(Json::Value& _json,
                                                 const dev::h160& address,
                                                 const string& vname,
                                                 const vector<string>& indices,
                                                 bool temp) {
  LOG_MARKER();

  std::map<std::string, bytes> states;
  FetchStateDataForContract(states, address, vname, indices, temp);

  for (const auto& state : states) {
    vector<string> fragments;
    boost::split(fragments, state.first,
                 bind1st(std::equal_to<char>(), SCILLA_INDEX_SEPARATOR));
    if (fragments.at(0) != address.hex()) {
      LOG_GENERAL(WARNING, "wrong state fetched: " << state.first);
      return false;
    }
    if (fragments.back().empty()) fragments.pop_back();

    string vname = fragments.at(1);

    if (vname == CONTRACT_ADDR_INDICATOR || vname == SCILLA_VERSION_INDICATOR ||
        vname == MAP_DEPTH_INDICATOR || vname == TYPE_INDICATOR ||
        vname == HAS_MAP_INDICATOR) {
      continue;
    }

    /// addr+vname+[indices...]
    vector<string> map_indices(fragments.begin() + 2, fragments.end());

    std::function<void(Json::Value&, const vector<string>&, const bytes&,
                       unsigned int, int)>
        jsonMapWrapper = [&](Json::Value& _json, const vector<string>& indices,
                             const bytes& value, unsigned int cur_index,
                             int mapdepth) -> void {
      if (cur_index + 1 < indices.size()) {
        string key = indices.at(cur_index);
        UnquoteString(key);
        jsonMapWrapper(_json[key], indices, value, cur_index + 1, mapdepth);
      } else {
        if (mapdepth > 0) {
          if ((int)indices.size() == mapdepth) {
            InsertValueToStateJson(_json, indices.at(cur_index),
                                   DataConversion::CharArrayToString(value));
          } else {
            if (indices.empty()) {
              _json = Json::objectValue;
            } else {
              string key = indices.at(cur_index);
              UnquoteString(key);
              _json[key] = Json::objectValue;
            }
          }
        } else if (mapdepth == 0) {
          InsertValueToStateJson(
              _json, "", DataConversion::CharArrayToString(value), true, true);
        } else {
          /// Enters only when the fields_map_depth not available, almost
          /// impossible Check value whether parsable to Protobuf
          ProtoScillaVal empty_val;
          if (empty_val.ParseFromArray(value.data(), value.size()) &&
              empty_val.IsInitialized() && empty_val.has_mval() &&
              empty_val.mval().m().empty()) {
            string key = indices.at(cur_index);
            UnquoteString(key);
            _json[key] = Json::objectValue;
          } else {
            InsertValueToStateJson(_json, indices.at(cur_index),
                                   DataConversion::CharArrayToString(value));
          }
        }
      }
    };

    map<string, bytes> map_depth;
    string map_depth_key =
        GenerateStorageKey(address, MAP_DEPTH_INDICATOR, {vname});
    FetchStateDataForKey(map_depth, map_depth_key, temp);

    jsonMapWrapper(_json[vname], map_indices, state.second, 0,
                   !map_depth.empty()
                       ? std::stoi(DataConversion::CharArrayToString(
                             map_depth[map_depth_key]))
                       : -1);
  }

  return true;
}

void ContractStorage2::FetchStateDataForKey(map<string, bytes>& states,
                                            const string& key, bool temp) {
  LOG_MARKER();

  lock_guard<mutex> g(m_stateDataMutex);

  std::map<std::string, bytes>::iterator p;
  if (temp) {
    p = t_stateDataMap.lower_bound(key);
    while (p != t_stateDataMap.end() &&
           p->first.compare(0, key.size(), key) == 0) {
      states.emplace(p->first, p->second);
      ++p;
    }
  }

  p = m_stateDataMap.lower_bound(key);
  while (p != m_stateDataMap.end() &&
         p->first.compare(0, key.size(), key) == 0) {
    if (states.find(p->first) == states.end()) {
      states.emplace(p->first, p->second);
    }
    ++p;
  }

  std::unique_ptr<leveldb::Iterator> it(
      m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions()));

  it->Seek({key});
  if (!it->Valid() || it->key().ToString().compare(0, key.size(), key) != 0) {
    // no entry
  } else {
    for (; it->Valid() && it->key().ToString().compare(0, key.size(), key) == 0;
         it->Next()) {
      if (states.find(it->key().ToString()) == states.end()) {
        bytes val(it->value().data(), it->value().data() + it->value().size());
        states.emplace(it->key().ToString(), val);
      }
    }
  }

  if (temp) {
    for (auto it = states.begin(); it != states.end();) {
      if (t_indexToBeDeleted.find(it->first) != t_indexToBeDeleted.cend()) {
        it = states.erase(it);
      } else {
        it++;
      }
    }
  }

  for (auto it = states.begin(); it != states.end();) {
    if (m_indexToBeDeleted.find(it->first) != m_indexToBeDeleted.cend() &&
        ((temp && t_stateDataMap.find(it->first) == t_stateDataMap.end()) ||
         !temp)) {
      it = states.erase(it);
    } else {
      it++;
    }
  }
}

void ContractStorage2::FetchStateDataForContract(map<string, bytes>& states,
                                                 const dev::h160& address,
                                                 const string& vname,
                                                 const vector<string>& indices,
                                                 bool temp) {
  string key = GenerateStorageKey(address, vname, indices);
  FetchStateDataForKey(states, key, temp);
}

void ContractStorage2::FetchUpdatedStateValuesForAddress(
    const dev::h160& address, map<string, bytes>& t_states,
    vector<std::string>& toDeletedIndices, bool temp) {
  LOG_MARKER();

  if (address == dev::h160()) {
    LOG_GENERAL(WARNING, "address provided is empty");
    return;
  }

  lock_guard<mutex> g(m_stateDataMutex);

  if (temp) {
    auto p = t_stateDataMap.lower_bound(address.hex());
    while (p != t_stateDataMap.end() &&
           p->first.compare(0, address.hex().size(), address.hex()) == 0) {
      t_states.emplace(p->first, p->second);
      ++p;
    }

    auto r = t_indexToBeDeleted.lower_bound(address.hex());
    while (r != t_indexToBeDeleted.end() &&
           r->compare(0, address.hex().size(), address.hex()) == 0) {
      toDeletedIndices.emplace_back(*r);
      ++r;
    }
  } else {
    auto p = m_stateDataMap.lower_bound(address.hex());
    while (p != m_stateDataMap.end() &&
           p->first.compare(0, address.hex().size(), address.hex()) == 0) {
      if (t_states.find(p->first) == t_states.end()) {
        t_states.emplace(p->first, p->second);
      }
      ++p;
    }

    std::unique_ptr<leveldb::Iterator> it(
        m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions()));

    it->Seek({address.hex()});
    if (!it->Valid() || it->key().ToString().compare(0, address.hex().size(),
                                                     address.hex()) != 0) {
      // no entry
    } else {
      for (; it->Valid() && it->key().ToString().compare(
                                0, address.hex().size(), address.hex()) == 0;
           it->Next()) {
        if (t_states.find(it->key().ToString()) == t_states.end()) {
          bytes val(it->value().data(),
                    it->value().data() + it->value().size());
          t_states.emplace(it->key().ToString(), val);
        }
      }
    }

    auto r = m_indexToBeDeleted.lower_bound(address.hex());
    while (r != m_indexToBeDeleted.end() &&
           r->compare(0, address.hex().size(), address.hex()) == 0) {
      toDeletedIndices.emplace_back(*r);
      ++r;
    }
  }

  for (auto it = t_states.begin(); it != t_states.end();) {
    if (m_indexToBeDeleted.find(it->first) != m_indexToBeDeleted.cend() &&
        ((temp && t_stateDataMap.find(it->first) == t_stateDataMap.end()) ||
         !temp)) {
      it = t_states.erase(it);
    } else {
      it++;
    }
  }
}

bool ContractStorage2::CleanEmptyMapPlaceholders(const string& key) {
  // key = 0xabc.vname.[index1.index2.[...].indexn.
  vector<string> indices;
  boost::split(indices, key,
               bind1st(std::equal_to<char>(), SCILLA_INDEX_SEPARATOR));
  if (indices.size() < 2) {
    LOG_GENERAL(WARNING, "indices size too small: " << indices.size());
    return false;
  }
  if (indices.back().empty()) indices.pop_back();

  string scankey = indices.at(0) + SCILLA_INDEX_SEPARATOR + indices.at(1) +
                   SCILLA_INDEX_SEPARATOR;
  DeleteByIndex(scankey);  // clean root level

  for (unsigned int i = 2; i < indices.size() - 1 /*exclude the value key*/;
       ++i) {
    scankey += indices.at(i) + SCILLA_INDEX_SEPARATOR;
    DeleteByIndex(scankey);
  }
  return true;
}

void ContractStorage2::UpdateStateData(const string& key, const bytes& value,
                                       bool cleanEmpty) {
  if (LOG_SC) {
    LOG_GENERAL(INFO, "key: " << key << " value: "
                              << DataConversion::CharArrayToString(value));
  }

  if (cleanEmpty) {
    CleanEmptyMapPlaceholders(key);
  }

  lock_guard<mutex> g(m_stateDataMutex);

  auto pos = t_indexToBeDeleted.find(key);
  if (pos != t_indexToBeDeleted.end()) {
    t_indexToBeDeleted.erase(pos);
    // for reverting
    p_indexToBeDeleted.emplace(key, false);
  }

  // for reverting
  if (t_stateDataMap.find(key) != t_stateDataMap.end()) {
    p_stateDataMap[key] = t_stateDataMap[key];
  } else {
    p_stateDataMap[key] = {};
  }

  t_stateDataMap[key] = value;
}

bool ContractStorage2::UpdateStateValue(const dev::h160& addr, const bytes& q,
                                        unsigned int q_offset, const bytes& v,
                                        unsigned int v_offset) {
  LOG_MARKER();

  if (q_offset > q.size()) {
    LOG_GENERAL(WARNING, "Invalid query data and offset, data size "
                             << q.size() << ", offset " << q_offset);
    return false;
  }

  if (v_offset > v.size()) {
    LOG_GENERAL(WARNING, "Invalid value data and offset, data size "
                             << v.size() << ", offset " << v_offset);
  }

  ProtoScillaQuery query;
  query.ParseFromArray(q.data() + q_offset, q.size() - q_offset);

  if (!query.IsInitialized()) {
    LOG_GENERAL(WARNING, "Parse bytes into ProtoScillaQuery failed");
    return false;
  }

  ProtoScillaVal value;
  value.ParseFromArray(v.data() + v_offset, v.size() - v_offset);

  if (!value.IsInitialized()) {
    LOG_GENERAL(WARNING, "Parse bytes into ProtoScillaVal failed");
    return false;
  }

  if (query.name() == CONTRACT_ADDR_INDICATOR ||
      query.name() == SCILLA_VERSION_INDICATOR ||
      query.name() == MAP_DEPTH_INDICATOR || query.name() == TYPE_INDICATOR ||
      query.name() == HAS_MAP_INDICATOR) {
    LOG_GENERAL(WARNING, "invalid query: " << query.name());
    return false;
  }

  string key = addr.hex() + SCILLA_INDEX_SEPARATOR + query.name() +
               SCILLA_INDEX_SEPARATOR;

  if (query.ignoreval()) {
    if (query.indices().size() < 1) {
      LOG_GENERAL(WARNING, "indices cannot be empty")
      return false;
    }
    for (int i = 0; i < query.indices().size() - 1; ++i) {
      key += query.indices().Get(i) + SCILLA_INDEX_SEPARATOR;
    }
    string parent_key = key;
    key += query.indices().Get(query.indices().size() - 1) +
           SCILLA_INDEX_SEPARATOR;
    if (LOG_SC) {
      LOG_GENERAL(INFO, "Delete key: " << key);
    }
    DeleteByPrefix(key);

    map<string, bytes> t_states;
    FetchStateDataForKey(t_states, parent_key, true);
    if (t_states.empty()) {
      ProtoScillaVal empty_val;
      empty_val.mutable_mval()->mutable_m();
      bytes dst;
      if (!SerializeToArray(empty_val, dst, 0)) {
        LOG_GENERAL(WARNING, "empty_mval SerializeToArray failed");
        return false;
      }
      UpdateStateData(parent_key, dst);
    }
  } else {
    for (const auto& index : query.indices()) {
      key += index + SCILLA_INDEX_SEPARATOR;
    }

    if ((unsigned int)query.indices().size() > query.mapdepth()) {
      LOG_GENERAL(WARNING, "indices is deeper than map depth");
      return false;
    } else if ((unsigned int)query.indices().size() == query.mapdepth()) {
      if (value.has_mval()) {
        LOG_GENERAL(WARNING, "val is not bytes but supposed to be");
        return false;
      }
      UpdateStateData(key, DataConversion::StringToCharArray(value.bval()),
                      true);
      return true;
    } else {
      DeleteByPrefix(key);

      std::function<bool(const string&, const ProtoScillaVal&)> mapHandler =
          [&](const string& keyAcc, const ProtoScillaVal& value) -> bool {
        if (!value.has_mval()) {
          LOG_GENERAL(WARNING, "val is not map but supposed to be");
          return false;
        }
        if (value.mval().m().empty()) {
          // We have an empty map. Insert an entry for keyAcc in
          // the store to indicate that the key itself exists.
          bytes dst;
          if (!SerializeToArray(value, dst, 0)) {
            return false;
          }
          // DB Put
          UpdateStateData(keyAcc, dst, true);
          return true;
        }
        for (const auto& entry : value.mval().m()) {
          string index(keyAcc);
          index += entry.first + SCILLA_INDEX_SEPARATOR;
          if (entry.second.has_mval()) {
            // We haven't reached the deepeast nesting
            if (!mapHandler(index, entry.second)) {
              return false;
            }
          } else {
            // DB Put
            if (LOG_SC) {
              LOG_GENERAL(INFO, "mval().m() first: " << entry.first
                                                     << " second: "
                                                     << entry.second.bval());
            }
            UpdateStateData(
                index, DataConversion::StringToCharArray(entry.second.bval()),
                true);
          }
        }
        return true;
      };

      return mapHandler(key, value);
    }
  }
  return true;
}

void ContractStorage2::UpdateStateDatasAndToDeletes(
    const dev::h160& addr, const std::map<std::string, bytes>& states,
    const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
    bool temp, bool revertible, bool migrating) {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_stateDataMutex);

    if (temp) {
      for (const auto& state : states) {
        t_stateDataMap[state.first] = state.second;
        auto pos = t_indexToBeDeleted.find(state.first);
        if (pos != t_indexToBeDeleted.end()) {
          t_indexToBeDeleted.erase(pos);
        }
      }
      for (const auto& index : toDeleteIndices) {
        t_indexToBeDeleted.emplace(index);
      }
    } else {
      for (const auto& state : states) {
        if (revertible) {
          if (m_stateDataMap.find(state.first) != m_stateDataMap.end()) {
            r_stateDataMap[state.first] = m_stateDataMap[state.first];
          } else {
            r_stateDataMap[state.first] = {};
          }
        }
        m_stateDataMap[state.first] = state.second;
        auto pos = m_indexToBeDeleted.find(state.first);
        if (pos != m_indexToBeDeleted.end()) {
          m_indexToBeDeleted.erase(pos);
          if (revertible) {
            r_indexToBeDeleted.emplace(state.first, false);
          }
        }
      }
      for (const auto& toDelete : toDeleteIndices) {
        if (revertible) {
          r_indexToBeDeleted.emplace(toDelete, true);
        }
        m_indexToBeDeleted.emplace(toDelete);
      }
    }
  }

  if (!migrating && !temp) {
    // Only when merging state delta into perm
    stateHash = GetContractStateHashForMergingDelta(addr, stateHash, states,
                                                    toDeleteIndices);
  } else {
    // applies to all other situations
    stateHash = GetContractStateHash(addr, stateHash, temp, revertible);
  }
}

void ContractStorage2::ResetBufferedAtomicState() {
  LOG_MARKER();

  lock_guard<mutex> g(m_stateDataMutex);
  p_stateDataMap.clear();
  p_indexToBeDeleted.clear();
}

void ContractStorage2::RevertAtomicState() {
  LOG_MARKER();
  lock_guard<mutex> g(m_stateDataMutex);

  for (const auto& data : p_stateDataMap) {
    if (data.second.empty()) {
      t_stateDataMap.erase(data.first);
    } else {
      t_stateDataMap[data.first] = data.second;
    }
  }

  for (const auto& index : p_indexToBeDeleted) {
    if (index.second) {
      // revert newly added indexToBeDeleted
      const auto& found = t_indexToBeDeleted.find(index.first);
      if (found != t_indexToBeDeleted.end()) {
        t_indexToBeDeleted.erase(found);
      }
    } else {
      // revert newly deleted indexToBeDeleted
      t_indexToBeDeleted.emplace(index.first);
    }
  }
}

void ContractStorage2::RevertContractStates() {
  LOG_MARKER();
  lock_guard<mutex> g(m_stateDataMutex);

  for (const auto& data : r_stateDataMap) {
    if (data.second.empty()) {
      m_stateDataMap.erase(data.first);
    } else {
      m_stateDataMap[data.first] = data.second;
    }
  }

  for (const auto& index : r_indexToBeDeleted) {
    if (index.second) {
      // revert newly added indexToBeDeleted
      const auto& found = m_indexToBeDeleted.find(index.first);
      if (found != m_indexToBeDeleted.end()) {
        m_indexToBeDeleted.erase(found);
      }
    } else {
      // revert newly deleted indexToBeDeleted
      m_indexToBeDeleted.emplace(index.first);
    }
  }

  m_permADMap->revert();
}

void ContractStorage2::InitRevertibles() {
  LOG_MARKER();
  lock_guard<mutex> g(m_stateDataMutex);
  r_stateDataMap.clear();
  r_indexToBeDeleted.clear();

  m_permADMap->reset_recordings();
}

bool ContractStorage2::CommitStateDB() {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_stateDataMutex);
    // copy everything into m_stateXXDB;
    // Index
    unordered_map<string, std::string> batch;

    // Data
    for (const auto& i : m_stateDataMap) {
      batch.insert({i.first, DataConversion::CharArrayToString(i.second)});
    }
    if (!m_stateDataDB.BatchInsert(batch)) {
      LOG_GENERAL(WARNING, "BatchInsert m_stateDataDB failed");
      return false;
    }
    // ToDelete
    for (const auto& index : m_indexToBeDeleted) {
      if (m_stateDataDB.DeleteKey(index) < 0) {
        LOG_GENERAL(WARNING, "DeleteKey " << index << " failed");
        return false;
      }
    }

    // For State Merkle Trie
    batch.clear();
    // ADDS
    for (const auto& i : *mp_stateDataMap) {
      if (LOG_SC) {
        LOG_GENERAL(INFO, "DB batch insert: "
                              << i.first.hex() << endl
                              << DataConversion::CharArrayToString(i.second));
      }
      batch.insert(
          {i.first.hex(), DataConversion::CharArrayToString(i.second)});
    }
    if (!mp_stateDataDB->BatchInsert(batch)) {
      LOG_GENERAL(WARNING, "BatchInsert m_stateDataDB failed");
      return false;
    }

    // DELETES
    for (const auto& index : *mp_indexToBeDeleted) {
      if (LOG_SC) {
        LOG_GENERAL(INFO, "DB delete: " << index.hex());
      }
      if (mp_stateDataDB->DeleteKey(index) < 0) {
        LOG_GENERAL(WARNING, "DeleteKey " << index << " failed");
        return false;
      }
    }

    m_stateDataMap.clear();
    m_indexToBeDeleted.clear();

    mp_stateDataMap->clear();
    mp_indexToBeDeleted->clear();
  }

  InitTempState();

  return true;
}

void ContractStorage2::InitTempState() {
  lock_guard<mutex> g(m_stateDataMutex);

  t_stateDataMap.clear();
  t_indexToBeDeleted.clear();

  m_tempADMap->reset();
}

dev::h256 ContractStorage2::DirectHashState(
    const std::map<std::string, bytes>& states) {
  LOG_MARKER();

  // iterate the raw protobuf string and hash
  SHA2<HashType::HASH_VARIANT_256> sha2;
  for (const auto& state : states) {
    if (LOG_SC) {
      LOG_GENERAL(INFO, "state key: "
                            << state.first << " value: "
                            << DataConversion::CharArrayToString(state.second));
    }
    sha2.Update(DataConversion::StringToCharArray(state.first));
    if (!state.second.empty()) {
      sha2.Update(state.second);
    }
  }
  // return dev::h256(sha2.Finalize());
  dev::h256 ret(sha2.Finalize());
  return ret;
}

dev::h256 ContractStorage2::UpdateContractTrie(
    const dev::h256& root, const std::map<std::string, bytes>& states,
    const std::vector<std::string>& toDeletedIndices, bool temp,
    bool revertible) {
  LOG_MARKER();

  lock_guard<mutex> g(m_stateMPTMutex);

  LOG_GENERAL(INFO, "root: " << root.hex());

  if (temp) {
    if (root == dev::h256()) {
      m_tempTrie.init();
    } else {
      m_tempTrie.setRoot(root);
    }

    for (const auto& state : states) {
      if (LOG_SC) {
        LOG_GENERAL(INFO, "insert key: " << state.first << " value: "
                                         << DataConversion::CharArrayToString(
                                                state.second));
      }
      m_tempTrie.insert(
          DataConversion::StringToCharArray(RemoveAddrFromKey(state.first)),
          state.second);
    }

    for (const auto& index : toDeletedIndices) {
      bytes b_index =
          DataConversion::StringToCharArray(RemoveAddrFromKey(index));
      if (m_tempTrie.contains(b_index)) {
        if (LOG_SC) {
          LOG_GENERAL(INFO, "delete key: " << index);
        }
        m_tempTrie.remove(b_index);
      }
    }

    return m_tempTrie.root();
  } else {
    if (revertible) {
      m_permADMap->start_recording();
    }

    if (root == dev::h256()) {
      m_permTrie.init();
    } else {
      m_permTrie.setRoot(root);
    }

    for (const auto& state : states) {
      if (LOG_SC) {
        LOG_GENERAL(INFO, "insert key: " << state.first << " value: "
                                         << DataConversion::CharArrayToString(
                                                state.second));
      }
      m_permTrie.insert(
          DataConversion::StringToCharArray(RemoveAddrFromKey(state.first)),
          state.second);
    }

    for (const auto& index : toDeletedIndices) {
      bytes b_index =
          DataConversion::StringToCharArray(RemoveAddrFromKey(index));
      if (m_permTrie.contains(b_index)) {
        if (LOG_SC) {
          LOG_GENERAL(INFO, "delete key: " << index);
        }
        m_permTrie.remove(b_index);
      }
    }

    if (revertible) {
      m_permADMap->stop_recording();
    }

    return m_permTrie.root();
  }
}

bool ContractStorage2::CheckHasMap(const dev::h160& addr, bool temp) {
  std::map<std::string, bytes> t_hasMap;
  std::string hasMap_key = GenerateStorageKey(addr, HAS_MAP_INDICATOR, {});

  FetchStateDataForKey(t_hasMap, hasMap_key, temp);

  if (t_hasMap.empty()) {
    LOG_GENERAL(WARNING, "Failed to fetch hasMap for addr: " << addr.hex());
  } else {
    try {
      return DataConversion::CharArrayToString(t_hasMap[hasMap_key]) == "true";
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Invalid hasMap: " << hasMap_key << endl
                                              << e.what());
    }
  }

  return false;
}

dev::h256 ContractStorage2::GetContractStateHashForMergingDelta(
    const dev::h160& addr, const dev::h256& root,
    const std::map<std::string, bytes>& states,
    const std::vector<std::string>& toDeleteIndices) {
  if (IsNullAddress(addr)) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return dev::h256();
  }

  if (CheckHasMap(addr, false)) {
    // only use the provided data to populate the merkle trie
    return UpdateContractTrie(root, states, toDeleteIndices, false, true);
  } else {
    std::map<std::string, bytes> states;
    FetchStateDataForContract(states, addr, "", {}, false);
    return DirectHashState(states);
  }
}

dev::h256 ContractStorage2::GetContractStateHash(const dev::h160& addr,
                                                 const dev::h256& root,
                                                 bool temp, bool revertible) {
  if (IsNullAddress(addr)) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return dev::h256();
  }

  std::map<std::string, bytes> states;
  std::vector<std::string> toDeleteIndices;

  if (CheckHasMap(addr, temp)) {
    FetchUpdatedStateValuesForAddress(addr, states, toDeleteIndices, temp);
    return UpdateContractTrie(root, states, toDeleteIndices, temp, revertible);
  } else {
    FetchStateDataForContract(states, addr, "", {}, temp);
    return DirectHashState(states);
  }
}

void ContractStorage2::Reset() {
  {
    lock_guard<mutex> g(m_codeMutex);
    m_codeDB.ResetDB();
  }
  {
    lock_guard<mutex> g(m_initDataMutex);
    m_initDataDB.ResetDB();
  }
  {
    lock_guard<mutex> g(m_stateDataMutex);
    m_stateDataDB.ResetDB();

    p_stateDataMap.clear();
    p_indexToBeDeleted.clear();

    t_stateDataMap.clear();
    t_indexToBeDeleted.clear();

    r_stateDataMap.clear();
    r_indexToBeDeleted.clear();

    m_stateDataMap.clear();
    m_indexToBeDeleted.clear();
  }
  {
    lock_guard<mutex> g(m_stateMPTMutex);
    mp_stateDataDB->ResetDB();
    m_tempADMap->reset();
    m_permADMap->reset();
  }
}

bool ContractStorage2::RefreshAll() {
  bool ret;
  {
    lock_guard<mutex> g(m_codeMutex);
    ret = m_codeDB.RefreshDB();
  }
  if (ret) {
    lock_guard<mutex> g(m_initDataMutex);
    ret = m_initDataDB.RefreshDB();
  }
  if (ret) {
    lock_guard<mutex> g(m_stateDataMutex);
    ret = m_stateDataDB.RefreshDB();
    ret = ret && mp_stateDataDB->RefreshDB();
  }

  return ret;
}

}  // namespace Contract
