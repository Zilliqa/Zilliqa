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

#include "ContractStorage.h"

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

ContractStorage::ContractStorage()
    : m_codeDB("contractCode"),
      m_initDataDB("contractInitState2"),
      m_trieDB("contractStateTrieDB"),
      m_stateTrie(&m_trieDB) {}

// Code
// ======================================

bool ContractStorage::PutContractCode(const dev::h160& address,
                                      const bytes& code) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.Insert(address.hex(), code) == 0;
}

bool ContractStorage::PutContractCodeBatch(
    const unordered_map<string, string>& batch) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.BatchInsert(batch);
}

bytes ContractStorage::GetContractCode(const dev::h160& address) {
  shared_lock<shared_timed_mutex> g(m_codeMutex);
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage::DeleteContractCode(const dev::h160& address) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.DeleteKey(address.hex()) == 0;
}

// InitData
// ========================================
bool ContractStorage::PutInitData(const dev::h160& address,
                                  const bytes& initData) {
  unique_lock<shared_timed_mutex> g(m_initDataMutex);
  return m_initDataDB.Insert(address.hex(), initData) == 0;
}

bool ContractStorage::PutInitDataBatch(
    const unordered_map<string, string>& batch) {
  unique_lock<shared_timed_mutex> g(m_initDataMutex);
  return m_initDataDB.BatchInsert(batch);
}

bytes ContractStorage::GetInitData(const dev::h160& address) {
  shared_lock<shared_timed_mutex> g(m_initDataMutex);
  return DataConversion::StringToCharArray(m_initDataDB.Lookup(address.hex()));
}

