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

#include "common/Common.h"
#include "libData/AccountStore/AccountStore.h"
#include "libData/AccountStore/services/evm/EvmClient.h"
#include "libScilla/ScillaClient.h"

#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"
#include "libMessage/MessengerAccountStoreTrie.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/ContractStorage.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include "libPersistence/ScillaMessage.pb.h"

#pragma GCC diagnostic pop

#include "libData/AccountStore/services/evm/EvmClient.h"
#include "libMetrics/Api.h"
#include "libScilla/ScillaIPCServer.h"
#include "libScilla/ScillaUtils.h"
#include "libScilla/UnixDomainSocketServer.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/SysCommand.h"

using namespace std;
using namespace dev;
using namespace boost::multiprecision;
using namespace Contract;

namespace zil {
namespace local {

Z_DBLHIST &GetEvmLatency() {
  static std::vector<double> latencieBoudaries{
      0, 0.25, 0.5, 0.75, 1, 2, 3, 4, 5, 10, 20, 30, 40, 60, 120};
  static Z_DBLHIST counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "evm.latency",
                           latencieBoudaries, "latency of processing", "ms"};
  return counter;
}

Z_DBLHIST &GetScillaLatency() {
  static std::vector<double> latencieBoudaries{0, 0.25, 0.5, .75, 1,  2,  3,  4,
                                               5, 10,   20,  30,  40, 60, 120};
  static Z_DBLHIST counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "scilla.latency",
                           latencieBoudaries, "latency of processing", "ms"};
  return counter;
}

Z_DBLHIST &GetGasUsed() {
  static std::vector<double> latencieBoudaries{
      0, 100, 200, 300, 400, 500, 1000, 2000, 100000, 1000000};

  static Z_DBLHIST counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "gas",
                           latencieBoudaries, "amount of gas used", "zils"};
  return counter;
}

Z_DBLHIST &GetSizeUsed() {
  static std::vector<double> latencieBoudaries{0, 1000, 2000, 3000, 4000, 5000};

  static Z_DBLHIST counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "size",
                           latencieBoudaries, "size of contract", "bytes"};
  return counter;
}

Z_I64METRIC &GetCallCounter() {
  static Z_I64METRIC counter{Z_FL::ACCOUNTSTORE_HISTOGRAMS, "errors",
                             "Errors for AccountStore", "calls"};
  return counter;
}

}  // namespace local
}  // namespace zil

AccountStore::AccountStore()
    : m_db("state"),
      m_state(&m_db),
      m_accountStoreTemp(*this),
      m_scillaIPCServerConnector(SCILLA_IPC_SOCKET_PATH) {
  bool ipcScillaInit = false;

  if (ENABLE_SC || ENABLE_EVM || ISOLATED_SERVER) {
    /// Scilla IPC Server
    /// clear path
    boost::filesystem::remove_all(SCILLA_IPC_SOCKET_PATH);
    m_scillaIPCServer =
        make_shared<ScillaIPCServer>(this, m_scillaIPCServerConnector);

    if (!LOOKUP_NODE_MODE || ISOLATED_SERVER) {
      ScillaClient::GetInstance().Init();
      ipcScillaInit = true;
    }

    if (m_scillaIPCServer == nullptr) {
      LOG_GENERAL(WARNING, "m_scillaIPCServer NULL");
    } else {
      m_accountStoreTemp.SetScillaIPCServer(m_scillaIPCServer);
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

void AccountStore::InitTrie() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  m_state.init();
  m_prevRoot = m_state.root();
}

void AccountStore::InitSoft() {
  unique_lock<shared_timed_mutex> g(m_mutexPrimary);

  AccountStoreBase::Init();
  InitTrie();

  InitRevertibles();

  InitTemp();
}

Account *AccountStore::GetAccount(const Address &address) {
  return this->GetAccount(address, false);
}

Account *AccountStore::GetAccount(const Address &address, bool resetRoot) {
  using namespace boost::multiprecision;

  Account *account = AccountStoreBase::GetAccount(address);
  if (account != nullptr) {
    return account;
  }

  std::string rawAccountBase;

  {
    std::lock(m_mutexTrie, m_mutexDB);
    std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);

    if (LOOKUP_NODE_MODE && resetRoot) {
      if (m_prevRoot != dev::h256()) {
        try {
          auto t_state = m_state;
          t_state.setRoot(m_prevRoot);
          rawAccountBase =
              t_state.at(DataConversion::StringToCharArray(address.hex()));
        } catch (std::exception &e) {
          LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed, "
                                              << e.what());
          return nullptr;
        }
      }
    } else {
      rawAccountBase =
          m_state.at(DataConversion::StringToCharArray(address.hex()));
    }
  }
  if (rawAccountBase.empty()) {
    return nullptr;
  }

  account = new Account();
  if (!account->DeserializeBase(
          zbytes(rawAccountBase.begin(), rawAccountBase.end()), 0)) {
    LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
    delete account;
    return nullptr;
  }

  if (account->isContract()) {
    account->SetAddress(address);
  }

  auto it2 = this->m_addressToAccount->emplace(address, *account);

  delete account;

  return &it2.first->second;
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

  m_accountStoreTemp.Init();
  m_stateDeltaSerialized.clear();

  ContractStorage::GetContractStorage().InitTempState();
}

