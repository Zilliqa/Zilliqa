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

#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

using namespace std;

namespace Contract {
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

const bytes ContractStorage::GetContractCode(const dev::h160& address) {
  shared_lock<shared_timed_mutex> g(m_codeMutex);
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage::DeleteContractCode(const dev::h160& address) {
  unique_lock<shared_timed_mutex> g(m_codeMutex);
  return m_codeDB.DeleteKey(address.hex()) == 0;
}

// State
// ========================================

Index GetIndex(const dev::h160& address, const string& key,
               unsigned int counter) {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(address.asBytes());
  sha2.Update(DataConversion::StringToCharArray(key));
  if (counter != 0) {
    sha2.Update(DataConversion::StringToCharArray(to_string(counter)));
  }
  return dev::h256(sha2.Finalize());
}

bool ContractStorage::CheckIndexExists(const Index& index) {
  shared_lock<shared_timed_mutex> g(m_stateIndexMutex);
  auto t_found = t_stateDataMap.find(index.hex());
  auto m_found = m_stateDataMap.find(index.hex());
  return t_found != t_stateDataMap.end() || m_found != m_stateDataMap.end() ||
         m_stateDataDB.Exists(index.hex());
}

Index ContractStorage::GetNewIndex(const dev::h160& address, const string& key,
                                   const vector<Index>& existing_indexes) {
  // LOG_MARKER();
  Index index;
  unsigned int counter = 0;

  do {
    index = GetIndex(address, key, counter);
    counter++;
  } while (find(existing_indexes.begin(), existing_indexes.end(), index) ==
               existing_indexes.end() &&
           CheckIndexExists(index));

  return index;
}

bool ContractStorage::PutContractState(const dev::h160& address,
                                       const vector<StateEntry>& states,
                                       dev::h256& stateHash, bool temp) {
  // LOG_MARKER();
  vector<pair<Index, bytes>> entries;

  vector<Index> entry_indexes = GetContractStateIndexes(address, true);

  for (const auto& state : states) {
    Index index = GetNewIndex(address, std::get<VNAME>(state), entry_indexes);

    bytes rawBytes;
    if (!Messenger::SetStateData(rawBytes, 0, state)) {
      LOG_GENERAL(WARNING, "Messenger::SetStateData failed");
      return false;
    }

    entries.emplace_back(index, rawBytes);
  }

  return PutContractState(address, entries, stateHash, temp, false,
                          entry_indexes, true);
}

bool ContractStorage::PutContractState(
    const dev::h160& address, const vector<pair<Index, bytes>>& entries,
    dev::h256& stateHash, bool temp, bool revertible,
    const vector<Index>& existing_indexes, bool provideExisting) {
  // LOG_MARKER();
  {
    unique_lock<shared_timed_mutex> g(m_stateMainMutex);

    if (address == Address()) {
      LOG_GENERAL(WARNING, "Null address rejected");
      return false;
    }

    vector<Index> entry_indexes;

    if (provideExisting) {
      entry_indexes = existing_indexes;
    } else {
      entry_indexes = GetContractStateIndexes(address, temp);
    }

    unordered_map<string, string> batch;

    for (const auto& entry : entries) {
      // Append the new index to the existing indexes
      if (find(entry_indexes.begin(), entry_indexes.end(), entry.first) ==
          entry_indexes.end()) {
        entry_indexes.emplace_back(entry.first);
      }

      if (temp) {
        t_stateDataMap[entry.first.hex()] = entry.second;
      } else {
        if (revertible) {
          r_stateDataMap[entry.first.hex()] = m_stateDataMap[entry.first.hex()];
        }
        m_stateDataMap[entry.first.hex()] = entry.second;
      }
    }

    // Update the stateIndexDB
    if (!SetContractStateIndexes(address, entry_indexes, temp, revertible)) {
      LOG_GENERAL(WARNING, "SetContractStateIndex failed");
      return false;
    }
  }

  stateHash = GetContractStateHash(address, temp);

  return true;
}

void ContractStorage::BufferCurrentState() {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_stateMainMutex);
  p_stateIndexMap = t_stateIndexMap;
  p_stateDataMap = t_stateDataMap;
}

void ContractStorage::RevertPrevState() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateMainMutex);
  t_stateIndexMap = std::move(p_stateIndexMap);
  t_stateDataMap = std::move(p_stateDataMap);
}

