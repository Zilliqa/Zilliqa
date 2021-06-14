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

#ifndef ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE_H_
#define ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE_H_

#include <json/json.h>
#include <leveldb/db.h>
#include <shared_mutex>

#include "ContractStorageOldData.h"
#include "common/Constants.h"
#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"
#include "libData/DataStructures/TraceableDB.h"

class ProtoScillaQuery;

namespace Contract {

static std::string type_placeholder;

enum TERM { TEMPORARY, SHORTTERM, LONGTERM };

Index GetIndex(const dev::h160& address, const std::string& key);
class ContractStorage : public Singleton<ContractStorage> {
  LevelDB m_stateDataDB;
  LevelDB m_codeDB;
  LevelDB m_initDataDB;
  TraceableDB m_trieDB;

  dev::GenericTrieDB<TraceableDB> m_stateTrie;

  // Used by AccountStore
  std::map<std::string, bytes> m_stateDataMap;

  // Used by AccountStoreTemp for StateDelta
  std::map<std::string, bytes> t_stateDataMap;

  // Used for revert state due to failure in chain call
  std::map<std::string, bytes> p_stateDataMap;
  std::set<std::string> p_indexToBeDeleted;

  // Used for RevertCommitTemp
  std::unordered_map<dev::h256, std::unordered_map<std::string, bytes>>
      r_stateDataMap;
  // value being true for newly added, false for newly deleted
  std::unordered_map<std::string, bool> r_indexToBeDeleted;

  // Used for delete map index
  std::set<std::string> m_indexToBeDeleted;
  std::set<std::string> t_indexToBeDeleted;

  mutable std::mutex m_codeMutex;
  mutable std::mutex m_initDataMutex;
  mutable std::mutex m_stateDataMutex;

  void DeleteByPrefix(const std::string& prefix);

  void DeleteByIndex(const std::string& index);

  void UpdateStateData(const std::string& key, const bytes& value,
                       bool cleanEmpty = false);

  bool CleanEmptyMapPlaceholders(const std::string& key);

  void UnquoteString(std::string& input);

  bool CheckHasMap(const dev::h160& addr, bool temp);

  void FetchProofForKey(std::set<std::string>& proof, const dev::h256& key);

  ContractStorage();

  ~ContractStorage() = default;

 public:
  /// Returns the singleton ContractStorage instance.
  static ContractStorage& GetContractStorage() {
    static ContractStorage cs;
    return cs;
  }

  /////////////////////////////////////////////////////////////////////////////

  bool PutContractCode(const dev::h160& address, const bytes& code);

  /// Adds contract codes to persistence in batch
  bool PutContractCodeBatch(
      const std::unordered_map<std::string, std::string>& batch);

  /// Get the desired code from persistence
  bytes GetContractCode(const dev::h160& address);

  /// Delete the contract code in persistence
  bool DeleteContractCode(const dev::h160& address);

  /////////////////////////////////////////////////////////////////////////////
  bool PutInitData(const dev::h160& address, const bytes& initData);

  bool PutInitDataBatch(
      const std::unordered_map<std::string, std::string>& batch);

  bytes GetInitData(const dev::h160& address);

  bool DeleteInitData(const dev::h160& address);

  /////////////////////////////////////////////////////////////////////////////
  static std::string GenerateStorageKey(
      const dev::h160& addr, const std::string& vname,
      const std::vector<std::string>& indices);

  std::string RemoveAddrFromKey(const std::string& key);

  bool IsReservedVName(const std::string& name);

  bool FetchStateValue(const dev::h160& addr, const bytes& src,
                       unsigned int s_offset, bytes& dst, unsigned int d_offset,
                       bool& foundVal, bool getType = false,
                       std::string& type = type_placeholder);

  bool FetchStateValue(const dev::h160& addr, const ProtoScillaQuery& query,
                       bytes& dst, unsigned int d_offset, bool& foundVal,
                       bool getType = false,
                       std::string& type = type_placeholder);

  bool FetchExternalStateValue(
      const dev::h160& caller, const dev::h160& target, const bytes& src,
      unsigned int s_offset, bytes& dst, unsigned int d_offset, bool& foundVal,
      std::string& type,
      uint32_t caller_version = std::numeric_limits<uint32_t>::max());

  void InsertValueToStateJson(Json::Value& _json, std::string key,
                              std::string value, bool unquote = true,
                              bool nokey = false);

  bool FetchStateJsonForContract(Json::Value& _json, const dev::h160& address,
                                 const std::string& vname = "",
                                 const std::vector<std::string>& indices = {},
                                 bool temp = false);

  void FetchStateDataForKey(std::map<std::string, bytes>& states,
                            const std::string& key, bool temp);

  void FetchStateDataForContract(std::map<std::string, bytes>& states,
                                 const dev::h160& address,
                                 const std::string& vname = "",
                                 const std::vector<std::string>& indices = {},
                                 bool temp = true);

  void FetchUpdatedStateValuesForAddress(
      const dev::h160& address, std::map<std::string, bytes>& states,
      std::set<std::string>& toDeletedIndices, bool temp = false);
  bool FetchStateProofForContract(std::set<std::string>& proof,
                                  const dev::h256& rootHash,
                                  const dev::h256& key);

  bool UpdateStateValue(const dev::h160& addr, const bytes& q,
                        unsigned int q_offset, const bytes& v,
                        unsigned int v_offset);

  bool CheckIfKeyIsEmpty(const std::string& key, bool temp);

  void UpdateStateDatasAndToDeletes(
      const dev::h160& addr, const dev::h256& rootHash,
      const std::map<std::string, bytes>& states,
      const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
      bool temp, bool revertible);

  void PurgeUnnecessary();

  void SetPurgeStopSignal();

  bool IsPurgeRunning();

  /// Buffer the current t_map into p_map
  void BufferCurrentState();

  /// Revert the t_map from the p_map just buffered
  void RevertPrevState();

  /// Put the in-memory m_map into database
  bool CommitStateDB(const uint64_t& dsBlockNum);

  /// Clean t_maps
  void InitTempState(bool callFromExternal = false);

  void InitTempStateCore();

  /// Clean the databases
  void Reset();

  /// Revert m_map with r_map
  void RevertContractStates();

  /// Clean r_map
  void InitRevertibles();

  /// Refresh all DB
  bool RefreshAll();
};

}  // namespace Contract

#endif  // ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE_H_