void AccountStore::InitRevertibles() {
  lock_guard<mutex> g(m_mutexRevertibles);

  m_addressToAccountRevChanged.clear();
  m_addressToAccountRevCreated.clear();

  ContractStorage::GetContractStorage().InitRevertibles();
}

AccountStore &AccountStore::GetInstance() {
  static AccountStore accountstore;
  return accountstore;
}

bool AccountStore::Serialize(zbytes &dst, unsigned int offset) const {
  LOG_MARKER();
  shared_lock<shared_timed_mutex> lock(m_mutexPrimary);
  std::lock_guard<std::mutex> g(m_mutexTrie);
  if (LOOKUP_NODE_MODE) {
    if (m_prevRoot != dev::h256()) {
      try {
        m_state.setRoot(m_prevRoot);
      } catch (...) {
        return false;
      }
    }
  }
  if (!MessengerAccountStoreTrie::SetAccountStoreTrie(
          dst, offset, m_state, this->m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreTrie failed.");
    return false;
  }

  return true;
}

bool AccountStore::Deserialize(const zbytes &src, unsigned int offset) {
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

bool AccountStore::Deserialize(const string &src, unsigned int offset) {
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
                                       m_accountStoreTemp, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStoreDelta failed.");
    return false;
  }

  return true;
}

void AccountStore::GetSerializedDelta(zbytes &dst) {
  lock_guard<mutex> g(m_mutexDelta);

  dst.clear();

  copy(m_stateDeltaSerialized.begin(), m_stateDeltaSerialized.end(),
       back_inserter(dst));
}

bool AccountStore::DeserializeDelta(const zbytes &src, unsigned int offset,
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
    unique_lock<shared_timed_mutex> g(m_mutexPrimary);

    if (!Messenger::GetAccountStoreDelta(src, offset, *this, revertible,
                                         false)) {
      LOG_GENERAL(WARNING, "Messenger::GetAccountStoreDelta failed.");
      return false;
    }
  }

  m_prevRoot = GetStateRootHash();

  return true;
}

bool AccountStore::DeserializeDeltaTemp(const zbytes &src,
                                        unsigned int offset) {
  lock_guard<mutex> g(m_mutexDelta);
  return m_accountStoreTemp.DeserializeDelta(src, offset);
}