bool ContractStorage::DeleteInitData(const dev::h160& address) {
  unique_lock<shared_timed_mutex> g(m_initDataMutex);
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

string ContractStorage::GenerateStorageKey(const string& vname,
                                           const vector<string>& indices) {
  string ret;
  if (!vname.empty()) {
    ret = vname + SCILLA_INDEX_SEPARATOR;
  }
  for (const auto& index : indices) {
    ret += index + SCILLA_INDEX_SEPARATOR;
  }
  return ret;
}

bool ContractStorage::FetchStateValue(const dev::h160& addr,
                                      const dev::h256& rootHash,
                                      const bytes& src, unsigned int s_offset,
                                      bytes& dst, unsigned int d_offset,
                                      bool& foundVal, bool getType,
                                      string& type, bool reloadRootHash) {
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
      query.name() == MAP_DEPTH_INDICATOR || query.name() == TYPE_INDICATOR) {
    LOG_GENERAL(WARNING, "invalid query: " << query.name());
    return false;
  }

  shared_lock<shared_timed_mutex> g(m_stateDataMutex);

  if (reloadRootHash) {
    if (rootHash == dev::h256()) {
      m_stateTrie.init();
    } else {
      try {
        m_stateTrie.setRoot(rootHash);
      } catch (...) {
        LOG_GENERAL(WARNING, "root not found");
        return false;
      }
    }
  }

  if (getType) {
    std::map<std::string, bytes> t_type;
    std::string type_key = GenerateStorageKey(TYPE_INDICATOR, {query.name()});
    FetchStateDataForKey(t_type, addr, type_key, true);
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

  string key = query.name() + SCILLA_INDEX_SEPARATOR;

  ProtoScillaVal value;

  for (const auto& index : query.indices()) {
    key += index + SCILLA_INDEX_SEPARATOR;
  }

  if ((unsigned int)query.indices().size() > query.mapdepth()) {
    LOG_GENERAL(WARNING, "indices is deeper than map depth");
    return false;
  }

  if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
    auto d_found = t_indexToBeDeleted[addr].find(key);
    if (d_found != t_indexToBeDeleted[addr].end()) {
      // ignore the deleted empty placeholder
      if ((unsigned int)query.indices().size() == query.mapdepth()) {
        foundVal = false;
        return true;
      }
    }
  }

  if ((unsigned int)query.indices().size() == query.mapdepth()) {
    // result will not be a map and can be just fetched into the store
    bytes bval;
    bool found = false;

    if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
      const auto& t_found = t_stateDataMap[addr].find(key);
      if (t_found != t_stateDataMap[addr].end()) {
        bval = t_found->second;
        found = true;
      }
    }
    if (!found) {
      std::string sval = m_stateTrie.at(dev::bytesConstRef(key));

      if (!sval.empty()) {
        if (query.ignoreval()) {
          return true;
        }
        bval = DataConversion::StringToCharArray(sval);
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

  unordered_map<string, bytes> entries;

  // We're fetching a Map value. Need to iterate level-db lexicographically
  // first fetch from t_data, then m_data, lastly db
  if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
    std::map<std::string, bytes>::iterator p;
    if (!key.empty()) {
      p = t_stateDataMap[addr].lower_bound(key);
    } else {
      p = t_stateDataMap[addr].begin();
    }

    while (p != t_stateDataMap[addr].end() &&
           p->first.compare(0, key.size(), key) == 0) {
      if (query.ignoreval()) {
        return true;
      }
      entries.emplace(p->first, p->second);
      ++p;
    }
  }

  dev::GenericTrieDB<TraceableDB>::iterator p2;
  if (!key.empty()) {
    p2 = m_stateTrie.lower_bound(dev::bytesConstRef(key));
  } else {
    p2 = m_stateTrie.begin();
  }

  while (p2 != m_stateTrie.end() &&
         p2.at().first.toString().compare(0, key.size(), key) == 0) {
    if (query.ignoreval()) {
      return true;
    }
    auto exist = entries.find(p2.at().first.toString());
    if (exist == entries.end()) {
      entries.emplace(p2.at().first.toString(), p2.at().second.toBytes());
    }
    ++p2;
  }

  set<string>::iterator isDeleted;

  uint32_t counter = 0;

  for (const auto& entry : entries) {
    if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
      isDeleted = t_indexToBeDeleted[addr].find(entry.first);
      if (isDeleted != t_indexToBeDeleted[addr].end()) {
        continue;
      }
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

// bool ContractStorage::FetchExternalStateValue(
//     const dev::h160& caller, const dev::h256& callerRootHash,
//     const dev::h160& target, const dev::h256& targetRootHash, const bytes&
//     src, unsigned int s_offset, bytes& dst, unsigned int d_offset, bool&
//     foundVal, string& type, uint32_t caller_version) {
//   // get caller version if not available
//   shared_lock<shared_timed_mutex> g(m_stateDataMutex);

//   if (caller_version == std::numeric_limits<uint32_t>::max()) {
//     std::map<std::string, bytes> t_caller_version;
//     string version_key = GenerateStorageKey(SCILLA_VERSION_INDICATOR, {});
//     if (callerRootHash == dev::h256()) {
//       m_stateTrie.init();
//     } else {
//       try {
//         m_stateTrie.setRoot(callerRootHash);
//       } catch (...) {
//         LOG_GENERAL(WARNING, "root not found");
//         return false;
//       }
//     }
//     FetchStateDataForKey(t_caller_version, caller, version_key, true);
//     if (t_caller_version.empty()) {
//       return false;
//     }
//     try {
//       caller_version = std::stoul(
//           DataConversion::CharArrayToString(t_caller_version[version_key]));
//     } catch (const std::exception& e) {
//       LOG_GENERAL(WARNING, "invalid caller_version " << version_key << endl
//                                                      << e.what());
//       return false;
//     }
//   }

//   // get target version if not available
//   std::map<std::string, bytes> t_target_version;
//   string version_key = GenerateStorageKey(SCILLA_VERSION_INDICATOR, {});
//   if (targetRootHash == dev::h256()) {
//     m_stateTrie.init();
//   } else {
//     try {
//       m_stateTrie.setRoot(targetRootHash);
//     } catch (...) {
//       LOG_GENERAL(WARNING, "root not found");
//       return false;
//     }
//   }
//   FetchStateDataForKey(t_target_version, target, version_key, true);
//   if (t_target_version.empty()) {
//     return false;
//   }

//   uint32_t target_version;
//   try {
//     target_version = std::stoul(
//         DataConversion::CharArrayToString(t_target_version[version_key]));
//   } catch (const std::exception& e) {
//     LOG_GENERAL(WARNING, "invalid target_version: " << version_key << endl
//                                                     << e.what());
//     return false;
//   }

//   if (target_version != caller_version) {
//     LOG_GENERAL(WARNING, "Caller(" << caller_version << ") target("
//                                    << target_version << ") version
//                                    mismatch");
//     return false;
//   }

//   // get value
//   return FetchStateValue(target, targetRootHash, src, s_offset, dst,
//   d_offset,
//                          foundVal, true, type, false);
// }

void ContractStorage::DeleteByPrefix(const dev::h160& addr,
                                     const string& prefix) {
  if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
    std::map<std::string, bytes>::iterator p;
    if (!prefix.empty()) {
      p = t_stateDataMap[addr].lower_bound(prefix);
    } else {
      p = t_stateDataMap[addr].begin();
    }
    while (p != t_stateDataMap[addr].end() &&
           p->first.compare(0, prefix.size(), prefix) == 0) {
      p_indexToBeDeleted[addr].emplace(p->first, true);  // for reverting
      t_indexToBeDeleted[addr].emplace(p->first);
      ++p;
    }
  }

  dev::GenericTrieDB<TraceableDB>::iterator p2;
  if (!prefix.empty()) {
    p2 = m_stateTrie.lower_bound(dev::bytesConstRef(prefix));
  } else {
    p2 = m_stateTrie.begin();
  }
  while (p2 != m_stateTrie.end() &&
         p2.at().first.toString().compare(0, prefix.size(), prefix) == 0) {
    p_indexToBeDeleted[addr].emplace(p2.at().first.toString(),
                                     true);  // for reverting
    t_indexToBeDeleted[addr].emplace(p2.at().first.toString());
    ++p2;
  }
}

void ContractStorage::DeleteByIndex(const dev::h160& addr,
                                    const string& index) {
  if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
    if (t_stateDataMap[addr].find(index) == t_stateDataMap[addr].end()) {
      // didn't found in t_stateDataMap
      std::string sval = m_stateTrie.at(dev::bytesConstRef(index));
      if (sval.empty()) {
        // still not found;
        return;
      }
    }
  }

  p_indexToBeDeleted[addr].emplace(index, true);  // for reverting
  t_indexToBeDeleted[addr].emplace(index);
}

void ContractStorage::UnquoteString(string& input) {
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

void ContractStorage::InsertValueToStateJson(Json::Value& _json, string key,
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

bool ContractStorage::FetchStateJsonForContract(
    Json::Value& _json, const dev::h160& addr, const dev::h256& rootHash,
    const string& vname, const vector<string>& indices, bool temp) {
  shared_lock<shared_timed_mutex> g(m_stateDataMutex);

  if (rootHash == dev::h256()) {
    m_stateTrie.init();
  } else {
    try {
      m_stateTrie.setRoot(rootHash);
    } catch (...) {
      LOG_GENERAL(WARNING, "root not found");
      return false;
    }
  }

  std::map<std::string, bytes> states;
  FetchStateDataForContract(states, addr, vname, indices, temp);

  for (const auto& state : states) {
    vector<string> fragments;
    boost::split(fragments, state.first,
                 bind1st(std::equal_to<char>(), SCILLA_INDEX_SEPARATOR));
    if (fragments.back().empty()) fragments.pop_back();

    string vname = fragments.at(0);

    if (vname == CONTRACT_ADDR_INDICATOR || vname == SCILLA_VERSION_INDICATOR ||
        vname == MAP_DEPTH_INDICATOR || vname == TYPE_INDICATOR) {
      continue;
    }

    /// addr+vname+[indices...]
    vector<string> map_indices(fragments.begin() + 1, fragments.end());

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
    string map_depth_key = GenerateStorageKey(MAP_DEPTH_INDICATOR, {vname});
    FetchStateDataForKey(map_depth, addr, map_depth_key, temp);

    jsonMapWrapper(_json[vname], map_indices, state.second, 0,
                   !map_depth.empty()
                       ? std::stoi(DataConversion::CharArrayToString(
                             map_depth[map_depth_key]))
                       : -1);
  }

  return true;
}

bool ContractStorage::FetchStateProofForContract(
    std::set<string>& proof, const dev::h256& rootHash,
    const std::vector<std::pair<std::string, std::vector<std::string>>>& keys) {
  shared_lock<shared_timed_mutex> g(m_stateDataMutex);

  if (rootHash == dev::h256()) {
    return false;
  } else {
    try {
      m_stateTrie.setRoot(rootHash);
    } catch (...) {
      LOG_GENERAL(WARNING, "root not found");
      return false;
    }
  }

  for (const auto& key : keys) {
    string keyStr = GenerateStorageKey(key.first, key.second);
    FetchProofForKey(proof, keyStr);
  }

  return true;
}

void ContractStorage::FetchProofForKey(std::set<string>& proof,
                                       const string& key) {
  std::set<std::string> t_keys;
  dev::GenericTrieDB<TraceableDB>::iterator p;
  if (!key.empty()) {
    p = m_stateTrie.lower_bound(dev::bytesConstRef(key));
  } else {
    p = m_stateTrie.begin();
  }
  while (p != m_stateTrie.end()) {
    // TODO: remove metadata
    string t_key = p.at().first.toString();
    if (t_key.compare(0, key.size(), key) != 0) {
      break;
    }
    t_keys.emplace(std::move(t_key));
    ++p;
  }

  for (const auto& k : t_keys) {
    m_stateTrie.getProof(dev::bytesConstRef(k), proof);
  }
}

void ContractStorage::FetchStateDataForKey(map<string, bytes>& states,
                                           const dev::h160& addr,
                                           const string& key, bool temp) {
  std::map<std::string, bytes>::iterator p;
  if (temp) {
    if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
      if (!key.empty()) {
        p = t_stateDataMap[addr].lower_bound(key);
      } else {
        p = t_stateDataMap[addr].begin();
      }
      while (p != t_stateDataMap[addr].end() &&
             p->first.compare(0, key.size(), key) == 0) {
        states.emplace(p->first, p->second);
        ++p;
      }
    }
  }

  dev::GenericTrieDB<TraceableDB>::iterator p2;
  if (!key.empty()) {
    p2 = m_stateTrie.lower_bound(dev::bytesConstRef(key));
  } else {
    p2 = m_stateTrie.begin();
  }

  while (p2 != m_stateTrie.end() &&
         p2.at().first.toString().compare(0, key.size(), key) == 0) {
    if (states.find(p2.at().first.toString()) == states.end()) {
      states.emplace(p2.at().first.toString(), p2.at().second.toBytes());
    }
    ++p2;
  }

  if (temp) {
    if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
      for (auto it = states.begin(); it != states.end();) {
        if (t_indexToBeDeleted[addr].find(it->first) !=
            t_indexToBeDeleted[addr].cend()) {
          it = states.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
}

void ContractStorage::FetchStateDataForContract(map<string, bytes>& states,
                                                const dev::h160& addr,
                                                const string& vname,
                                                const vector<string>& indices,
                                                bool temp) {
  string key = GenerateStorageKey(vname, indices);
  FetchStateDataForKey(states, addr, key, temp);
}

bool ContractStorage::FetchUpdatedStateValuesForAddr(
    const dev::h160& addr, const dev::h256& rootHash,
    map<string, bytes>& t_states, set<std::string>& toDeletedIndices,
    bool temp) {
  if (addr == dev::h160()) {
    LOG_GENERAL(WARNING, "address provided is empty");
    return false;
  }

  if (temp) {
    if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
      t_states = t_stateDataMap[addr];
    }

    if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
      toDeletedIndices = t_indexToBeDeleted[addr];
    }
  } else {
    if (rootHash == dev::h256()) {
      m_stateTrie.init();
    } else {
      try {
        m_stateTrie.setRoot(rootHash);
      } catch (...) {
        LOG_GENERAL(WARNING, "root not found");
        return false;
      }
    }
    for (const auto& iter : m_stateTrie) {
      t_states.emplace(iter.first.toString(), iter.second.toBytes());
    }
  }

  return true;
}

bool ContractStorage::CleanEmptyMapPlaceholders(const dev::h160& addr,
                                                const string& key) {
  // key = 0xabc.vname.[index1.index2.[...].indexn.
  vector<string> indices;
  boost::split(indices, key,
               bind1st(std::equal_to<char>(), SCILLA_INDEX_SEPARATOR));
  if (indices.size() < 1) {
    LOG_GENERAL(WARNING, "indices size too small: " << indices.size());
    return false;
  }
  if (indices.back().empty()) indices.pop_back();

  string scankey = indices.at(0) + SCILLA_INDEX_SEPARATOR;
  DeleteByIndex(addr, scankey);  // clean root level

  for (unsigned int i = 1; i < indices.size() - 1 /*exclude the value key*/;
       ++i) {
    scankey += indices.at(i) + SCILLA_INDEX_SEPARATOR;
    DeleteByIndex(addr, scankey);
  }
  return true;
}

void ContractStorage::UpdateStateData(const dev::h160& addr, const string& key,
                                      const bytes& value, bool cleanEmpty) {
  if (LOG_SC) {
    LOG_GENERAL(INFO, "key: " << key << " value: "
                              << DataConversion::CharArrayToString(value));
  }

  if (cleanEmpty) {
    CleanEmptyMapPlaceholders(addr, key);
  }

  if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
    auto pos = t_indexToBeDeleted[addr].find(key);
    if (pos != t_indexToBeDeleted[addr].end()) {
      t_indexToBeDeleted[addr].erase(pos);
      // for reverting
      p_indexToBeDeleted[addr].emplace(key, false);
    }
  }

  // for reverting
  if (t_stateDataMap.find(addr) != t_stateDataMap.end()) {
    if (t_stateDataMap[addr].find(key) != t_stateDataMap[addr].end()) {
      p_stateDataMap[addr][key] = t_stateDataMap[addr][key];
    } else {
      p_stateDataMap[addr][key] = {};
    }
  }

  t_stateDataMap[addr][key] = value;
}

bool ContractStorage::UpdateStateValue(const dev::h160& addr,
                                       const dev::h256& rootHash,
                                       const bytes& q, unsigned int q_offset,
                                       const bytes& v, unsigned int v_offset) {
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
      query.name() == MAP_DEPTH_INDICATOR || query.name() == TYPE_INDICATOR) {
    LOG_GENERAL(WARNING, "invalid query: " << query.name());
    return false;
  }

  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  if (rootHash == dev::h256()) {
    m_stateTrie.init();
  } else {
    try {
      m_stateTrie.setRoot(rootHash);
    } catch (...) {
      LOG_GENERAL(WARNING, "root not found");
      return false;
    }
  }

  string key = query.name() + SCILLA_INDEX_SEPARATOR;

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
    DeleteByPrefix(addr, key);

    map<string, bytes> t_states;
    FetchStateDataForKey(t_states, addr, parent_key, true);
    if (t_states.empty()) {
      ProtoScillaVal empty_val;
      empty_val.mutable_mval()->mutable_m();
      bytes dst;
      if (!SerializeToArray(empty_val, dst, 0)) {
        LOG_GENERAL(WARNING, "empty_mval SerializeToArray failed");
        return false;
      }
      UpdateStateData(addr, parent_key, dst);
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
      UpdateStateData(addr, key,
                      DataConversion::StringToCharArray(value.bval()), true);
      return true;
    } else {
      DeleteByPrefix(addr, key);

      std::function<bool(const dev::h160&, const dev::h256&, const string&,
                         const ProtoScillaVal&)>
          mapHandler = [&](const dev::h160& addr, const dev::h256& rootHash,
                           const string& keyAcc,
                           const ProtoScillaVal& value) -> bool {
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
          UpdateStateData(addr, keyAcc, dst, true);
          return true;
        }
        for (const auto& entry : value.mval().m()) {
          string index(keyAcc);
          index += entry.first + SCILLA_INDEX_SEPARATOR;
          if (entry.second.has_mval()) {
            // We haven't reached the deepeast nesting
            if (!mapHandler(addr, rootHash, index, entry.second)) {
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
                addr, index,
                DataConversion::StringToCharArray(entry.second.bval()), true);
          }
        }
        return true;
      };

      return mapHandler(addr, rootHash, key, value);
    }
  }
  return true;
}

