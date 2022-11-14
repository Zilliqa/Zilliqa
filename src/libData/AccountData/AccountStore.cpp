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
#include <boost/filesystem/operations.hpp>
#include <regex>

#include "AccountStore.h"
#include "EvmClient.h"
#include "ScillaClient.h"

#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "libPersistence/ScillaMessage.pb.h"
#pragma GCC diagnostic pop
#include "EvmClient.h"
#include "libServer/ScillaIPCServer.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/ScillaUtils.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace dev;
using namespace boost::multiprecision;
using namespace Contract;

AccountStore::AccountStore() : m_externalWriters{0} {
  m_accountStoreTemp = make_unique<AccountStoreTemp>(*this);
  bool ipcScillaInit = false;

  if (ENABLE_SC || ENABLE_EVM || ISOLATED_SERVER) {
    /// Scilla IPC Server
    /// clear path
    boost::filesystem::remove_all(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServerConnector =
        make_unique<jsonrpc::UnixDomainSocketServer>(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServerConnector->SetWaitTime(
        SCILLA_SERVER_LOOP_WAIT_MICROSECONDS);
    m_scillaIPCServer =
        make_shared<ScillaIPCServer>(*m_scillaIPCServerConnector);

    if (!LOOKUP_NODE_MODE || ISOLATED_SERVER) {
      ScillaClient::GetInstance().Init();
      ipcScillaInit = true;
    }

    if (m_scillaIPCServer == nullptr) {
      LOG_GENERAL(WARNING, "m_scillaIPCServer NULL");
    } else {
      m_accountStoreTemp->SetScillaIPCServer(m_scillaIPCServer);
      if (m_scillaIPCServer->StartListening()) {
        LOG_GENERAL(INFO, "Scilla IPC Server started successfully");
      } else {
        LOG_GENERAL(WARNING, "Scilla IPC Server couldn't start");
      }
    }
  }
  // EVM required to run on Lookup nodes too for view calls
  if (ENABLE_EVM) {
    // TODO lookup nodes may also need it
    if (not ipcScillaInit /*&& !LOOKUP_NODE_MODE*/) {
      ScillaClient::GetInstance().Init();
    }
    EvmClient::GetInstance().Init();
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

  ContractStorage::GetContractStorage().Reset();
  m_db.ResetDB();
}

void AccountStore::InitSoft() {
  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  AccountStoreTrie<unordered_map<Address, Account>>::Init();

  m_externalWriters = 0;

  InitRevertibles();

  InitTemp();
}

bool AccountStore::RefreshDB() {
  bool ret = true;
  {
    lock_guard<mutex> g(m_mutexDB);
    ret = ret && m_db.RefreshDB();
  }
  return ret;
}

void AccountStore::InitTemp() {
  lock_guard<mutex> g(m_mutexDelta);

  m_accountStoreTemp->Init();
  m_stateDeltaSerialized.clear();

  ContractStorage::GetContractStorage().InitTempState();
}

void AccountStore::InitRevertibles() {
  lock_guard<mutex> g(m_mutexRevertibles);

  m_addressToAccountRevChanged.clear();
  m_addressToAccountRevCreated.clear();

  ContractStorage::GetContractStorage().InitRevertibles();
}

AccountStore& AccountStore::GetInstance() {
  static AccountStore accountstore;
  return accountstore;
}

bool AccountStore::Serialize(zbytes& src, unsigned int offset) const {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> lock(m_mutexPrimary);
  return AccountStoreTrie<std::unordered_map<Address, Account>>::Serialize(
      src, offset);
}

bool AccountStore::Deserialize(const zbytes& src, unsigned int offset) {
  LOG_MARKER();

  this->Init();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  if (!Messenger::GetAccountStore(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  m_prevRoot = GetStateRootHash();

  return true;
}

bool AccountStore::Deserialize(const string& src, unsigned int offset) {
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

void AccountStore::GetSerializedDelta(zbytes& dst) {
  lock_guard<mutex> g(m_mutexDelta);

  dst.clear();

  copy(m_stateDeltaSerialized.begin(), m_stateDeltaSerialized.end(),
       back_inserter(dst));
}

bool AccountStore::DeserializeDelta(const zbytes& src, unsigned int offset,
                                    bool revertible) {
  if (LOOKUP_NODE_MODE) {
    std::lock_guard<std::mutex> g(m_mutexTrie);
    if (m_prevRoot != dev::h256()) {
      try {
        m_state.setRoot(m_prevRoot);
      } catch (...) {
        LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
        return false;
      }
    }
  }

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
    if (LOOKUP_NODE_MODE) {
      IncrementPrimaryWriteAccessCount();
    }
    unique_lock<shared_timed_mutex> g(m_mutexPrimary);
    if (LOOKUP_NODE_MODE) {
      DecrementPrimaryWriteAccessCount();
    }

    if (!Messenger::GetAccountStoreDelta(src, offset, *this, revertible,
                                         false)) {
      LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
      return false;
    }
  }

  m_prevRoot = GetStateRootHash();

  return true;
}

bool AccountStore::DeserializeDeltaTemp(const zbytes& src,
                                        unsigned int offset) {
  lock_guard<mutex> g(m_mutexDelta);
  return m_accountStoreTemp->DeserializeDelta(src, offset);
}

bool AccountStore::MoveRootToDisk(const dev::h256& root) {
  // convert h256 to bytes
  if (!BlockStorage::GetBlockStorage().PutStateRoot(root.asBytes())) {
    LOG_GENERAL(INFO, "FAIL: Put state root failed " << root.hex());
    return false;
  }
  return true;
}

bool AccountStore::MoveUpdatesToDisk(uint64_t dsBlockNum) {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  unordered_map<string, string> code_batch;
  unordered_map<string, string> initdata_batch;

  for (const auto& i : *m_addressToAccount) {
    if (i.second.isContract() || i.second.IsLibrary()) {
      if (ContractStorage::GetContractStorage()
              .GetContractCode(i.first)
              .empty()) {
        code_batch.insert({i.first.hex(), DataConversion::CharArrayToString(
                                              i.second.GetCode())});
      }

      if (ContractStorage::GetContractStorage().GetInitData(i.first).empty()) {
        initdata_batch.insert({i.first.hex(), DataConversion::CharArrayToString(
                                                  i.second.GetInitData())});
      }
    }
  }

  if (!ContractStorage::GetContractStorage().PutContractCodeBatch(code_batch)) {
    LOG_GENERAL(WARNING, "PutContractCodeBatch failed");
    return false;
  }

  if (!ContractStorage::GetContractStorage().PutInitDataBatch(initdata_batch)) {
    LOG_GENERAL(WARNING, "PutInitDataBatch failed");
    return false;
  }

  bool ret = true;

  if (ret) {
    if (!ContractStorage::GetContractStorage().CommitStateDB(dsBlockNum)) {
      LOG_GENERAL(WARNING,
                  "CommitTempStateDB failed. need to revert the changes on "
                  "ContractCode");
      ret = false;
    }
  }

  if (!ret) {
    for (const auto& it : code_batch) {
      if (!ContractStorage::GetContractStorage().DeleteContractCode(
              h160(it.first))) {
        LOG_GENERAL(WARNING, "Failed to delete contract code for " << it.first);
      }
    }
  }

  try {
    lock_guard<mutex> g(m_mutexTrie);
    if (!m_state.db()->commit(dsBlockNum)) {
      LOG_GENERAL(WARNING, "LevelDB commit failed");
    }

    if (!MoveRootToDisk(m_state.root())) {
      LOG_GENERAL(WARNING, "MoveRootToDisk failed " << m_state.root().hex());
      return false;
    }
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::MoveUpdatesToDisk(). "
                             << boost::diagnostic_information(e));
    return false;
  }

  m_addressToAccount->clear();

  return true;
}

void AccountStore::PurgeUnnecessary() {
  m_state.db()->DetachedExecutePurge();
  ContractStorage::GetContractStorage().PurgeUnnecessary();
}

void AccountStore::SetPurgeStopSignal() {
  m_state.db()->SetStopSignal();
  ContractStorage::GetContractStorage().SetPurgeStopSignal();
}

bool AccountStore::IsPurgeRunning() {
  return (m_state.db()->IsPurgeRunning() ||
          ContractStorage::GetContractStorage().IsPurgeRunning());
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

void AccountStore::DiscardUnsavedUpdates() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  try {
    {
      lock_guard<mutex> g(m_mutexTrie);
      m_state.db()->rollback();
      if (m_prevRoot != dev::h256()) {
        try {
          m_state.setRoot(m_prevRoot);
        } catch (...) {
          LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
          return;
        }
      }
    }
    m_addressToAccount->clear();
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::DiscardUnsavedUpdates. "
                             << boost::diagnostic_information(e));
  }
}

bool AccountStore::RetrieveFromDisk() {
  InitSoft();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  zbytes rootBytes;
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
    dev::h256 root(rootBytes);
    LOG_GENERAL(INFO, "StateRootHash:" << root.hex());
    lock_guard<mutex> g(m_mutexTrie);
    if (root != dev::h256()) {
      try {
        m_state.setRoot(root);
        m_prevRoot = m_state.root();
      } catch (...) {
        LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
        return false;
      }
    }
  } catch (const boost::exception& e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::RetrieveFromDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }
  return true;
}

bool AccountStore::RetrieveFromDiskOld() {
  // Only For migration
  InitSoft();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDB, defer_lock);
  lock(g, g2);

  zbytes rootBytes;
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

Account* AccountStore::GetAccountTempAtomic(const Address& address) {
  return m_accountStoreTemp->GetAccountAtomic(address);
}

bool AccountStore::UpdateAccountsTemp(
    const uint64_t& blockNum, const unsigned int& numShards, const bool& isDS,
    const Transaction& transaction, const TxnExtras& txnExtras,
    TransactionReceipt& receipt, TxnStatus& error_code) {
  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDelta, defer_lock);
  lock(g, g2);

  bool isEvm{false};

  if (Transaction::GetTransactionType(transaction) ==
      Transaction::CONTRACT_CREATION) {
    isEvm = EvmUtils::isEvm(transaction.GetCode());
  } else {
    // We need to look at the code for any transaction type. Even if it is a
    // simple transfer, it might actually be a call.
    Account* contractAccount = this->GetAccountTemp(transaction.GetToAddr());
    if (contractAccount != nullptr) {
      isEvm = EvmUtils::isEvm(contractAccount->GetCode());
    }
  }
  if (ENABLE_EVM == false && isEvm) {
    LOG_GENERAL(WARNING,
                "EVM is disabled so not processing this EVM transaction ");
    return false;
  }
  if (isEvm) {
    EvmProcessContext p(blockNum,transaction,txnExtras);
    return m_accountStoreTemp->UpdateAccountsEvm(
        blockNum, numShards, isDS, receipt, error_code, p);
  } else {
    return m_accountStoreTemp->UpdateAccounts(blockNum, numShards, isDS,
                                              transaction, receipt, error_code);
  }
}

bool AccountStore::UpdateCoinbaseTemp(const Address& rewardee,
                                      const Address& genesisAddress,
                                      const uint128_t& amount) {
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

bool AccountStore::RevertCommitTemp() {
  LOG_MARKER();

  unique_lock<shared_timed_mutex> g(m_mutexPrimary);
  // Revert changed
  for (auto const& entry : m_addressToAccountRevChanged) {
    (*m_addressToAccount)[entry.first] = entry.second;
    UpdateStateTrie(entry.first, entry.second);
  }
  for (auto const& entry : m_addressToAccountRevCreated) {
    RemoveAccount(entry.first);
    RemoveFromTrie(entry.first);
  }

  ContractStorage::GetContractStorage().RevertContractStates();

  return true;
}

void AccountStore::NotifyTimeoutTemp() { m_accountStoreTemp->NotifyTimeout(); }
