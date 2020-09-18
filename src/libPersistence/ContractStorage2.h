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

#ifndef ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE2_H_
#define ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE2_H_

#include <json/json.h>
#include <leveldb/db.h>
#include <shared_mutex>

#include "ContractStorage2Data.h"
#include "common/Constants.h"
#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"

class ProtoScillaVal;

namespace Contract {

using DefaultAddDeleteMap =
    AddDeleteMap<std::unordered_map<dev::h256, bytes>, std::set<dev::h256>>;
using RevertableAddDeleteMap =
    RecordableAddDeleteMap<std::unordered_map<dev::h256, bytes>,
                           std::set<dev::h256>>;

using PermOverlayMap = OverlayMap<std::shared_ptr<RevertableAddDeleteMap>,
                                  std::shared_ptr<LevelDBMap>>;
using TempOverlayMap = OverlayMap<std::shared_ptr<DefaultAddDeleteMap>,
                                  std::shared_ptr<RevertableAddDeleteMap>,
                                  std::shared_ptr<LevelDBMap>>;

static std::string type_placeholder;

enum TERM { TEMPORARY, SHORTTERM, LONGTERM };

Index GetIndex(const dev::h160& address, const std::string& key);

class ContractStorage2 : public Singleton<ContractStorage2> {
  LevelDB m_codeDB;
  LevelDB m_initDataDB;
  LevelDB m_stateDataDB;

  std::shared_ptr<LevelDB> mp_stateDataDB;

  // Used by AccountStore
  std::map<std::string, bytes> m_stateDataMap;
  std::set<std::string> m_indexToBeDeleted;

  std::shared_ptr<std::unordered_map<dev::h256, bytes>> mp_stateDataMap;
  std::shared_ptr<std::set<dev::h256>> mp_indexToBeDeleted;

  // Used by AccountStoreTemp for StateDelta
  std::map<std::string, bytes> t_stateDataMap;
  std::set<std::string> t_indexToBeDeleted;
  std::shared_ptr<std::unordered_map<dev::h256, bytes>> tp_stateDataMap;
  std::shared_ptr<std::set<dev::h256>> tp_indexToBeDeleted;

  // Used for revert state due to failure in chain call
  std::unordered_map<std::string, bytes> p_stateDataMap;
  std::unordered_map<std::string, bool> p_indexToBeDeleted;

  // Used for RevertCommitTemp
  std::unordered_map<std::string, bytes> r_stateDataMap;
  // value being true for newly added, false for newly deleted
  std::unordered_map<std::string, bool> r_indexToBeDeleted;

  std::shared_ptr<DefaultAddDeleteMap> m_tempADMap;
  std::shared_ptr<RevertableAddDeleteMap> m_permADMap;
  std::shared_ptr<LevelDBMap> m_levelDBMap;

  PermOverlayMap m_permOM;
  TempOverlayMap m_tempOM;

  dev::GenericTrieDB<PermOverlayMap> m_permTrie;
  dev::GenericTrieDB<TempOverlayMap> m_tempTrie;

  std::mutex m_codeMutex;
  std::mutex m_initDataMutex;
  std::mutex m_stateDataMutex;
  std::mutex m_stateMPTMutex;

  void DeleteByPrefix(const std::string& prefix);

  void DeleteByIndex(const std::string& index);

  void UpdateStateData(const std::string& key, const bytes& value,
                       bool cleanEmpty = false);

  bool CleanEmptyMapPlaceholders(const std::string& key);

  bool CheckHasMap(const dev::h160& addr, bool temp);

  dev::h256 UpdateContractTrie(const dev::h256& root,
                               const std::map<std::string, bytes>& states,
                               const std::vector<std::string>& toDeletedIndices,
                               bool temp, bool revertible);

  dev::h256 GetContractStateHashForMergingDelta(
      const dev::h160& addr, const dev::h256& root,
      const std::map<std::string, bytes>& states,
      const std::vector<std::string>& toDeleteIndices);

  dev::h256 DirectHashState(const std::map<std::string, bytes>& states);

  ContractStorage2();

  ~ContractStorage2() = default;

 public:
  /// Returns the singleton ContractStorage instance.
  static ContractStorage2& GetContractStorage() {
    static ContractStorage2 cs;
    return cs;
  }

  /// Adds a contract code to persistence
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

  bool FetchStateValue(const dev::h160& addr, const bytes& src,
                       unsigned int s_offset, bytes& dst, unsigned int d_offset,
                       bool& foundVal, bool getType = false,
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
      const dev::h160& address, std::map<std::string, bytes>& t_states,
      std::vector<std::string>& toDeletedIndices, bool temp = false);

  bool UpdateStateValue(const dev::h160& addr, const bytes& q,
                        unsigned int q_offset, const bytes& v,
                        unsigned int v_offset);

  void UpdateStateDatasAndToDeletes(
      const dev::h160& addr, const std::map<std::string, bytes>& states,
      const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
      bool temp, bool revertible, bool migrating);

  /// Buffer the current t_map into p_map
  void ResetBufferedAtomicState();

  /// Revert the t_map from the p_map just buffered
  void RevertAtomicState();

  /// Put the in-memory m_map into database
  bool CommitStateDB();

  /// Clean t_maps
  void InitTempState();

  /// Get the state hash of a contract account
  dev::h256 GetContractStateHash(const dev::h160& addr, const dev::h256& root,
                                 bool temp, bool revertible = false);

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

#endif  // ZILLIQA_SRC_LIBPERSISTENCE_CONTRACTSTORAGE2_H_
