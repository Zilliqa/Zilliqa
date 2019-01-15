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

using namespace std;

namespace Contract {

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

bool ContractStorage::PutContractCode(const dev::h160& address,
                                      const bytes& code) {
  return m_codeDB.Insert(address.hex(), code) == 0;
}

bool ContractStorage::PutContractCodeBatch(
    const unordered_map<string, string>& batch) {
  return m_codeDB.BatchInsert(batch);
}

const bytes ContractStorage::GetContractCode(const dev::h160& address) {
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage::DeleteContractCode(const dev::h160& address) {
  return m_codeDB.DeleteKey(address.hex()) == 0;
}

bool ContractStorage::CheckIndexExists(const Index& index) {
  return false;
  return m_stateDataDB.Exists(index.hex()) || t_stateDataDB.Exists(index.hex());
}

Index ContractStorage::GetNewIndex(const dev::h160& address,
                                   const string& key) {
  // LOG_MARKER();
  Index index;
  unsigned int counter = 0;

  index = GetIndex(address, key, counter);
  // TODO: avoid index collision

  return index;
}

bool ContractStorage::PutContractState(const dev::h160& address,
                                       const vector<StateEntry>& states,
                                       dev::h256& stateHash) {
  // LOG_MARKER();
  vector<pair<Index, bytes>> entries;
  for (const auto& state : states) {
    Index index = GetNewIndex(address, std::get<VNAME>(state));

    bytes rawBytes;
    if (!Messenger::SetStateData(rawBytes, 0, state)) {
      LOG_GENERAL(WARNING, "Messenger::SetStateData failed");
      return false;
    }

    entries.emplace_back(index, rawBytes);
  }

  return PutContractState(address, entries, stateHash);
}

bool ContractStorage::PutContractState(
    const dev::h160& address, const vector<pair<Index, bytes>>& entries,
    dev::h256& stateHash) {
  LOG_MARKER();

  vector<Index> new_entry_indexes;

  unordered_map<string, string> batch;

  for (const auto& entry : entries) {
    // Append the new index to the existing indexes
    new_entry_indexes.emplace_back(entry.first);

    batch.insert(
        {entry.first.hex(), DataConversion::CharArrayToString(entry.second)});
  }

  if (!t_stateDataDB.BatchInsert(batch)) {
    LOG_GENERAL(WARNING, "BatchInsert t_stateDataDB failed");
    return false;
  }

  // Update the stateIndexDB
  if (!SetContractStateIndexes(address, new_entry_indexes)) {
    // for (const auto& index : new_entry_indexes) {
    //   t_stateDataDB.DeleteKey(index.hex());
    // }
    // TODO: revert the state data if failed
    return false;
  }

  stateHash = GetContractStateHash(address);

  return true;
}

bool ContractStorage::SetContractStateIndexes(
    const dev::h160& address, const std::vector<Index>& indexes) {
  // LOG_MARKER();

  bytes rawBytes;
  if (!Messenger::SetStateIndex(rawBytes, 0, indexes)) {
    LOG_GENERAL(WARNING, "Messenger::SetStateIndex failed.");
    return false;
  }

  return t_stateIndexDB.Insert(address.hex(), rawBytes) == 0;
}

vector<Index> ContractStorage::GetContractStateIndexes(
    const dev::h160& address) {
  // get from stateIndexDB
  // return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
  // LOG_MARKER();
  std::vector<Index> indexes;

  string rawBytes;

  if (t_stateIndexDB.Exists(address.hex())) {
    rawBytes = t_stateIndexDB.Lookup(address.hex());
  } else if (m_stateIndexDB.Exists(address.hex())) {
    rawBytes = m_stateIndexDB.Lookup(address.hex());
  } else {
    return {};
  }

  if (!Messenger::GetStateIndex(bytes(rawBytes.begin(), rawBytes.end()), 0,
                                indexes)) {
    LOG_GENERAL(WARNING, "Messenger::GetStateIndex failed.");
    return {};
  }

  return indexes;
}

vector<string> ContractStorage::GetContractStatesData(
    const dev::h160& address) {
  // LOG_MARKER();
  // get indexes
  vector<Index> indexes = GetContractStateIndexes(address);

  vector<string> rawStates;

  // return vector of raw protobuf string
  for (const auto& index : indexes) {
    if (t_stateDataDB.Exists(index.hex())) {
      rawStates.push_back(t_stateDataDB.Lookup(index.hex()));
      continue;
    }
    rawStates.push_back(m_stateDataDB.Lookup(index.hex()));
  }

  return rawStates;
}

string ContractStorage::GetContractStateData(const Index& index) {
  // LOG_MARKER();
  if (t_stateDataDB.Exists(index.hex())) {
    return t_stateDataDB.Lookup(index.hex());
  }
  return m_stateDataDB.Lookup(index.hex());
}

bool ContractStorage::CommitTempStateDB() {
  LOG_MARKER();
  // copy everything into m_stateXXDB;
  // Index
  unordered_map<string, std::string> batch;
  unordered_map<string, std::string> reset_buffer;
  leveldb::Iterator* it =
      t_stateIndexDB.GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    batch.insert({it->key().ToString(), it->value().ToString()});
    reset_buffer.insert(
        {it->key().ToString(), m_stateIndexDB.Lookup(it->key().ToString())});
  }
  if (!m_stateIndexDB.BatchInsert(batch)) {
    LOG_GENERAL(WARNING, "BatchInsert t_stateIndexDB failed");
    return false;
  }
  batch.clear();
  // Data
  it = t_stateDataDB.GetDB()->NewIterator(leveldb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    batch.insert({it->key().ToString(), it->value().ToString()});
  }
  if (!m_stateDataDB.BatchInsert(batch)) {
    LOG_GENERAL(WARNING, "BatchInsert t_stateDataDB failed");
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

  t_stateIndexDB.ResetDB();
  t_stateDataDB.ResetDB();

  return true;
}

Json::Value ContractStorage::GetContractStateJson(const dev::h160& address) {
  // LOG_MARKER();
  // iterate and deserialize the vector of raw protobuf string
  vector<string> rawStates = GetContractStatesData(address);

  Json::Value root;
  for (const auto& rawState : rawStates) {
    StateEntry entry;
    if (!Messenger::GetStateData(bytes(rawState.begin(), rawState.end()), 0,
                                 entry)) {
      LOG_GENERAL(WARNING, "Messenger::GetStateData failed.");
      continue;
    }

    string tVname = std::get<VNAME>(entry);
    bool tMutable = std::get<MUTABLE>(entry);
    string tType = std::get<TYPE>(entry);
    string tValue = std::get<VALUE>(entry);

    if (!tMutable) {
      continue;
    }

    Json::Value item;
    item["vname"] = tVname;
    item["type"] = tType;
    if (tValue[0] == '[' || tValue[0] == '{') {
      Json::CharReaderBuilder builder;
      unique_ptr<Json::CharReader> reader(builder.newCharReader());
      Json::Value obj;
      string errors;
      if (!reader->parse(tValue.c_str(), tValue.c_str() + tValue.size(), &obj,
                         &errors)) {
        LOG_GENERAL(WARNING,
                    "The json object cannot be extracted from Storage: "
                        << tValue << endl
                        << "Error: " << errors);
        continue;
      }
      item["value"] = obj;
    } else {
      item["value"] = tValue;
    }
    root.append(item);
  }
  return root;
}

dev::h256 ContractStorage::GetContractStateHash(const dev::h160& address) {
  // LOG_MARKER();
  // iterate the raw protobuf string and hash
  vector<string> rawStates = GetContractStatesData(address);
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  for (const auto& rawState : rawStates) {
    sha2.Update(DataConversion::StringToCharArray(rawState));
  }
  return dev::h256(sha2.Finalize());
}

void ContractStorage::Reset() {
  m_stateDB.ResetDB();

  m_codeDB.ResetDB();
  m_stateIndexDB.ResetDB();
  m_stateDataDB.ResetDB();
  t_stateIndexDB.ResetDB();
  t_stateDataDB.ResetDB();
}

}  // namespace Contract