bool AccountStore::MoveRootToDisk(const dev::h256 &root) {
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

  for (const auto &i : *m_addressToAccount) {
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
    for (const auto &it : code_batch) {
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
  } catch (const boost::exception &e) {
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
  } catch (const boost::exception &e) {
    LOG_GENERAL(WARNING, "Error with AccountStore::RetrieveFromDisk. "
                             << boost::diagnostic_information(e));
    return false;
  }
  return true;
}

Account *AccountStore::GetAccountTemp(const Address &address) {
  return m_accountStoreTemp.GetAccount(address);
}

Account *AccountStore::GetAccountTempAtomic(const Address &address) {
  return m_accountStoreTemp.GetAccountAtomic(address);
}

bool AccountStore::UpdateAccountsTemp(
    const uint64_t &blockNum, const unsigned int &numShards, const bool &isDS,
    const Transaction &transaction, const TxnExtras &txnExtras,
    TransactionReceipt &receipt, TxnStatus &error_code) {
  unique_lock<shared_timed_mutex> g(m_mutexPrimary, defer_lock);
  unique_lock<mutex> g2(m_mutexDelta, defer_lock);

  // start the clock

  std::chrono::system_clock::time_point tpLatencyStart = r_timer_start();

  lock(g, g2);

  bool isEvm{false};

  if (Transaction::GetTransactionType(transaction) ==
      Transaction::CONTRACT_CREATION) {
    isEvm = EvmUtils::isEvm(transaction.GetCode());
  } else {
    // We need to look at the code for any transaction type. Even if it is a
    // simple transfer, it might actually be a call.
    Account *contractAccount = this->GetAccountTemp(transaction.GetToAddr());
    if (contractAccount != nullptr) {
      isEvm = EvmUtils::isEvm(contractAccount->GetCode());
    }
  }

  if (ENABLE_EVM == false && isEvm) {
    LOG_GENERAL(WARNING,
                "EVM is disabled so not processing this EVM transaction ");
    if (zil::local::GetCallCounter().Enabled()) {
      zil::local::GetCallCounter().IncrementAttr({{"not.evm", __FUNCTION__}});
    }
    return false;
  }

  bool status;
  LOG_GENERAL(WARNING,
              "[AS] Starting to Process <" << transaction.GetTranID() << ">");
  if (isEvm) {
    EvmProcessContext context(blockNum, transaction, txnExtras);

    status = m_accountStoreTemp.UpdateAccountsEvm(blockNum, numShards, isDS,
                                                  receipt, error_code, context);
  } else {
    status = m_accountStoreTemp.UpdateAccounts(
        blockNum, numShards, isDS, transaction, txnExtras, receipt, error_code);
  }
  LOG_GENERAL(WARNING, "[AS] Finished Processing <"
                           << transaction.GetTranID() << "> ("
                           << (status ? "Successfully)" : "Failed)"));

  // This needs to be outside the above as needs to include possibility of non evm tx
  if(ARCHIVAL_LOOKUP_WITH_TX_TRACES && transaction.GetTranID()) {

    if (!BlockStorage::GetBlockStorage().PutOtterAddressNonceLookup(transaction.GetTranID(),
                                                                    transaction.GetNonce() - 1, transaction.GetSenderAddr().hex())) {
      LOG_GENERAL(INFO,
                  "FAIL: Put otter addr nonce failed " << transaction.GetTranID());
    }

    // For when vanilla TX, we still want to log this for otterscan
    if (!isEvm) {
      std::set<std::string> addresses_touched;
      addresses_touched.insert(transaction.GetSenderAddr().hex());
      addresses_touched.insert(transaction.GetToAddr().hex());

      if (!BlockStorage::GetBlockStorage().PutOtterTxAddressMapping(transaction.GetTranID(),
                                                                    addresses_touched, blockNum)) {
        LOG_GENERAL(INFO,
                    "FAIL: Put otter tx addr mapping failed " << transaction.GetTranID());
      }
    }
  }

  // Record and publish delay
  auto delay = r_timer_end(tpLatencyStart);
  double dVal = delay / 1000;
  if (dVal > 0) {
    if (isEvm && zil::local::GetEvmLatency().Enabled()) {
      zil::local::GetEvmLatency().Record(
          (dVal), {{status ? "passed" : "failed", __FUNCTION__}});
    }
    if (not isEvm && zil::local::GetScillaLatency().Enabled()) {
      zil::local::GetScillaLatency().Record(
          (dVal), {{status ? "passed" : "failed", __FUNCTION__}});
    }
    if (zil::local::GetGasUsed().Enabled()) {
      double gasUsed = receipt.GetCumGas();
      zil::local::GetGasUsed().Record(
          gasUsed, {{isEvm ? "evm" : "scilla", __FUNCTION__}});
    }
    if (zil::local::GetSizeUsed().Enabled()) {
      if (not transaction.GetCode().empty()) {
        double size = transaction.GetCode().size();
        zil::local::GetSizeUsed().Record(
            size, {{isEvm ? "evm" : "scilla", __FUNCTION__}});
      }
    }
  }
  return status;
}

bool AccountStore::UpdateCoinbaseTemp(const Address &rewardee,
                                      const Address &genesisAddress,
                                      const uint128_t &amount) {
  lock_guard<mutex> g(m_mutexDelta);

  if (m_accountStoreTemp.GetAccount(rewardee) == nullptr) {
    m_accountStoreTemp.AddAccount(rewardee, {0, 0});
  }
  return m_accountStoreTemp.TransferBalance(genesisAddress, rewardee, amount);
  // Should the nonce increase ??
}

uint128_t AccountStore::GetNonceTemp(const Address &address) {
  lock_guard<mutex> g(m_mutexDelta);

  if (m_accountStoreTemp.GetAddressToAccount()->find(address) !=
      m_accountStoreTemp.GetAddressToAccount()->end()) {
    return m_accountStoreTemp.GetNonce(address);
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

  SHA256Calculator sha2;
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
  for (auto const &entry : m_addressToAccountRevChanged) {
    m_addressToAccount->insert_or_assign(entry.first, entry.second);
    UpdateStateTrie(entry.first, entry.second);
  }
  for (auto const &entry : m_addressToAccountRevCreated) {
    RemoveAccount(entry.first);
    RemoveFromTrie(entry.first);
  }

  ContractStorage::GetContractStorage().RevertContractStates();

  return true;
}

void AccountStore::NotifyTimeoutTemp() { m_accountStoreTemp.NotifyTimeout(); }

bool AccountStore::GetProof(const Address &address, const dev::h256 &rootHash,
                            Account &account, std::set<std::string> &nodes) {
  if (!LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING, "not lookup node");
    return false;
  }

  std::string rawAccountBase;

  dev::h256 t_rootHash = (rootHash == dev::h256()) ? m_prevRoot : rootHash;

  LOG_GENERAL(INFO, "RootHash " << t_rootHash.hex());

  {
    std::lock(m_mutexTrie, m_mutexDB);
    std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
    std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);

    auto t_state = m_state;

    if (t_rootHash != dev::h256()) {
      try {
        t_state.setRoot(t_rootHash);
      } catch (std::exception &e) {
        LOG_GENERAL(WARNING, "setRoot for " << t_rootHash.hex() << " failed "
                                            << e.what());
        return false;
      }
    }

    rawAccountBase = t_state.getProof(
        DataConversion::StringToCharArray(address.hex()), nodes);
  }

  if (rawAccountBase.empty()) {
    return false;
  }

  Account t_account;
  if (!t_account.DeserializeBase(
          zbytes(rawAccountBase.begin(), rawAccountBase.end()), 0)) {
    LOG_GENERAL(WARNING, "Account::DeserializeBase failed");
    return false;
  }

  if (t_account.isContract()) {
    t_account.SetAddress(address);
  }

  account = std::move(t_account);

  return true;
}

