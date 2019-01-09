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

#include "depends/common/RLP.h"
#include "libCrypto/Sha2.h"
#include "libUtils/DataConversion.h"

using namespace dev;
using namespace std;

namespace Contract {

Index GetIndex(const h160& address, const string& key, unsigned int counter) {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(address.asBytes());
  sha2.Update(DataConversion::StringToCharArray(key));
  if (counter != 0) {
	sha2.Update(DataConversion::StringToCharArray(to_string(counter)));
  }
  return h256(sha2.Finalize());
}

bool ContractStorage::PutContractCode(const h160& address, const bytes& code) {
  return m_codeDB.Insert(address.hex(), code) == 0;
}

const bytes ContractStorage::GetContractCode(const h160& address) {
  return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
}

bool ContractStorage::CheckIndexExists(const Index& index) {
	return m_stateDataDB.Exists(index.hex());
}

Index ContractStorage::GetNewIndex(const h160& address, const string& key) {
	Index index;
	unsigned int counter = 0;
	do {
	  index = GetIndex(address, key, counter);
	  counter++;
	} while (CheckIndexExists(index));

	return index;
}

bool ContractStorage::PutContractState(const h160& address,
                                       const vector<StateEntry>& states,
                                       h256& stateHash) {
  vector<pair<Index, string>> entries;
  RLPStream rlpStream(ITEMS_NUM);
  for (const auto& state : states) {
  	// Generate new index = hash(addr + vname)
  	Index index = GetNewIndex(address, std::get<VNAME>(state));

  	// Check if index exists

  	// Serialize with rlp
    rlpStream << std::get<VNAME>(state)
              << (std::get<MUTABLE>(state) ? "True" : "False")
              << std::get<TYPE>(state) << std::get<VALUE>(state);
    entries.emplace_back(index, rlpStream.out());
  }

  return PutContractState(address, entries, stateHash);
}

bool ContractStorage::PutContractState(const h160& address,
                    const vector<pair<Index, bytes>>& entries,
                    h256& stateHash) {
  // Get all the indexes from this account
  vector<Index> indexes = GetContractStateIndexes(address);

  vector<Index> new_entry_indexes;

  bool ret = true;

  for (const auto& entry : entries) {
  	// Append the new index to the existing indexes
  	indexes.emplace_back(entry.first());

  	// Add new entry in stateDataDB
  	if (!m_stateDataDB.Insert(entry.first().hex(), entry.second())) {
  		ret = false;
  		break;
  	}

  	new_entry_indexes.emplace_back(entry.first());
  }

  if (!ret) {
    for (const auto& index : new_entry_indexes) {
      m_stateDataDB.DeleteKey(index.hex());
    }

    return false;
  }

  // Update the stateIndexDB
  if (!SetContractStateIndexes(address, indexes)) {
    for (const auto& index : new_entry_indexes) {
      m_stateDataDB.DeleteKey(index.hex());
    }
    return false;
  }

  stateHash = GetContractStateHash(address);

  return true;
}

bool ContractStorage::SetContractStateIndexes(
    const h160& address, const std::vector<Index>& indexes) {
  RLPStream rlpStream(indexes.size());
  for (const auto& index : indexes) {
    rlpStream << index.hex();
  }
  return m_stateIndexDB.Insert(address.hex(), rlpStream.out());
}

vector<Index> ContractStorage::GetContractStateIndexes(const h160& address) {
  // get from stateIndexDB
  // return DataConversion::StringToCharArray(m_codeDB.Lookup(address.hex()));
  std::vector<Index> indexes;
  RLP rlps(m_stateIndexDB.Lookup(address.hex()));
  for (const auto& rlp : rlps) {
    indexes.push_back(Index(rlp.toBytes()));
  }

  return indexes;
}

vector<string> ContractStorage::GetContractStatesData(const h160& address) {
  // get indexes
  vector<Index> indexes = GetContractStateIndexes(address);

  vector<string> rawStates;

  // return vector of raw rlp string
  for (const auto& index : indexes) {
    rawStates.push_back(m_stateDataDB.Lookup(index.hex()));
  }

  return rawStates;
}

string ContractStorage::GetContractStateData(const Index& index) {
	return m_stateDataDB.Lookup(index.hex());
}

Json::Value ContractStorage::GetContractStateJson(const h160& address) {
  // iterate and deserialize the vector of raw rlp string
  vector<string> rawStates = GetContractStatesData(address);

  Json::Value root;
  for (const auto& rawState : rawStates) {
    RLP rlp(rawState);
    string tVname = rlp[VNAME].toString();
    string tMutable = rlp[MUTABLE].toString();
    string tType = rlp[TYPE].toString();
    string tValue = rlp[VALUE].toString();

    if (tMutable == "False") {
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

h256 ContractStorage::GetContractStateHash(const h160& address) {
  // iterate the raw rlp string and hash
  vector<string> rawStates = GetContractStatesData(address);
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  for (const auto& rawState : rawStates) {
    sha2.Update(DataConversion::StringToCharArray(rawState));
  }
  return h256(sha2.Finalize());
}

}  // namespace Contract