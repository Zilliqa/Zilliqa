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

#include <leveldb/db.h>

#include "AccountStore.h"
#include "depends/safeserver/safetcpsocketserver.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libPersistence/ScillaMessage.pb.h"
#pragma GCC diagnostic pop
#include "libServer/ScillaIPCServer.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace dev;
using namespace boost::multiprecision;
using namespace Contract;

AccountStore::AccountStore() {
  m_accountStoreTemp = make_unique<AccountStoreTemp>(*this);

  /// Scilla IPC Server
  if (ENABLE_SC) {
    /// remove previous file path
    boost::filesystem::remove_all(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServerConnector =
        make_unique<jsonrpc::UnixDomainSocketServer>(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServer =
        make_shared<ScillaIPCServer>(*m_scillaIPCServerConnector);
    if (m_scillaIPCServer == nullptr) {
      LOG_GENERAL(WARNING, "m_scillaIPCServer NULL");
    } else {
      SetScillaIPCServer(m_scillaIPCServer);
      if (m_scillaIPCServer->StartListening()) {
        LOG_GENERAL(INFO, "Scilla IPC Server started successfully");
      } else {
        LOG_GENERAL(WARNING, "Scilla IPC Server couldn't start")
      }
    }
  }
}

AccountStore::~AccountStore() {
  // boost::filesystem::remove_all("./state");
  if (m_scillaIPCServer != nullptr) {
    m_scillaIPCServer->StopListening();
  }
}

void AccountStore::Init() {
  LOG_MARKER();

  InitSoft();

  lock_guard<mutex> g(m_mutexDB);

  ContractStorage2::GetContractStorage().Reset();
  m_db.ResetDB();
}

void AccountStore::SetScillaIPCServer(
    std::shared_ptr<ScillaIPCServer> scillaIPCServer) {
  m_accountStoreTemp->SetScillaIPCServer(scillaIPCServer);
}

void AccountStore::InitSoft() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  AccountStoreTrie<OverlayDB, unordered_map<Address, Account>>::Init();

  InitRevertibles();

  InitTemp();
}

bool AccountStore::RefreshDB() {
  LOG_MARKER();
  lock_guard<mutex> g(m_mutexDB);
  return m_db.RefreshDB();
}

void AccountStore::InitTemp() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexDelta);

  m_accountStoreTemp->Init();
  m_stateDeltaSerialized.clear();

  ContractStorage2::GetContractStorage().InitTempState(true);
}

void AccountStore::InitRevertibles() {
  LOG_MARKER();

  lock_guard<mutex> g(m_mutexRevertibles);

  m_addressToAccountRevChanged.clear();
  m_addressToAccountRevCreated.clear();

  ContractStorage2::GetContractStorage().InitRevertibles();
}

AccountStore& AccountStore::GetInstance() {
  static AccountStore accountstore;
  return accountstore;
}

bool AccountStore::Serialize(bytes& src, unsigned int offset) const {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> lock(m_mutexPrimary);
  return AccountStoreTrie<
      dev::OverlayDB, std::unordered_map<Address, Account>>::Serialize(src,
                                                                       offset);
}