bool ContractStorage::UpdateStateDatasAndToDeletes(
    const dev::h160& addr, const dev::h256& rootHash,
    const std::map<std::string, bytes>& states,
    const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
    bool temp, bool revertible) {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  if (temp) {
    for (const auto& state : states) {
      t_stateDataMap[addr][state.first] = state.second;

      if (t_indexToBeDeleted.find(addr) != t_indexToBeDeleted.end()) {
        auto pos = t_indexToBeDeleted[addr].find(state.first);
        if (pos != t_indexToBeDeleted[addr].end()) {
          t_indexToBeDeleted[addr].erase(pos);
        }
      }
    }
    for (const auto& index : toDeleteIndices) {
      t_indexToBeDeleted[addr].emplace(index);
    }
    if (t_indexToBeDeleted[addr].empty()) {
      t_indexToBeDeleted.erase(addr);
    }
  } else {
    if (rootHash == dev::h256()) {
      m_stateTrie.init();
    } else {
      try {
        m_stateTrie.setRoot(rootHash);
      } catch (...) {
        LOG_GENERAL(WARNING, "root not found");
        return false;
      }
    }

    std::unordered_map<std::string, bytes> t_r_stateDataMap;
    for (const auto& state : states) {
      if (revertible) {
        std::string sval = m_stateTrie.at(dev::bytesConstRef(state.first));
        if (!sval.empty()) {
          t_r_stateDataMap[state.first] =
              DataConversion::StringToCharArray(sval);
        } else {
          t_r_stateDataMap[state.first] = {};
        }
      }
      m_stateTrie.insert(dev::bytesConstRef(state.first), state.second);
    }
    for (const auto& toDelete : toDeleteIndices) {
      if (revertible) {
        std::string sval = m_stateTrie.at(dev::bytesConstRef(toDelete));
        t_r_stateDataMap[toDelete] = DataConversion::StringToCharArray(sval);
      }
      m_stateTrie.remove(dev::bytesConstRef(toDelete));
    }
    stateHash = m_stateTrie.root();
    r_stateDataMap[m_stateTrie.root()] = t_r_stateDataMap;
  }
  return true;
}

