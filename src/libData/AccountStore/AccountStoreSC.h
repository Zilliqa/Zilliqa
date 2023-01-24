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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORESC_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORESC_H_

#include <json/json.h>
#include <opentelemetry/metrics/async_instruments.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "AccountStoreAtomic.h"
#include "AccountStoreBase.h"
#include "libData/AccountData/InvokeType.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libScilla/ScillaIPCServer.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/TxnExtras.h"

namespace zil {
namespace accountstore {
const std::string EVM_HISTOGRAM = "zilliqa.evm.histogram";
const std::string SCILLA_HISTOGRAM = "zilliqa.scilla.histogram";
//=======================================================================================
// TODO : Put these into configuration as they will need tuning.
/// Metrics callback for block number
struct counter_t {
  // These are the Non-automatic Manually set metrics
  int64_t blockNumber{0};
  int64_t blockNumberDS{0};
  int64_t evmCall{0};
  int64_t scillaCall{0};
};
//=======================================================================================
}  // namespace accountstore
};  // namespace zil

class ScillaIPCServer;

class AccountStoreSC : public AccountStoreBase {
  friend class AccountStoreCpsInterface;
  /// the amount transfers happened within the current txn will only commit when
  /// the txn is successful
  std::unique_ptr<AccountStoreAtomic> m_accountStoreAtomic;

  /// mutex to block major accounts changes
  std::mutex m_mutexUpdateAccounts;

  /// the blocknum while executing each txn
  uint64_t m_curBlockNum{0};

  /// the DS blocknum
  uint64_t m_curDSBlockNum{0};

  /// the current contract address for each hop of invoking
  Address m_curContractAddr;

  /// the current sender address for each hop of invoking
  Address m_curSenderAddr;

  /// the address of transaction sender
  Address m_originAddr;

  /// the transfer amount while executing each txn
  uint128_t m_curAmount{0};

  /// the gas limit while executing each txn
  uint64_t m_curGasLimit{0};

  /// the gas price while executing each txn
  uint128_t m_curGasPrice{0};

  /// the gas price while executing each txn will be used in calculating the
  /// shard allocation of sender/recipient during chain call
  unsigned int m_curNumShards{0};

  /// whether is processed by ds node while executing each txn
  bool m_curIsDS{false};

  /// the interpreter path for each hop of invoking
  std::string m_root_w_version;

  /// the depth of chain call while executing the current txn
  unsigned int m_curEdges{0};

  /// for contract execution timeout
  std::mutex m_MutexCVCallContract;
  std::condition_variable m_CallContractConditionVariable;
  std::atomic<bool> m_txnProcessTimeout;

  /// Scilla IPC server
  std::shared_ptr<ScillaIPCServer> m_scillaIPCServer;

  /// A set of contract account address pending for storageroot updating
  std::set<Address> m_storageRootUpdateBuffer;

  /// A set of contract account address pending for storageroot updating
  /// for each transaction, will be added to the non-atomic one once
  /// the transaction succeeded
  std::set<Address> m_storageRootUpdateBufferAtomic;

  std::vector<Address> m_newLibrariesCreated;

  //=======================================================================================
  // TODO : Put these into configuration as they will need tuning.
  std::list<double> m_latencieBoudaries{0,  1,  2,  4,  6,  8,
                                        10, 20, 30, 40, 60, 120};

  std::shared_ptr<zil::accountstore::counter_t> GetGeneralStatistics() {
    std::shared_ptr<zil::accountstore::counter_t> stats =
        std::make_shared<zil::accountstore::counter_t>();
    return stats;
  }

  // shared Area for simply assigning metrics;
  //=======================================================================================

  /// Contract Deployment
  /// verify the return from scilla_runner for deployment is valid
  bool ParseCreateContract(uint64_t &gasRemained,
                           const std::string &runnerPrint,
                           TransactionReceipt &receipt, bool is_library);

  /// convert the interpreter output into parsable json object for deployment
  bool ParseCreateContractOutput(Json::Value &jsonOutput,
                                 const std::string &runnerPrint,
                                 TransactionReceipt &receipt);

  /// parse the output from interpreter for deployment
  bool ParseCreateContractJsonOutput(const Json::Value &_json,
                                     uint64_t &gasRemained,
                                     TransactionReceipt &receipt,
                                     bool is_library);

  /// Contract Calling
  /// verify the return from scilla_runner for calling is valid
  bool ParseCallContract(uint64_t &gasRemained, const std::string &runnerPrint,
                         TransactionReceipt &receipt, uint32_t tree_depth,
                         uint32_t scilla_version);

  /// convert the interpreter output into parsable json object for calling
  bool ParseCallContractOutput(Json::Value &jsonOutput,
                               const std::string &runnerPrint,
                               TransactionReceipt &receipt);