bool AccountStore::Deserialize(const bytes& src, unsigned int offset) {
  LOG_MARKER();

  this->Init();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  if (!Messenger::GetAccountStore(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStore::SerializeDelta() {
  LOG_MARKER();

  unique_lock<mutex> g(m_mutexDelta, defer_lock);
  shared_lock<shared_timed_mutex> g2(m_mutexPrimary, defer_lock);
  lock(g, g2);

  m_stateDeltaSerialized.clear();

  if (!Messenger::SetAccountStoreDelta(m_stateDeltaSerialized, 0,
                                       *m_accountStoreTemp, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreDelta failed.");
    return false;
  }

  return true;
}

void AccountStore::GetSerializedDelta(bytes& dst) {
  lock_guard<mutex> g(m_mutexDelta);

  dst.clear();

  copy(m_stateDeltaSerialized.begin(), m_stateDeltaSerialized.end(),
       back_inserter(dst));
}

bool AccountStore::DeserializeDelta(const bytes& src, unsigned int offset,
                                    bool revertible) {
  LOG_MARKER();

  if (revertible) {
    unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
    unique_lock<mutex> g2(m_mutexRevertibles, defer_lock);
    lock(g, g2);

    if (!Messenger::GetAccountStoreDelta(src, offset, *this, revertible,
                                         false)) {
      LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
      return false;
    }
  } else {
    unique_lock<shared_timed_mutex> g(m_mutexPrimary);

    if (!Messenger::GetAccountStoreDelta(src, offset, *this, revertible,
                                         false)) {
      LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
      return false;
    }
  }

  return true;
}

bool AccountStore::DeserializeDeltaTemp(const bytes& src, unsigned int offset) {
  lock_guard<mutex> g(m_mutexDelta);
  return m_accountStoreTemp->DeserializeDelta(src, offset);
}

bool AccountStore::MoveRootToDisk(const h256& root) {
  // convert h256 to bytes
  if (!BlockStorage::GetBlockStorage().PutStateRoot(root.asBytes())) {
    LOG_GENERAL(INFO, "FAIL: Put state root failed " << root.hex());
    return false;
  }
  return true;
}

bool AccountStore::MoveUpdatesToDisk(bool repopulate, bool retrieveFromTrie) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  unordered_map<string, string> code_batch;
  unordered_map<string, string> initdata_batch;

  for (const auto& i : *m_addressToAccount) {
    if (i.second.isContract()) {
      if (ContractStorage2::GetContractStorage()
              .GetContractCode(i.first)
              .empty()) {
        code_batch.insert({i.first.hex(), DataConversion::CharArrayToString(
                                              i.second.GetCode())});
      }

      if (ContractStorage2::GetContractStorage().GetInitData(i.first).empty()) {
        initdata_batch.insert({i.first.hex(), DataConversion::CharArrayToString(
                                                  i.second.GetInitData())});
      }
    }
  }

  if (!ContractStorage2::GetContractStorage().PutContractCodeBatch(
          code_batch)) {
    LOG_GENERAL(WARNING, "PutContractCodeBatch failed");
    return false;
  }

  if (!ContractStorage2::GetContractStorage().PutInitDataBatch(
          initdata_batch)) {
    LOG_GENERAL(WARNING, "PutInitDataBatch failed");
    return false;
  }

  bool ret = true;

  if (ret) {
    if (!ContractStorage2::GetContractStorage().CommitStateDB()) {
      LOG_GENERAL(WARNING,
                  "CommitTempStateDB failed. need to rever the change on "
                  "ContractCode");
      ret = false;
    }
  }

  if (!ret) {
    for (const auto& it : code_batch) {
      if (!ContractStorage2::GetContractStorage().DeleteContractCode(
              h160(it.first))) {
        LOG_GENERAL(WARNING, "Failed to delete contract code for " << it.first);
      }
    }
  }

  try {
    if (repopulate && !RepopulateStateTrie(retrieveFromTrie)) {
      LOG_GENERAL(WARNING, "RepopulateStateTrie failed");
      return false;
    }
    lock_guard<mutex> g(m_mutexTrie);
    m_state.db()->commit();
    if (!MoveRootToDisk(m_state.root())) {
      LOG_GENERAL(WARNING, "MoveRootToDisk failed " << m_state.root().hex());
      return false;
    }
    m_prevRoot = m_state.root();
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::MoveUpdatesToDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }

  m_addressToAccount->clear();

  return true;
}

bool AccountStore::RepopulateStateTrie(bool retrieveFromTrie) {
  LOG_MARKER();

  unsigned int counter = 0;
  bool batched_once = false;

  if (retrieveFromTrie) {
    lock_guard<mutex> g(m_mutexTrie);
    for (const auto& i : m_state) {
      counter++;

      if (counter >= ACCOUNT_IO_BATCH_SIZE) {
        // Write into db
        if (!BlockStorage::GetBlockStorage().PutTempState(
                *this->m_addressToAccount)) {
          LOG_GENERAL(WARNING, "PutTempState failed");
          return false;
        } else {
          // this->m_addressToAccount->clear();
          counter = 0;
          batched_once = true;
        }
      }

      Address address(i.first);

      if (!batched_once) {
        if (this->m_addressToAccount->find(address) !=
            this->m_addressToAccount->end()) {
          continue;
        }
      }

      Account account;
      if (!account.DeserializeBase(bytes(i.second.begin(), i.second.end()),
                                   0)) {
        LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
        continue;
      }
      if (account.isContract()) {
        account.SetAddress(address);
      }

      this->m_addressToAccount->insert({address, account});
    }
  }

  if (!this->m_addressToAccount->empty()) {
    if (!BlockStorage::GetBlockStorage().PutTempState(
            *(this->m_addressToAccount))) {
      LOG_GENERAL(WARNING, "PutTempState failed");
      return false;
    } else {
      this->m_addressToAccount->clear();
      batched_once = true;
    }
  }

  m_db.ResetDB();
  InitTrie();

  if (batched_once) {
    return UpdateStateTrieFromTempStateDB();
  } else {
    return UpdateStateTrieAll();
  }
}

bool AccountStore::UpdateStateTrieFromTempStateDB() {
  LOG_MARKER();

  leveldb::Iterator* iter = nullptr;

  while (iter == nullptr || iter->Valid()) {
    vector<StateSharedPtr> states;
    if (!BlockStorage::GetBlockStorage().GetTempStateInBatch(iter, states)) {
      LOG_GENERAL(WARNING, "GetTempStateInBatch failed");
      return false;
    }
    for (const auto& state : states) {
      UpdateStateTrie(state->first, state->second);
    }
  }

  if (!BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TEMP_STATE)) {
    LOG_GENERAL(WARNING, "BlockStorage::ResetDB (TEMP_STATE) failed");
    return false;
  }
  return true;
}

void AccountStore::DiscardUnsavedUpdates() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  try {
    {
      lock_guard<mutex> g(m_mutexTrie);
      m_state.db()->rollback();
      m_state.setRoot(m_prevRoot);
    }
    m_addressToAccount->clear();
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::DiscardUnsavedUpdates. "
                             << boost::diagnostic_information(e));
  }
}

