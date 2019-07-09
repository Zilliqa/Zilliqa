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

#include "ScillaMessage.pb.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

#include <bits/stdc++.h>
#include <boost/algorithm/string.hpp>

using namespace std;
using namespace ZilliqaMessage;

namespace Contract {
// Code
// ======================================

bool ContractStorage2::PutContractCode(const dev::h160& address,
                                       const bytes& code) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.Insert(address.hex(), code) == 0;
}

bool ContractStorage2::PutContractCodeBatch(
    const unordered_map<string, string>& batch) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.BatchInsert(batch);
}

const bytes ContractStorage2::GetContractCode(const dev::h160& address) {
  shared_lock<shared_timed_mutex> g(m_codeMutex);
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage2::DeleteContractCode(const dev::h160& address) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.DeleteKey(address.hex()) == 0;
}

// InitData
// ========================================
bool ContractStorage2::PutInitData(const dev::h160& address,
                                   const bytes& initData) {
  unique_lock<shared_timed_mutex> g(m_initDataMutex);
  return m_initDataDB.Insert(address.hex(), initData) == 0;
}

bool ContractStorage2::PutInitDataBatch(
    const unordered_map<string, string>& batch) {
  unique_lock<shared_timed_mutex> g(m_initDataMutex);
  return m_initDataDB.BatchInsert(batch);
}

const bytes ContractStorage2::GetInitData(const dev::h160& address) {
  shared_lock<shared_timed_mutex> g(m_initDataMutex);
  return DataConversion::StringToCharArray(m_initDataDB.Lookup(address.hex()));
}

bool ContractStorage2::DeleteInitData(const dev::h160& address) {
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

bool ContractStorage2::FetchStateValue(const dev::h160& addr, const bytes& src,
                                       unsigned int s_offset, bytes& dst,
                                       unsigned int d_offset) {
  LOG_MARKER();

  if (s_offset >= src.size()) {
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

  string key = addr.hex() + "." + query.name();

  ProtoScillaVal value;

  for (const auto& index : query.indices()) {
    key += "." + index;
  }

  if ((unsigned int)query.indices().size() > query.mapdepth()) {
    LOG_GENERAL(WARNING, "indices is deeper than map depth");
    return false;
  }

  if ((unsigned int)query.indices().size() == query.mapdepth()) {
    // result will not be a map and can be just fetched into the store
    bytes bval;  //= GetValue(key); [TODO]
    value.set_bval(bval.data(), bval.size());
    return SerializeToArray(value, dst, 0);
  }

  // We're fetching a Map value. Need to iterate level-db lexicographically
  // first fetch from t_data, then m_data, lastly db
  auto p = t_stateDataMap.lower_bound(key);

  unordered_map<string, bytes> entries;

  while (p != t_stateDataMap.end() &&
         p->first.compare(0, key.size(), key) == 0) {
    entries.emplace(p->first, p->second);
    ++p;
  }

  p = m_stateDataMap.lower_bound(key);

  while (p != m_stateDataMap.end() &&
         p->first.compare(0, key.size(), key) == 0) {
    auto exist = entries.find(p->first);
    if (exist != entries.end()) {
      entries.emplace(p->first, p->second);
    }
    ++p;
  }

  auto it = m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions());
  it->Seek({key});
  if (it->key().ToString().compare(0, key.size(), key) != 0) {
    // no entry
  } else {
    for (; it->key().ToString().compare(0, key.size(), key) == 0 && it->Valid();
         it->Next()) {
      auto exist = entries.find(it->key().ToString());
      if (exist != entries.end()) {
        bytes val(it->value().data(), it->value().data() + it->value().size());
        entries.emplace(it->key().ToString(), val);
      }
    }
  }

  set<string>::iterator isDeleted;

  for (const auto& entry : entries) {
    isDeleted = m_indexToBeDeleted.find(entry.first);
    if (isDeleted != m_indexToBeDeleted.end()) {
      continue;
    }

    std::vector<string> indices;
    // remove the prefixes, as shown below surrounded by []
    // [address.vname.index0.index1.(...).]indexN0.indexN1.(...).indexNn
    string key_non_prefix =
        entry.first.substr(key.size() + 1, entry.first.size());
    boost::split(indices, key_non_prefix, boost::is_any_of("."));
    unsigned int n = indices.size();
    ProtoScillaVal t_value;
    if (query.indices().size() + indices.size() < query.mapdepth()) {
      for (unsigned int i = 1; i < indices.size() - 1; ++i) {
        t_value =
            t_value.mutable_mval()->mutable_m()->operator[](indices.at(i));
      }
      t_value.mutable_mval()
          ->mutable_m()
          ->
          operator[](indices.back())
          .mutable_mval();
    } else {
      ProtoScillaVal t_value;
      for (unsigned int i = 1; i < indices.size() - 1; ++i) {
        t_value =
            t_value.mutable_mval()->mutable_m()->operator[](indices.at(i));
      }
      t_value.mutable_mval()
          ->mutable_m()
          ->
          operator[](indices.back())
          .set_bval(entry.second.data(), entry.second.size());
    }
    value.mutable_mval()->mutable_m()->operator[](indices.front()) = t_value;
  }

  return SerializeToArray(value, dst, 0);
}

void ContractStorage2::DeleteIndex(const string& prefix) {
  LOG_MARKER();

  auto p = t_stateDataMap.lower_bound(prefix);
  while (p != t_stateDataMap.end() &&
         p->first.compare(0, prefix.size(), prefix) == 0) {
    m_indexToBeDeleted.emplace(p->first);
    ++p;
  }

  p = m_stateDataMap.lower_bound(prefix);
  while (p != m_stateDataMap.end() &&
         p->first.compare(0, prefix.size(), prefix) == 0) {
    m_indexToBeDeleted.emplace(p->first);
    ++p;
  }
}

void ContractStorage2::FetchStateValueForAddress(
    const dev::h160& address, std::map<std::string, bytes>& states) {
  // LOG_MARKER();

  auto p = t_stateDataMap.lower_bound(address.hex());
  while (p != t_stateDataMap.end() &&
         p->first.compare(0, address.hex().size(), address.hex()) == 0) {
    states.emplace(p->first, p->second);
  }

  p = m_stateDataMap.lower_bound(address.hex());
  while (p != m_stateDataMap.end() &&
         p->first.compare(0, address.hex().size(), address.hex()) == 0) {
    if (states.find(p->first) == states.end()) {
      states.emplace(p->first, p->second);
    }
  }

  auto it = m_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions());
  it->Seek({address.hex()});
  if (it->key().ToString().compare(0, address.hex().size(), address.hex()) !=
      0) {
    // no entry
  } else {
    for (; it->key().ToString().compare(0, address.hex().size(),
                                        address.hex()) == 0 &&
           it->Valid();
         it->Next()) {
      if (states.find(it->key().ToString()) == states.end()) {
        bytes val(it->value().data(), it->value().data() + it->value().size());
        states.emplace(it->key().ToString(), val);
      }
    }
  }

  for (auto it = states.begin(); it != states.end();) {
    if (m_indexToBeDeleted.find(it->first) != m_indexToBeDeleted.cend()) {
      it = states.erase(it);
    } else {
      it++;
    }
  }
}