  /// parse the output from interpreter for calling and update states
  bool ParseCallContractJsonOutput(const Json::Value &_json,
                                   uint64_t &gasRemained,
                                   TransactionReceipt &receipt,
                                   uint32_t tree_depth,
                                   uint32_t pre_scilla_version);

  /// export files that ExportCreateContractFiles and ExportContractFiles
  /// both needs
  void ExportCommonFiles(
      std::ofstream &os, const Account &contract,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  /// generate the files for initdata, contract state, blocknum for interpreter
  /// to call contract
  bool ExportContractFiles(
      Account &contract, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  /// generate the files for message from txn for interpreter to call contract
  bool ExportCallContractFiles(
      Account &contract, const Transaction &transaction,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  /// generate the files for message from previous contract output for
  /// interpreter to call another contract
  bool ExportCallContractFiles(
      Account &contract, const Json::Value &contractData,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports);

  void EvmCallRunner(const INVOKE_TYPE invoke_type, const evm::EvmArgs &args,
                     bool &ret, TransactionReceipt &receipt,
                     evm::EvmResult &result);

  void CreateScillaCodeFiles(
      Account &contract,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_exports,
      const std::string &scillaCodeExtension);

  /// Amount Transfer
  /// add amount transfer to the m_accountStoreAtomic
  bool TransferBalanceAtomic(const Address &from, const Address &to,
                             const uint128_t &delta);

  /// commit the existing transfers in m_accountStoreAtomic to update the
  /// balance of accounts
  void CommitAtomics();

  /// discard the existing transfers in m_accountStoreAtomic
  void DiscardAtomics();

 protected:
  AccountStoreSC();

  const uint64_t &getCurBlockNum() const { return m_curBlockNum; }

  const uint64_t &getCurDSBlockNum() const { return m_curDSBlockNum; }

  /// generate input files for interpreter to deploy contract
  bool ExportCreateContractFiles(
      const Account &contract, bool is_library, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>
          &extlibs_export);

  /// invoke scilla interpreter
  void InvokeInterpreter(INVOKE_TYPE invoke_type,
                         std::string &interprinterPrint,
                         const uint32_t &version, bool is_library,
                         const uint64_t &available_gas,
                         const boost::multiprecision::uint128_t &balance,
                         bool &ret, TransactionReceipt &receipt);

  uint64_t InvokeEvmInterpreter(Account *contractAccount,
                                INVOKE_TYPE invoke_type,
                                const evm::EvmArgs &args, bool &ret,
                                TransactionReceipt &receipt,
                                evm::EvmResult &result);

  /// verify the return from scilla_checker for deployment is valid
  /// expose in protected for using by data migration
  bool ParseContractCheckerOutput(const Address &addr,
                                  const std::string &checkerPrint,
                                  TransactionReceipt &receipt,
                                  std::map<std::string, zbytes> &metadata,
                                  uint64_t &gasRemained,
                                  bool is_library = false);

  /// external interface for processing txn
  bool UpdateAccounts(const uint64_t &blockNum, const unsigned int &numShards,
                      const bool &isDS, const Transaction &transaction,
                      TransactionReceipt &receipt, TxnStatus &error_code);

  bool PopulateExtlibsExports(
      uint32_t scilla_version, const std::vector<Address> &extlibs,
      std::map<Address, std::pair<std::string, std::string>> &extlibs_exports);

 public:
  /// Initialize the class
  static double m_transactionLatency;

  void Init() override;

  /// external interface for calling timeout for txn processing
  void NotifyTimeout();

  bool UpdateAccountsEvm(const uint64_t &blockNum,
                         const unsigned int &numShards, const bool &isDS,
                         TransactionReceipt &receipt, TxnStatus &error_code,
                         EvmProcessContext &evmContext);

  /// public interface to setup scilla ipc server
  void SetScillaIPCServer(std::shared_ptr<ScillaIPCServer> scillaIPCServer);

  /// public interface to invoke processing of the buffered storage root
  /// updating tasks
  void ProcessStorageRootUpdateBuffer();

  /// public interface to clean StorageRootUpdateBuffer
  void CleanStorageRootUpdateBuffer();

  /// Clean cache of newly created contracts in this epoch
  void CleanNewLibrariesCache();

  // Get value from atomic accountstore
  Account *GetAccountAtomic(const dev::h160 &addr);

  // Adds an Account to the atomic AccountStore.
  bool AddAccountAtomic(const Address &address, const Account &account);

  bool EvmProcessMessage(EvmProcessContext &params, evm::EvmResult &result);

  zil::accountstore::counter_t m_stats;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_ACCOUNTSTORESC_H_