bool AccountStore::RetrieveFromDisk() {
  LOG_MARKER();

  InitSoft();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  bytes rootBytes;
  if (!BlockStorage::GetBlockStorage().GetStateRoot(rootBytes)) {
    // To support backward compatibilty - lookup with new binary trying to
    // recover from old database
    if (BlockStorage::GetBlockStorage().GetMetadata(STATEROOT, rootBytes)) {
      if (!BlockStorage::GetBlockStorage().PutStateRoot(rootBytes)) {
        LOG_GENERAL(WARNING,
                    "BlockStorage::PutStateRoot failed "
                        << DataConversion::CharArrayToString(rootBytes));
        return false;
      }
    } else {
      LOG_GENERAL(WARNING, "Failed to retrieve StateRoot from disk");
      return false;
    }
  }

  try {
    h256 root(rootBytes);
    LOG_GENERAL(INFO, "StateRootHash:" << root.hex());
    lock_guard<mutex> g(m_mutexTrie);
    m_state.setRoot(root);
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::RetrieveFromDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }
  return true;
}

Account* AccountStore::GetAccountTemp(const Address& address) {
  return m_accountStoreTemp->GetAccount(address);
}

bool AccountStore::UpdateAccountsTemp(const uint64_t& blockNum,
                                      const unsigned int& numShards,
                                      const bool& isDS,
                                      const Transaction& transaction,
                                      TransactionReceipt& receipt) {
  // LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDelta, defer_lock);
  lock(g, g2);

  return m_accountStoreTemp->UpdateAccounts(blockNum, numShards, isDS,
                                            transaction, receipt);
}

