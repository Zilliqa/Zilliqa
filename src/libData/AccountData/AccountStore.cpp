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

  if (ENABLE_SC && !LOOKUP_NODE_MODE) {
    /// Scilla IPC Server
    /// clear path
    lock_guard<mutex> g(m_mutexIPC);
    boost::filesystem::remove_all(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServerConnector =
        make_unique<jsonrpc::UnixDomainSocketServer>(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServer =
        make_shared<ScillaIPCServer>(*m_scillaIPCServerConnector);
    ScillaClient::GetInstance().Init();
    if (m_scillaIPCServer == nullptr) {
      LOG_GENERAL(WARNING, "m_scillaIPCServer NULL");
    } else {
      m_accountStoreTemp->SetScillaIPCServer(m_scillaIPCServer);
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
  lock_guard<mutex> g(m_mutexIPC);
  if (m_scillaIPCServer != nullptr) {
    m_scillaIPCServer->StopListening();
  }
}

void AccountStore::Init() {
  LOG_MARKER();

  InitSoft();

  ContractStorage2::GetContractStorage().Reset();
  AccountStoreTrie<OverlayDB,
                   unordered_map<Address, std::shared_ptr<Account>>>::ResetDB();
}

void AccountStore::InitSoft() {
  LOG_MARKER();

  AccountStoreTrie<OverlayDB,
                   unordered_map<Address, std::shared_ptr<Account>>>::Init();

  InitRevertibles();

  InitTemp();
}

void AccountStore::InitTemp() {
  LOG_MARKER();

  m_accountStoreTemp->Init();

  {
    unique_lock<shared_timed_mutex> g(m_mutexDelta);
    m_stateDeltaSerialized.clear();
  }

  ContractStorage2::GetContractStorage().InitTempState();
}

void AccountStore::InitRevertibles() {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexRevertibles);

    m_addressToAccountRevChanged.clear();
    m_addressToAccountRevCreated.clear();
  }

  ContractStorage2::GetContractStorage().InitRevertibles();
}

AccountStore& AccountStore::GetInstance() {
  static AccountStore accountstore;
  return accountstore;
}

bool AccountStore::Serialize(bytes& src, unsigned int offset) const {
  LOG_MARKER();
  return AccountStoreTrie<
      dev::OverlayDB,
      std::unordered_map<Address, std::shared_ptr<Account>>>::Serialize(src,
                                                                        offset);
}

bool AccountStore::Deserialize(const bytes& src, unsigned int offset) {
  LOG_MARKER();

  this->Init();

  if (!Messenger::GetAccountStore(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStore::SerializeDelta() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexDelta);

  m_stateDeltaSerialized.clear();

  if (!Messenger::SetAccountStoreDelta(m_stateDeltaSerialized, 0,
                                       *m_accountStoreTemp, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreDelta failed.");
    return false;
  }

  return true;
}

void AccountStore::GetSerializedDelta(bytes& dst) {
  dst.clear();
  {
    shared_lock<shared_timed_mutex> g(m_mutexDelta);
    copy(m_stateDeltaSerialized.begin(), m_stateDeltaSerialized.end(),
         back_inserter(dst));
  }
}

bool AccountStore::DeserializeDelta(const bytes& src, unsigned int offset,
                                    bool revertible) {
  LOG_MARKER();

  if (!Messenger::GetAccountStoreDelta(src, offset, *this, revertible, false)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
    return false;
  }

  return true;
}

bool AccountStore::DeserializeDeltaTemp(const bytes& src, unsigned int offset) {
  return m_accountStoreTemp->DeserializeDelta(src, offset);
}

bool AccountStore::UpdateStateTrieFromTempStateDB() {
  LOG_MARKER();

  leveldb::Iterator* iter = nullptr;

  while (iter == nullptr || iter->Valid()) {
    vector<StateSharedPtr> states;
    if (!BlockStorage::GetBlockStorage().GetTempStateInBatch(iter, states)) {
      LOG_GENERAL(WARNING, "GetTempStateInBatch failed");
      delete iter;
      return false;
    }
    for (const auto& state : states) {
      UpdateStateTrie(state->first, state->second);
    }
  }

  delete iter;

  if (!BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TEMP_STATE)) {
    LOG_GENERAL(WARNING, "BlockStorage::ResetDB (TEMP_STATE) failed");
    return false;
  }
  return true;
}

bool AccountStore::RetrieveFromDisk() {
  LOG_MARKER();

  InitSoft();

  return AccountStoreTrie<
      dev::OverlayDB, std::unordered_map<Address, std::shared_ptr<Account>>>::
      RetrieveFromDisk();
}

bool AccountStore::MoveUpdatesToDisk() {
  LOG_MARKER();

  unordered_map<string, string> code_batch;
  unordered_map<string, string> initdata_batch;

  {
    std::shared_ptr<std::unordered_map<Address, std::shared_ptr<Account>>>
        accounts;
    unique_lock<mutex> g(GetAccounts(accounts));
    for (const auto& i : *accounts) {
      if (i.second->isContract()) {
        if (ContractStorage2::GetContractStorage()
                .GetContractCode(i.first)
                .empty()) {
          code_batch.insert({i.first.hex(), DataConversion::CharArrayToString(
                                                i.second->GetCode())});
        }

        if (ContractStorage2::GetContractStorage()
                .GetInitData(i.first)
                .empty()) {
          initdata_batch.insert(
              {i.first.hex(),
               DataConversion::CharArrayToString(i.second->GetInitData())});
        }
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

  return AccountStoreTrie<
      dev::OverlayDB, std::unordered_map<Address, std::shared_ptr<Account>>>::
      MoveUpdatesToDisk();
}

std::unique_lock<std::mutex> AccountStore::GetAccountWMutexTemp(
    const Address& address, std::shared_ptr<Account>& acc) {
  return m_accountStoreTemp->GetAccountWMutex(address, acc);
}

bool AccountStore::UpdateAccountsTemp(const uint64_t& blockNum,
                                      const unsigned int& numShards,
                                      const bool& isDS,
                                      const Transaction& transaction,
                                      TransactionReceipt& receipt,
                                      TxnStatus& error_code) {
  return m_accountStoreTemp->UpdateAccounts(blockNum, numShards, isDS,
                                            transaction, receipt, error_code);
}

bool AccountStore::UpdateCoinbaseTemp(const Address& rewardee,
                                      const Address& genesisAddress,
                                      const uint128_t& amount) {
  {
    std::shared_ptr<Account> acc;
    unique_lock<std::mutex> g(GetAccountWMutexTemp(rewardee, acc));
    if (acc == nullptr) {
      g.unlock();
      m_accountStoreTemp->AddAccount(rewardee, make_shared<Account>(0, 0));
    }
  }

  return m_accountStoreTemp->TransferBalance(genesisAddress, rewardee, amount);
}

uint128_t AccountStore::GetNonceTemp(const Address& address) {
  {
    std::shared_ptr<std::map<Address, std::shared_ptr<Account>>> accs;
    unique_lock<std::mutex> g(m_accountStoreTemp->GetAccounts(accs));
    auto find = accs->find(address);
    if (find != accs->end()) {
      return find->second->GetNonce();
    }
  }

  return this->GetNonce(address);
}

StateHash AccountStore::GetStateDeltaHash() {
  bool isEmpty = true;
  SHA2<HashType::HASH_VARIANT_256> sha2;
  {
    shared_lock<shared_timed_mutex> g(m_mutexDelta);
    for (unsigned char i : m_stateDeltaSerialized) {
      if (i != 0) {
        isEmpty = false;
        break;
      }
    }

    if (isEmpty) {
      return StateHash();
    }

    sha2.Update(m_stateDeltaSerialized);
  }
  return StateHash(sha2.Finalize());
}

void AccountStore::CommitTemp() {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> g(m_mutexDelta);
  if (!DeserializeDelta(m_stateDeltaSerialized, 0)) {
    LOG_GENERAL(WARNING, "DeserializeDelta failed.");
  }
}

void AccountStore::CommitTempRevertible() {
  LOG_MARKER();

  InitRevertibles();

  shared_lock<shared_timed_mutex> g(m_mutexDelta);
  if (!DeserializeDelta(m_stateDeltaSerialized, 0, true)) {
    LOG_GENERAL(WARNING, "DeserializeDelta failed.");
  }
}

void AccountStore::RevertCommitTemp() {
  LOG_MARKER();

  {
    lock_guard<mutex> g(m_mutexRevertibles);

    // Revert changed
    for (auto const& entry : m_addressToAccountRevChanged) {
      AddAccount(entry.first, entry.second, true);
      UpdateStateTrie(entry.first, entry.second);
    }
    for (auto const& entry : m_addressToAccountRevCreated) {
      RemoveAccount(entry.first);
      RemoveFromTrie(entry.first);
    }
  }

  ContractStorage2::GetContractStorage().RevertContractStates();
}

bool AccountStore::MigrateContractStates2(
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

    std::shared_ptr<Account> account = std::make_shared<Account>();
    if (!account->DeserializeBase(bytes(i.second.begin(), i.second.end()), 0)) {
      LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
      return false;
    }

    if (account->isContract()) {
      account->SetAddress(address);
      if (!contract_address_output_dir.empty()) {
        os_1 << address.hex() << endl;
      }
    } else {
      this->AddAccount(address, account, true);
      if (!normal_address_output_dir.empty()) {
        os_2 << address.hex() << endl;
      }
      continue;
    }

    // adding new metadata
    std::map<std::string, bytes> t_metadata;
    bool is_library;
    uint32_t scilla_version;
    std::vector<Address> extlibs;
    if (!account->GetContractAuxiliaries(is_library, scilla_version, extlibs)) {
      LOG_GENERAL(WARNING, "GetScillaVersion failed");
      return false;
    }

    if (DISABLE_SCILLA_LIB && is_library) {
      LOG_GENERAL(WARNING, "ScillaLib disabled");
      return false;
    }

    std::map<Address, std::pair<std::string, std::string>> extlibs_exports;
    if (!PopulateExtlibsExports(scilla_version, extlibs, extlibs_exports)) {
      LOG_GENERAL(WARNING, "PopulateExtLibsExports failed");
      return false;
    }

    if (!ExportCreateContractFiles(*account, is_library, extlibs_exports)) {
      LOG_GENERAL(WARNING, "ExportCreateContractFiles failed");
      return false;
    }

    // invoke scilla checker
    std::string checkerPrint;
    bool ret_checker = true;
    TransactionReceipt receipt;
    uint64_t gasRem = UINT64_MAX;
    if (!InvokeInterpreter(CHECKER, checkerPrint, scilla_version, is_library,
                           gasRem, std::numeric_limits<uint128_t>::max(),
                           receipt)) {
      ret_checker = false;
    }

    if (!ret_checker) {
      LOG_GENERAL(WARNING, "InvokeScillaChecker failed");
      return false;
    }

    // adding scilla_version metadata
    t_metadata.emplace(
        Contract::ContractStorage2::GetContractStorage().GenerateStorageKey(
            address, SCILLA_VERSION_INDICATOR, {}),
        DataConversion::StringToCharArray(std::to_string(scilla_version)));

    // adding depth and type metadata
    if (!ParseContractCheckerOutput(address, checkerPrint, receipt, t_metadata,
                                    gasRem, is_library)) {
      LOG_GENERAL(WARNING, "ParseContractCheckerOutput failed");
      if (ignoreCheckerFailure) {
        continue;
      }
      return false;
    }

    // remove previous map depth
    std::vector<std::string> toDeletes;
    toDeletes.emplace_back(
        Contract::ContractStorage2::GetContractStorage().GenerateStorageKey(
            address, FIELDS_MAP_DEPTH_INDICATOR, {}));

    account->SetStorageRoot(dev::h256());

    account->UpdateStates(address, t_metadata, toDeletes, false, false, true);

    this->AddAccount(address, account, true);

    if (account->isContract())
      LOG_GENERAL(INFO, "storageRoot: " << account->GetStorageRoot().hex());
  }

  if (!contract_address_output_dir.empty()) {
    os_1.close();
  }
  if (!normal_address_output_dir.empty()) {
    os_2.close();
  }

  if (!UpdateStateTrieAll()) {
    LOG_GENERAL(WARNING, "UpdateStateTrieAll failed");
    return false;
  }

  /// repopulate trie and discard old persistence
  if (!MoveUpdatesToDisk()) {
    LOG_GENERAL(WARNING, "MoveUpdatesToDisk failed");
    return false;
  }

  return true;
}