void ContractStorage2::FetchUpdatedStateValuesForAddress(
    const dev::h160& address, map<string, bytes>& t_states,
    vector<std::string>& toDeletedIndices) {
  if (address == dev::h160()) {
    LOG_GENERAL(WARNING, "address provided is empty");
    return;
  }
  auto p = t_stateDataMap.lower_bound(address.hex());
  while (p != t_stateDataMap.end() &&
         p->first.compare(0, address.hex().size(), address.hex()) == 0) {
    t_states.emplace(p->first, p->second);
  }

  auto r = m_indexToBeDeleted.lower_bound(address.hex());
  while (r != m_indexToBeDeleted.end() &&
         r->compare(0, address.hex().size(), address.hex()) == 0) {
    toDeletedIndices.emplace_back(*r);
  }
}

void ContractStorage2::UpdateStateData(const string& key, const bytes& value) {
  auto pos = m_indexToBeDeleted.find(key);
  if (pos != m_indexToBeDeleted.end()) {
    m_indexToBeDeleted.erase(pos);
  }

  t_stateDataMap[key] = value;
}

bool ContractStorage2::UpdateStateValue(const dev::h160& addr, const bytes& q,
                                        unsigned int q_offset, const bytes& v,
                                        unsigned int v_offset) {
  LOG_MARKER();

  if (q_offset >= q.size()) {
    LOG_GENERAL(WARNING, "Invalid query data and offset, data size "
                             << q.size() << ", offset " << q_offset);
    return false;
  }

  if (v_offset >= v.size()) {
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

  string key = addr.hex() + "." + query.name();

  // bytes keyReady;
  // copy(keyBase.begin(), keyBase.end(), keyReady.begin());

  for (const auto& index : query.indices()) {
    key += "." + index;
  }

  if ((unsigned int)query.indices().size() > query.mapdepth()) {
    LOG_GENERAL(WARNING, "indices is deeper than map depth");
    return false;
  } else if (query.deletemapkey()) {
    if (query.indices().size() < 1) {
      LOG_GENERAL(WARNING, "indices cannot be empty")
      return false;
    }
    DeleteIndex(key);
  } else if ((unsigned int)query.indices().size() == query.mapdepth()) {
    if (value.has_mval()) {
      LOG_GENERAL(WARNING, "val is not bytes but supposed to be");
      return false;
    }
    UpdateStateData(key, DataConversion::StringToCharArray(value.bval()));
    return true;
  } else {
    DeleteIndex(key);
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
        if (!SerializeToArray(value.mval(), dst, 0)) {
          return false;
        }
        // DB Put
        UpdateStateData(keyAcc, dst);
        return true;
      }
      for (const auto& entry : value.mval().m()) {
        string index(keyAcc);
        index += "." + entry.first;
        if (entry.second.has_mval()) {
          // We haven't reached the deepeast nesting
          return mapHandler(index, entry.second);
        } else {
          // DB Put
          UpdateStateData(
              index, DataConversion::StringToCharArray(entry.second.bval()));
        }
      }
      return true;
    };

    return mapHandler(key, value);
  }
  return true;
}