bool AccountStore::UpdateCoinbaseTemp(const Address& rewardee,
                                      const Address& genesisAddress,
                                      const uint128_t& amount) {
  // LOG_MARKER();

  lock_guard<mutex> g(m_mutexDelta);

  if (m_accountStoreTemp->GetAccount(rewardee) == nullptr) {
    m_accountStoreTemp->AddAccount(rewardee, {0, 0});
  }
  return m_accountStoreTemp->TransferBalance(genesisAddress, rewardee, amount);
  // Should the nonce increase ??
}

uint128_t AccountStore::GetNonceTemp(const Address& address) {
  lock_guard<mutex> g(m_mutexDelta);

  if (m_accountStoreTemp->GetAddressToAccount()->find(address) !=
      m_accountStoreTemp->GetAddressToAccount()->end()) {
    return m_accountStoreTemp->GetNonce(address);
  } else {
    return this->GetNonce(address);
  }
}

StateHash AccountStore::GetStateDeltaHash() {
  lock_guard<mutex> g(m_mutexDelta);

  bool isEmpty = true;

  for (unsigned char i : m_stateDeltaSerialized) {
    if (i != 0) {
      isEmpty = false;
      break;
    }
  }

  if (isEmpty) {
    return StateHash();
  }

  SHA2<HashType::HASH_VARIANT_256> sha2;
  sha2.Update(m_stateDeltaSerialized);
  return StateHash(sha2.Finalize());
}

void AccountStore::CommitTemp() {
  LOG_MARKER();
  if (!DeserializeDelta(m_stateDeltaSerialized, 0)) {
    LOG_GENERAL(WARNING, "DeserializeDelta failed.");
  }
}

void AccountStore::CommitTempRevertible() {
  LOG_MARKER();

  InitRevertibles();

  if (!DeserializeDelta(m_stateDeltaSerialized, 0, true)) {
    LOG_GENERAL(WARNING, "DeserializeDelta failed.");
  }
}

void AccountStore::RevertCommitTemp() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  // Revert changed
  for (auto const& entry : m_addressToAccountRevChanged) {
    // LOG_GENERAL(INFO, "Revert changed address: " << entry.first);
    (*m_addressToAccount)[entry.first] = entry.second;
    UpdateStateTrie(entry.first, entry.second);
  }
  for (auto const& entry : m_addressToAccountRevCreated) {
    // LOG_GENERAL(INFO, "Remove created address: " << entry.first);
    RemoveAccount(entry.first);
    RemoveFromTrie(entry.first);
  }

  ContractStorage2::GetContractStorage().RevertContractStates();
}