bool AccountStore::UpdateStateTrie(const Address &address,
                                   const Account &account) {
  zbytes rawBytes;
  if (!account.SerializeBase(rawBytes, 0)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
    return false;
  }

  std::lock(m_mutexTrie, m_mutexCache);
  std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
  std::lock_guard<std::mutex> lock2(m_mutexCache, std::adopt_lock);


  zbytes z = DataConversion::StringToCharArray(address.hex());
  if(!m_state.contains(z)){
    std::array<unsigned char, 40> arr;
    std::copy(z.begin(), z.end(), arr.begin());
    m_cache.push_back(arr);
  }
  m_state.insert(z, rawBytes);

  return true;
}

bool AccountStore::RemoveFromTrie(const Address &address) {
  // LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexTrie);

  m_state.remove(DataConversion::StringToCharArray(address.hex()));

  return true;
}

dev::h256 AccountStore::GetStateRootHash() const {
  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_state.root();
}

dev::h256 AccountStore::GetPrevRootHash() const {
  std::lock_guard<std::mutex> g(m_mutexTrie);

  return m_prevRoot;
}

bool AccountStore::UpdateStateTrieAll() {
  std::lock_guard<std::mutex> g(m_mutexTrie);
  if (m_prevRoot != dev::h256()) {
    try {
      m_state.setRoot(m_prevRoot);
    } catch (...) {
      LOG_GENERAL(WARNING, "setRoot for " << m_prevRoot.hex() << " failed");
      return false;
    }
  }
  for (auto const &entry : *(this->m_addressToAccount)) {
    zbytes rawBytes;
    if (!entry.second.SerializeBase(rawBytes, 0)) {
      LOG_GENERAL(WARNING, "Messenger::SetAccountBase failed");
      return false;
    }
    m_state.insert(DataConversion::StringToCharArray(entry.first.hex()),
                   rawBytes);
  }

  m_prevRoot = m_state.root();

  return true;
}

void AccountStore::PrintAccountState() {
  AccountStoreBase::PrintAccountState();
  LOG_GENERAL(INFO, "State Root: " << GetStateRootHash());
}

void AccountStore::FillAddressCache(){
  std::lock(m_mutexTrie, m_mutexDB, m_mutexCache);
  std::lock_guard<std::mutex> lock1(m_mutexTrie, std::adopt_lock);
  std::lock_guard<std::mutex> lock2(m_mutexDB, std::adopt_lock);
  std::lock_guard<std::mutex> lock3(m_mutexCache, std::adopt_lock);

  m_cache.clear();

  for(auto it = m_state.begin(); it != m_state.end(); ++it){
    std::pair<zbytesConstRef , zbytesConstRef >item = it.at();
    std::array<unsigned char, 40> arr;
    std::copy(item.first.begin(), item.first.end(), arr.begin());
    m_cache.push_back(arr);
  }
}

void AccountStore::PrintAddressCache(){
  for (const std::array<unsigned char, 40>& entry : m_cache) {
    std::string address(entry.begin(), entry.end());
    LOG_GENERAL(INFO, "Address: " << address);
  }
}  