void ContractStorage::ResetBufferedAtomicState() {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);
  p_stateDataMap.clear();
  p_indexToBeDeleted.clear();
}

void ContractStorage::RevertAtomicState() {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  for (const auto& entry : p_stateDataMap) {
    for (const auto& data : entry.second) {
      if (data.second.empty()) {
        t_stateDataMap[entry.first].erase(data.first);
      } else {
        t_stateDataMap[entry.first][data.first] = data.second;
      }
    }
    if (t_stateDataMap[entry.first].empty()) {
      t_stateDataMap.erase(entry.first);
    }
  }

  for (const auto& entry : p_indexToBeDeleted) {
    for (const auto& index : entry.second) {
      if (index.second) {
        // revert newly added indexToBeDeleted
        if (t_indexToBeDeleted.find(entry.first) != t_indexToBeDeleted.end()) {
          const auto& found = t_indexToBeDeleted[entry.first].find(index.first);
          if (found != t_indexToBeDeleted[entry.first].end()) {
            t_indexToBeDeleted[entry.first].erase(found);
          }
        }
      } else {
        // revert newly deleted indexToBeDeleted
        t_indexToBeDeleted[entry.first].emplace(index.first);
      }
    }
    if (t_indexToBeDeleted[entry.first].empty()) {
      t_indexToBeDeleted.erase(entry.first);
    }
  }
}