bool AccountStore::MigrateContractStates(
    bool ignoreCheckerFailure, const string& contract_address_output_dir,
    const string& normal_address_output_dir) {
  LOG_MARKER();

  std::ofstream os_1;
  std::ofstream os_2;
  if (!contract_address_output_dir.empty()) {
    os_1.open(contract_address_output_dir);
  }
  if (!normal_address_output_dir.empty()) {
    os_2.open(normal_address_output_dir);
  }

  for (const auto& i : m_state) {
    Address address(i.first);
    LOG_GENERAL(INFO, "Address: " << address.hex());

    Account account;
    if (!account.DeserializeBase(bytes(i.second.begin(), i.second.end()), 0)) {
      LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
      return false;
    }
    if (account.isContract()) {
      account.SetAddress(address);
      if (!contract_address_output_dir.empty()) {
        os_1 << address.hex() << endl;
      }
    } else {
      this->AddAccount(address, account);
      if (!normal_address_output_dir.empty()) {
        os_2 << address.hex() << endl;
      }
      continue;
    }

    std::pair<Json::Value, Json::Value> roots;
    if (!account.GetStorageJson(roots)) {
      LOG_GENERAL(WARNING, "Cannot get StorageJson");
    } else {
      LOG_GENERAL(
          INFO, "InitJson: "
                    << JSONUtils::GetInstance().convertJsontoStr(roots.first));
      LOG_GENERAL(
          INFO, "old account state: "
                    << JSONUtils::GetInstance().convertJsontoStr(roots.second));
    }

    map<string, bytes> mutable_states;
    // map<string, bytes> immutable_states;
    Json::Value immutable_states;

    /// generate depth_map

    uint32_t scilla_version;
    bool found_scilla_version = false;

    /// retrieving immutable states (init data)
    for (const auto& index : account.GetStorageKeyHashes()) {
      string raw_val = account.GetRawStorage(index, false);

      StateEntry entry;
      uint32_t version;
      if (!Messenger::GetStateData(DataConversion::StringToCharArray(raw_val),
                                   0, entry, version)) {
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
      string tValue = std::get<VALUE>(entry);  /// could be "string" or [map]

      if (!std::get<MUTABLE>(entry)) {
        Json::Value immutable;
        immutable["vname"] = tVname;
        immutable["type"] = std::get<TYPE>(entry);
        if (tVname == "_scilla_version") {
          scilla_version = boost::lexical_cast<uint32_t>(tValue);
          found_scilla_version = true;
        }
        LOG_GENERAL(INFO,
                    "Immutable vname: " << tVname << " value: " << tValue);
        ContractStorage2::GetContractStorage().InsertValueToStateJson(
            immutable, "value", tValue, false);
        immutable_states.append(immutable);
      } else {
        continue;
      }
    }

    LOG_GENERAL(INFO,
                "Immutables: " << JSONUtils::GetInstance().convertJsontoStr(
                    immutable_states));

    account.SetImmutable(
        account.GetCode(),
        DataConversion::StringToCharArray(
            JSONUtils::GetInstance().convertJsontoStr(immutable_states)));

    Json::Value map_depth_json;

    if (found_scilla_version) {
      if (ExportCreateContractFiles(account, scilla_version)) {
        std::string checkerPrint;
        bool ret_checker = true;
        int pid = -1;
        TransactionReceipt receipt;
        uint64_t gasRem = UINT64_MAX;
        InvokeScillaChecker(checkerPrint, ret_checker, pid, gasRem, receipt);

        if (ret_checker) {
          bytes map_depth_data;
          if (!ParseContractCheckerOutput(checkerPrint, receipt, map_depth_data,
                                          gasRem)) {
            LOG_GENERAL(WARNING,
                        "Failed to generate map_depth_data from scilla_checker "
                        "print for contract "
                            << address.hex());
            if (ignoreCheckerFailure) {
              continue;
            }
            return false;
          }
          /// redundant conversion here as don't want to change
          /// ParseContractCheckerOutput
          if (map_depth_data.empty()) {
            map_depth_json = Json::objectValue;
            map_depth_data = DataConversion::StringToCharArray(
                JSONUtils::GetInstance().convertJsontoStr(map_depth_json));
          } else if (!JSONUtils::GetInstance().convertStrtoJson(
                         DataConversion::CharArrayToString(map_depth_data),
                         map_depth_json)) {
            LOG_GENERAL(WARNING,
                        "Account "
                            << address.hex()
                            << " failed to parse map_depth_data into json "
                            << DataConversion::CharArrayToString(
                                   map_depth_data));
            return false;
          }
          mutable_states.emplace(
              Contract::ContractStorage2::GetContractStorage()
                  .GenerateStorageKey(address, FIELDS_MAP_DEPTH_INDICATOR, {}),
              map_depth_data);
        } else {
          LOG_GENERAL(WARNING, "InvokeScillaChecker failed for contract: "
                                   << address.hex());
          return false;
        }
      } else {
        LOG_GENERAL(WARNING, "ExportCreateContractFiles failed for contract: "
                                 << address.hex());
        return false;
      }
    } else {
      LOG_GENERAL(WARNING,
                  "Didn't find scilla_version for contract: " << address.hex());
      return false;
    }

    /// retrieving mutable states
    for (const auto& index : account.GetStorageKeyHashes()) {
      string raw_val = account.GetRawStorage(index, false);

      StateEntry entry;
      uint32_t version;
      if (!Messenger::GetStateData(DataConversion::StringToCharArray(raw_val),
                                   0, entry, version)) {
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
      string tValue = std::get<VALUE>(entry);  /// could be "string" or [map]

      if (!std::get<MUTABLE>(entry)) {
        continue;
      }

      string key = i.first.hex();
      key += SCILLA_INDEX_SEPARATOR + tVname + SCILLA_INDEX_SEPARATOR;

      // Check is the value is map
      Json::Value json_val;

      if (!map_depth_json.isMember(tVname)) {
        LOG_GENERAL(WARNING, tVname
                                 << " is not found in map_depth_json: "
                                 << JSONUtils::GetInstance().convertJsontoStr(
                                        map_depth_json));
        return false;
      }

      uint map_depth = map_depth_json[tVname].asUInt();

      if (map_depth > 0 &&
          JSONUtils::GetInstance().convertStrtoJson(tValue, json_val) &&
          json_val.type() == Json::arrayValue) {
        /// mapHandler
        std::function<bool(const string&, const Json::Value&,
                           map<string, bytes>&, uint, uint)>
            mapHandler = [&](const string& key, const Json::Value& j_value,
                             map<string, bytes>& t_states, uint cur_depth,
                             uint map_depth) -> bool {
          cur_depth++;
          if (j_value.empty()) {
            // make an empty protobuf scilla map value object
            ProtoScillaVal t_scillaVal;
            t_scillaVal.mutable_mval()->mutable_m();
            bytes dst;
            dst.resize(t_scillaVal.ByteSize());
            if (!t_scillaVal.SerializeToArray(dst.data(),
                                              t_scillaVal.ByteSize())) {
              return false;
            }
            t_states.emplace(key, dst);
            return true;
          } else {
            for (const auto& map_entry : j_value) {
              if (!(map_entry.isMember("key") && map_entry.isMember("val"))) {
                LOG_GENERAL(WARNING,
                            "Invalid map entry: "
                                << JSONUtils::GetInstance().convertJsontoStr(
                                       map_entry));
                return false;
              } else {
                string new_key(key);
                new_key += '"' + map_entry["key"].asString() + '"' +
                           SCILLA_INDEX_SEPARATOR;
                if (map_entry["val"].type() == Json::arrayValue &&
                    cur_depth < map_depth) {
                  if (!mapHandler(new_key, map_entry["val"], t_states,
                                  cur_depth, map_depth)) {
                    return false;
                  }
                } else {
                  t_states.emplace(
                      new_key, DataConversion::StringToCharArray(
                                   JSONUtils::GetInstance().convertJsontoStr(
                                       map_entry["val"])));
                }
              }
            }
          }
          return true;
        };

        if (!mapHandler(key, json_val, mutable_states, 0, map_depth)) {
          LOG_GENERAL(WARNING, "failed to parse map value for: " << tValue);
          return false;
        }
      } else {
        LOG_GENERAL(INFO, "not map value");
        Json::Value tJson;
        if (JSONUtils::GetInstance().convertStrtoJson(tValue, tJson) &&
            (tJson.type() == Json::objectValue ||
             tJson.type() == Json::arrayValue)) {
          // do nothing
        } else {
          tValue.insert(0, "\"");
          tValue.append("\"");
        }
        mutable_states.emplace(key, DataConversion::StringToCharArray(tValue));
      }
    }

    account.UpdateStates(address, mutable_states, {}, false);

    LOG_GENERAL(
        INFO, "current account immutables: "
                  << DataConversion::CharArrayToString(account.GetInitData()));
    Json::Value cur_state;
    if (!account.FetchStateJson(cur_state)) {
      LOG_GENERAL(WARNING, "account fetch state json failed")
      return false;
    }
    LOG_GENERAL(INFO,
                "current account state: "
                    << JSONUtils::GetInstance().convertJsontoStr(cur_state));

    Account* originalAccount = GetAccount(address);
    *originalAccount = account;
    this->AddAccount(address, account);
  }

  if (!contract_address_output_dir.empty()) {
    os_1.close();
  }
  if (!normal_address_output_dir.empty()) {
    os_2.close();
  }

  /// repopulate trie and discard old persistence
  if (!MoveUpdatesToDisk(true, false)) {
    LOG_GENERAL(WARNING, "MoveUpdatesToDisk failed");
    return false;
  }

  return true;
}