void ContractStorage2::UpdateStateDatasAndToDeletes(
    const dev::h160& addr, const std::map<std::string, bytes>& t_states,
    const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
    bool temp, bool revertible) {
  if (temp) {
    for (const auto& state : t_states) {
      t_stateDataMap[state.first] = state.second;
    }
    for (const auto& index : toDeleteIndices) {
      m_indexToBeDeleted.emplace(index);
    }
  } else {
    for (const auto& state : t_states) {
      if (revertible) {
        if (m_stateDataMap.find(state.first) != m_stateDataMap.end()) {
          r_stateDataMap[state.first] = m_stateDataMap[state.first];
        } else {
          r_stateDataMap[state.first] = {};
        }
      }
      m_stateDataMap[state.first] = state.second;
    }
    for (const auto& toDelete : toDeleteIndices) {
      if (revertible) {
        r_indexToBeDeleted.emplace(toDelete);
      }
      m_indexToBeDeleted.emplace(toDelete);
    }
  }

  stateHash = GetContractStateHash(addr, temp);
}

void ContractStorage2::BufferCurrentState() {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_stateDataMutex);
  p_stateDataMap = t_stateDataMap;
  p_indexToBeDeleted = m_indexToBeDeleted;
}

void ContractStorage2::RevertPrevState() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);
  t_stateDataMap = std::move(p_stateDataMap);
  m_indexToBeDeleted = std::move(p_indexToBeDeleted);
}

void ContractStorage2::RevertContractStates() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);

  for (const auto& data : r_stateDataMap) {
    if (data.second.empty()) {
      m_stateDataMap.erase(data.first);
    } else {
      m_stateDataMap[data.first] = data.second;
    }
  }

  for (const auto& index : r_indexToBeDeleted) {
    const auto& found = m_indexToBeDeleted.find(index);
    if (found != m_indexToBeDeleted.end()) {
      m_indexToBeDeleted.erase(found);
    }
  }
}

void ContractStorage2::InitRevertibles() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);
  r_stateDataMap.clear();
  r_indexToBeDeleted.clear();
}

bool ContractStorage2::CommitStateDB() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateDataMutex);
  // copy everything into m_stateXXDB;
  // Index
  unordered_map<string, std::string> batch;
  unordered_map<string, std::string> reset_buffer;

  batch.clear();
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

  m_stateDataMap.clear();
  m_indexToBeDeleted.clear();

  InitTempState();

  return true;
}

void ContractStorage2::InitTempState() {
  LOG_MARKER();

  t_stateDataMap.clear();
}

dev::h256 ContractStorage2::GetContractStateHash(const dev::h160& address,
                                                 bool temp) {
  if (address == Address()) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return dev::h256();
  }

  std::map<std::string, bytes> states;
  FetchStateValueForAddress(address, states);

  // iterate the raw protobuf string and hash
  SHA2<HashType::HASH_VARIANT_256> sha2;
  for (const auto& state : states) {
    sha2.Update(state.second);
  }
  return dev::h256(sha2.Finalize());
}

void ContractStorage2::Reset() {
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    m_codeDB.ResetDB();
  }
  {
    unique_lock<shared_timed_mutex> g(m_stateDataMutex);
    m_stateDataDB.ResetDB();
  }
}

bool ContractStorage2::RefreshAll() {
  bool ret;
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    ret = m_codeDB.RefreshDB();
  }
  if (ret) {
    unique_lock<shared_timed_mutex> g(m_stateDataMutex);
    ret = m_stateDataDB.RefreshDB();
  }
  return ret;
}

}  // namespace Contract