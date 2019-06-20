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

#include "ScillaMessage.pb.h"
#include "common/Constants.h"
#include "common/Singleton.h"
#include "depends/libDatabase/LevelDB.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"

namespace Contract {

enum TERM { TEMPORARY, SHORTTERM, LONGTERM };

Index GetIndex(const dev::h160& address, const std::string& key);

class ContractStorage2 : public Singleton<ContractStorage2> {
  LevelDB m_codeDB;

  LevelDB m_stateDataDB;

  // Used by AccountStore
  std::map<std::string, bytes> m_stateDataMap;

  // Used by AccountStoreTemp for StateDelta
  std::map<std::string, bytes> t_stateDataMap;

  // Used for revert state due to failure in chain call
  std::map<std::string, bytes> p_stateDataMap;

  // Used for RevertCommitTemp
  std::unordered_map<std::string, bytes> r_stateDataMap;

  // Used for delete map index
  std::set<std::string> m_indexToBeDeleted;

  mutable std::shared_timed_mutex m_codeMutex;
  mutable std::shared_timed_mutex m_stateDataMutex;

  /// Get the raw encoded string of the states of an account
  // bool GetRawContractStates(const dev::h160& address, std::vector<bytes>
  // raw_states, TERM term);

  // bool CreateNestedMap(ZilliqaMessage::ProtoScillaVal& value, const
  // std::vector<string>& indices, const ZilliqaMessage::ProtoScillaVal&
  // emptyMap);

  // bool WriteNestedMap(ZilliqaMessage::ProtoScillaVal& value, const
  // std::vector<string>& indices, const bytes& val);

  void DeleteIndex(const std::string& prefix);

  void UpdateStateData(const std::string& key, const bytes& value);

  ContractStorage2()
      : m_codeDB("contractCode"), m_stateDataDB("contractStateData"){};

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
  const bytes GetContractCode(const dev::h160& address);

  /// Delete the contract code in persistence
  bool DeleteContractCode(const dev::h160& address);

  /////////////////////////////////////////////////////////////////////////////

  // /// Get the raw protobuf string of the state by a index
  // std::string GetContractStateData(const Index& index, bool temp);

  // /// Put one's contract states in database
  // bool PutContractState(const dev::h160& address,
  //                       const std::vector<StateEntry>& states,
  //                       dev::h256& stateHash, bool temp);

  // bool PutContractState(const dev::h160& address,
  //                       const std::vector<std::pair<Index, bytes>>& entries,
  //                       dev::h256& stateHash, bool temp, bool revertible,
  //                       const std::vector<Index>& existing_indexes = {},
  //                       bool provideExisting = false);

  // /// Get the json formatted data of the states for a contract account
  // bool GetContractStateJson(const dev::h160& address,
  //                           std::pair<Json::Value, Json::Value>& roots,
  //                           uint32_t& scilla_version, bool temp);

  bool FetchStateValue(const dev::h160& addr, const bytes& src,
                       unsigned int s_offset, bytes& dst,
                       unsigned int d_offset);

  void FetchStateValueForAddress(const dev::h160& address,
                                 std::map<std::string, bytes>& states);

  void FetchUpdatedStateValuesForAddress(
      const dev::h160& address, std::map<std::string, bytes>& t_states,
      std::vector<std::string>& toDeletedIndices);

  bool UpdateStateValue(const dev::h160& addr, const bytes& q,
                        unsigned int q_offset, const bytes& v,
                        unsigned int v_offset);

  void UpdateStateDatasAndToDeletes(
      const dev::h160& addr, const std::map<std::string, bytes>& t_states,
      const std::vector<std::string>& toDeleteIndices, dev::h256& stateHash,
      bool temp);

  /// Buffer the current t_map into p_map
  void BufferCurrentState();

  /// Revert the t_map from the p_map just buffered
  void RevertPrevState();

  /// Put the in-memory m_map into database
  bool CommitStateDB();

  /// Clean t_maps
  void InitTempState();

  /// Get the state hash of a contract account
  dev::h256 GetContractStateHash(const dev::h160& address, bool temp);

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