bool ContractStorage::SetContractStateIndexes(const dev::h160& address,
                                              const std::vector<Index>& indexes,
                                              bool temp, bool revertible) {
  // LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateIndexMutex);

  bytes rawBytes;
  if (!Messenger::SetStateIndex(rawBytes, 0, indexes)) {
    LOG_GENERAL(WARNING, "Messenger::SetStateIndex failed.");
    return false;
  }

  if (temp) {
    t_stateIndexMap[address.hex()] = rawBytes;
  } else {
    if (revertible) {
      r_stateIndexMap[address.hex()] = m_stateIndexMap[address.hex()];
    }
    m_stateIndexMap[address.hex()] = rawBytes;
  }

  return true;
}

void ContractStorage::RevertContractStates() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateMainMutex);

  for (const auto& acc : r_stateIndexMap) {
    if (acc.second.empty()) {
      m_stateIndexMap.erase(acc.first);
    } else {
      m_stateIndexMap[acc.first] = acc.second;
    }
  }
  for (const auto& data : r_stateDataMap) {
    if (data.second.empty()) {
      m_stateDataMap.erase(data.first);
    } else {
      m_stateDataMap[data.first] = data.second;
    }
  }
}

void ContractStorage::InitRevertibles() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateMainMutex);
  r_stateIndexMap.clear();
  r_stateDataMap.clear();
}

vector<Index> ContractStorage::GetContractStateIndexes(const dev::h160& address,
                                                       bool temp) {
  // get from stateIndexDB
  // return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
  // LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_stateIndexMutex);

  std::vector<Index> indexes;

  bytes rawBytes;

  auto t_found = t_stateIndexMap.find(address.hex());
  auto m_found = m_stateIndexMap.find(address.hex());
  if (temp && t_found != t_stateIndexMap.end()) {
    rawBytes = t_found->second;
  } else if (m_found != m_stateIndexMap.end()) {
    rawBytes = m_found->second;
  } else if (m_stateIndexDB.Exists(address.hex())) {
    std::string rawString = m_stateIndexDB.Lookup(address.hex());
    rawBytes = bytes(rawString.begin(), rawString.end());
  } else {
    return {};
  }

  if (!Messenger::GetStateIndex(rawBytes, 0, indexes)) {
    LOG_GENERAL(WARNING, "Messenger::GetStateIndex failed.");
    return {};
  }

  return indexes;
}

vector<bytes> ContractStorage::GetContractStatesData(const dev::h160& address,
                                                     bool temp) {
  // LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_stateMainMutex);
  // get indexes
  if (address == Address()) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return {};
  }

  vector<Index> indexes = GetContractStateIndexes(address, temp);

  vector<bytes> rawStates;

  // return vector of raw protobuf string
  for (const auto& index : indexes) {
    auto t_found = t_stateDataMap.find(index.hex());
    auto m_found = m_stateDataMap.find(index.hex());
    if (temp && t_found != t_stateDataMap.end()) {
      rawStates.push_back(t_found->second);
    } else if (m_found != m_stateDataMap.end()) {
      rawStates.push_back(m_found->second);
    } else if (m_stateDataDB.Exists(index.hex())) {
      std::string rawString = m_stateDataDB.Lookup(index.hex());
      rawStates.push_back(bytes(rawString.begin(), rawString.end()));
    } else {
      rawStates.push_back({});
    }
  }

  return rawStates;
}

string ContractStorage::GetContractStateData(const Index& index, bool temp) {
  // LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_stateDataMutex);
  auto t_found = t_stateDataMap.find(index.hex());
  auto m_found = m_stateDataMap.find(index.hex());
  if (temp && t_found != t_stateDataMap.end()) {
    return DataConversion::CharArrayToString(t_found->second);
  }
  if (m_found != m_stateDataMap.end()) {
    return DataConversion::CharArrayToString(m_found->second);
  }
  return m_stateDataDB.Lookup(index.hex());
}

bool ContractStorage::CommitStateDB() {
  LOG_MARKER();
  unique_lock<shared_timed_mutex> g(m_stateMainMutex);
  // copy everything into m_stateXXDB;
  // Index
  unordered_map<string, std::string> batch;
  unordered_map<string, std::string> reset_buffer;

  for (const auto& i : m_stateIndexMap) {
    batch.insert({i.first, DataConversion::CharArrayToString(i.second)});
    reset_buffer.insert({i.first, m_stateIndexDB.Lookup(i.first)});
  }

  if (!m_stateIndexDB.BatchInsert(batch)) {
    LOG_GENERAL(WARNING, "BatchInsert m_stateIndexDB failed");
    return false;
  }
  batch.clear();
  // Data
  for (const auto& i : m_stateDataMap) {
    batch.insert({i.first, DataConversion::CharArrayToString(i.second)});
  }
  if (!m_stateDataDB.BatchInsert(batch)) {
    LOG_GENERAL(WARNING, "BatchInsert m_stateDataDB failed");
    // Reset the values in m_stateIndexDB
    for (const auto& it : reset_buffer) {
      if (it.second.empty()) {
        if (m_stateIndexDB.DeleteKey(it.first) != 0) {
          LOG_GENERAL(WARNING,
                      "Something terrible happened, unable to clean the key in "
                      "m_stateIndexDB");
        }
      } else {
        if (m_stateIndexDB.Insert(
                it.first, DataConversion::StringToCharArray(it.second)) != 0) {
          LOG_GENERAL(WARNING,
                      "Something terrible happened, unable to reset the key in "
                      "m_stateIndexDB");
        }
      }
    }
    return false;
  }

  m_stateIndexMap.clear();
  m_stateDataMap.clear();

  InitTempState();

  return true;
}