bool ContractStorage::RevertContractStates() {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  for (const auto& entry : r_stateDataMap) {
    if (entry.first == dev::h256()) {
      m_stateTrie.init();
    } else {
      try {
        m_stateTrie.setRoot(entry.first);
      } catch (...) {
        LOG_GENERAL(WARNING, "root not found");
        return false;
      }
    }

    for (const auto& data : entry.second) {
      if (data.second.empty()) {
        m_stateTrie.remove(dev::bytesConstRef(data.first));
      } else {
        m_stateTrie.insert(dev::bytesConstRef(data.first), data.second);
      }
    }
  }
  return true;
}

void ContractStorage::InitRevertibles() {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);
  r_stateDataMap.clear();
}

bool ContractStorage::CommitStateDB(const uint64_t& dsBlockNum) {
  {
    unique_lock<shared_timed_mutex> g(m_stateDataMutex);
    m_stateTrie.db()->commit(dsBlockNum);
  }

  InitTempState();

  return true;
}

void ContractStorage::InitTempState() {
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  t_stateDataMap.clear();
  t_indexToBeDeleted.clear();
}

void ContractStorage::Reset() {
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    m_codeDB.ResetDB();
  }
  {
    unique_lock<shared_timed_mutex> g(m_initDataMutex);
    m_initDataDB.ResetDB();
  }
  {
    unique_lock<shared_timed_mutex> g(m_stateDataMutex);

    p_stateDataMap.clear();
    p_indexToBeDeleted.clear();

    t_stateDataMap.clear();
    t_indexToBeDeleted.clear();

    r_stateDataMap.clear();

    m_stateTrie.init();
    m_trieDB.ResetDB();
  }
}

bool ContractStorage::RefreshAll() {
  bool ret;
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    ret = m_codeDB.RefreshDB();
  }
  if (ret) {
    unique_lock<shared_timed_mutex> g(m_initDataMutex);
    ret = m_initDataDB.RefreshDB();
  }
  if (ret) {
    unique_lock<shared_timed_mutex> g(m_stateDataMutex);
    ret = m_trieDB.RefreshDB();
  }

  return ret;
}

}  // namespace Contract