void ContractStorage::InitTempState() {
  LOG_MARKER();

  t_stateIndexMap.clear();
  t_stateDataMap.clear();
}

bool ContractStorage::GetContractStateJson(
    const dev::h160& address, pair<Json::Value, Json::Value>& roots,
    uint32_t& scilla_version, bool temp) {
  // LOG_MARKER();

  if (address == Address()) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return false;
  }

  // iterate and deserialize the vector of raw protobuf string
  vector<bytes> rawStates = GetContractStatesData(address, temp);

  bool hasScillaVersion = false;
  pair<Json::Value, Json::Value> t_roots;
  try {
    for (const auto& rawState : rawStates) {
      StateEntry entry;
      uint32_t version;
      if (!Messenger::GetStateData(rawState, 0, entry, version)) {
        LOG_GENERAL(WARNING, "Messenger::GetStateData failed.");
        return false;
      }

      if (version != CONTRACT_STATE_VERSION) {
        LOG_GENERAL(WARNING, "state data version "
                                 << version
                                 << " is not match to CONTRACT_STATE_VERSION "
                                 << CONTRACT_STATE_VERSION);
        return false;
      }

      string tVname = std::get<VNAME>(entry);
      bool tMutable = std::get<MUTABLE>(entry);
      string tType = std::get<TYPE>(entry);
      string tValue = std::get<VALUE>(entry);

      if (!hasScillaVersion && tVname == "_scilla_version" &&
          tType == "Uint32" && !tMutable) {
        try {
          scilla_version = boost::lexical_cast<uint32_t>(tValue);
        } catch (...) {
          LOG_GENERAL(WARNING,
                      "_scilla_version " << tValue << " is not a number");
          return false;
        }

        hasScillaVersion = true;
      }

      Json::Value item;
      item["vname"] = tVname;
      item["type"] = tType;
      if (tValue[0] == '[' || tValue[0] == '{') {
        Json::Value obj;

        if (!JSONUtils::GetInstance().convertStrtoJson(tValue, obj)) {
          continue;
        }

        item["value"] = obj;
      } else {
        item["value"] = tValue;
      }

      if (!tMutable) {
        t_roots.first.append(item);
      } else {
        t_roots.second.append(item);
      }
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "Exception caught: " << e.what());
    return false;
  }

  if (!hasScillaVersion) {
    LOG_GENERAL(WARNING, "_scilla_version is not found in initData");
    return false;
  }

  roots = t_roots;

  return true;
}

dev::h256 ContractStorage::GetContractStateHash(const dev::h160& address,
                                                bool temp) {
  // LOG_MARKER();
  if (address == Address()) {
    LOG_GENERAL(WARNING, "Null address rejected");
    return dev::h256();
  }

  // iterate the raw protobuf string and hash
  vector<bytes> rawStates = GetContractStatesData(address, temp);
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  for (const auto& rawState : rawStates) {
    sha2.Update(rawState);
  }
  return dev::h256(sha2.Finalize());
}

void ContractStorage::Reset() {
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    m_codeDB.ResetDB();
  }
  {
    unique_lock<shared_timed_mutex> g(m_stateMainMutex);
    {
      unique_lock<shared_timed_mutex> g(m_stateIndexMutex);
      m_stateIndexDB.ResetDB();
    }
    {
      unique_lock<shared_timed_mutex> g(m_stateDataMutex);
      m_stateDataDB.ResetDB();
    }
  }
}

bool ContractStorage::RefreshAll() {
  bool ret;
  {
    unique_lock<shared_timed_mutex> g(m_codeMutex);
    ret = m_codeDB.RefreshDB();
  }
  if (ret) {
    unique_lock<shared_timed_mutex> g(m_stateMainMutex);
    {
      unique_lock<shared_timed_mutex> g(m_stateIndexMutex);
      ret = m_stateIndexDB.RefreshDB();
    }
    if (ret) {
      unique_lock<shared_timed_mutex> g(m_stateDataMutex);
      ret = m_stateDataDB.RefreshDB();
    }
  }
  return ret;
}

}  // namespace Contract