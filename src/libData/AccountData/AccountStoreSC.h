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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORESC_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORESC_H_

#include <json/json.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "AccountStoreBase.h"
#include "libUtils/DetachedFunction.h"

class ScillaIPCServer;

template <class MAP>
class AccountStoreSC;

template <class MAP>
class AccountStoreAtomic
    : public AccountStoreBase<std::unordered_map<Address, Account>> {
  AccountStoreSC<MAP>& m_parent;

 public:
  AccountStoreAtomic(AccountStoreSC<MAP>& parent);

  Account* GetAccount(const Address& address) override;

  const std::shared_ptr<std::unordered_map<Address, Account>>&
  GetAddressToAccount();
};

template <class MAP>
class AccountStoreSC : public AccountStoreBase<MAP> {
  /// the amount transfers happened within the current txn will only commit when
  /// the txn is successful
  std::unique_ptr<AccountStoreAtomic<MAP>> m_accountStoreAtomic;

  /// mutex to block major accounts changes
  std::mutex m_mutexUpdateAccounts;

  /// the blocknum while executing each txn
  uint64_t m_curBlockNum{0};

  /// the current contract address for each hop of invoking
  Address m_curContractAddr;

  /// the current sender address for each hop of invoking
  Address m_curSenderAddr;

  /// the transfer amount while executing each txn
  uint128_t m_curAmount{0};

  /// the gas limit while executing each txn
  uint64_t m_curGasLimit{0};

  /// the gas price while executing each txn
  uint128_t m_curGasPrice{GAS_PRICE_MIN_VALUE};

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
  std::condition_variable cv_callContract;
  std::atomic<bool> m_txnProcessTimeout;

  /// scilla IPC server
  std::shared_ptr<ScillaIPCServer> m_scillaIPCServer;

  /// A set of contract account address pending for storageroot updating
  std::set<Address> m_storageRootUpdateBuffer;

  /// A set of contract account address pending for storageroot updating
  /// for each transaction, will be added to the non-atomic one once
  /// the transaction succeeded
  std::set<Address> m_storageRootUpdateBufferAtomic;

  std::vector<Address> m_newLibrariesCreated;

  /// Contract Deployment
  /// verify the return from scilla_runner for deployment is valid
  bool ParseCreateContract(uint64_t& gasRemained,
                           const std::string& runnerPrint,
                           TransactionReceipt& receipt, bool is_library);
  /// convert the interpreter output into parsable json object for deployment
  bool ParseCreateContractOutput(Json::Value& jsonOutput,
                                 const std::string& runnerPrint,
                                 TransactionReceipt& receipt);
  /// parse the output from interpreter for deployment
  bool ParseCreateContractJsonOutput(const Json::Value& _json,
                                     uint64_t& gasRemained,
                                     TransactionReceipt& receipt,
                                     bool is_library);

  /// Contract Calling
  /// verify the return from scilla_runner for calling is valid
  bool ParseCallContract(uint64_t& gasRemained, const std::string& runnerPrint,
                         TransactionReceipt& receipt, uint32_t tree_depth,
                         uint32_t scilla_version);
  /// convert the interpreter output into parsable json object for calling
  bool ParseCallContractOutput(Json::Value& jsonOutput,
                               const std::string& runnerPrint,
                               TransactionReceipt& receipt);
  /// parse the output from interpreter for calling and update states
  bool ParseCallContractJsonOutput(const Json::Value& _json,
                                   uint64_t& gasRemained,
                                   TransactionReceipt& receipt,
                                   uint32_t tree_depth,
                                   uint32_t pre_scilla_version);

  /// Utility functions
  /// get the json format file for the current blocknum
  Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;
  /// get the command for invoking the scilla_checker while deploying
  std::string GetContractCheckerCmdStr(const std::string& root_w_version,
                                       bool is_library,
                                       const uint64_t& available_gas);
  /// get the command for invoking the scilla_runner while deploying
  std::string GetCreateContractCmdStr(
      const std::string& root_w_version, bool is_library,
      const uint64_t& available_gas,
      const boost::multiprecision::uint128_t& balance);
  /// get the command for invoking the scilla_runner while calling
  std::string GetCallContractCmdStr(
      const std::string& root_w_version, const uint64_t& available_gas,
      const boost::multiprecision::uint128_t& balance);
  /// updating m_root_w_version
  bool PrepareRootPathWVersion(const uint32_t& scilla_version);

  /// export files that ExportCreateContractFiles and ExportContractFiles
  /// both needs
  void ExportCommonFiles(
      std::ofstream& os, const Account& contract,
      const std::map<Address, std::pair<std::string, std::string>>&
          extlibs_exports);

  /// generate the files for initdata, contract state, blocknum for interpreter
  /// to call contract
  bool ExportContractFiles(
      Account& contract, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>&
          extlibs_exports);
  /// generate the files for message from txn for interpreter to call contract
  bool ExportCallContractFiles(
      Account& contract, const Transaction& transaction,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>&
          extlibs_exports);
  /// generate the files for message from previous contract output for
  /// interpreter to call another contract
  bool ExportCallContractFiles(
      Account& contract, const Json::Value& contractData,
      uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>&
          extlibs_exports);

  /// Amount Transfer
  /// add amount transfer to the m_accountStoreAtomic
  bool TransferBalanceAtomic(const Address& from, const Address& to,
                             const uint128_t& delta);
  /// commit the existing transfers in m_accountStoreAtomic to update the
  /// balance of accounts
  void CommitTransferAtomic();
  /// discard the existing transfers in m_accountStoreAtomic
  void DiscardTransferAtomic();

  bool PopulateExtlibsExports(
      uint32_t scilla_version, const std::vector<Address>& extlibs,
      std::map<Address, std::pair<std::string, std::string>>& extlibs_exports);

 protected:
  AccountStoreSC();

  /// generate input files for interpreter to deploy contract
  bool ExportCreateContractFiles(
      const Account& contract, bool is_library, uint32_t scilla_version,
      const std::map<Address, std::pair<std::string, std::string>>&
          extlibs_export);

  /// capsulate and expose in protected for using by data migartion
  void InvokeScillaChecker(std::string& checkerPrint, bool& ret_checker,
                           int& pid, const uint64_t& gasRemained,
                           TransactionReceipt& receipt, bool is_library);

  /// verify the return from scilla_checker for deployment is valid
  /// expose in protected for using by data migration
  bool ParseContractCheckerOutput(const std::string& checkerPrint,
                                  TransactionReceipt& receipt,
                                  bytes& map_depth_data, uint64_t& gasRemained,
                                  bool is_library = false);

  /// external interface for processing txn
  bool UpdateAccounts(const uint64_t& blockNum, const unsigned int& numShards,
                      const bool& isDS, const Transaction& transaction,
                      TransactionReceipt& receipt);

 public:
  /// Initialize the class
  void Init() override;

  /// external interface for calling timeout for txn processing
  void NotifyTimeout();

  /// public interface to setup scilla ipc server
  virtual void SetScillaIPCServer(
      std::shared_ptr<ScillaIPCServer> scillaIPCServer);

  /// public interface to invoke processing of the buffered storage root
  /// updating tasks
  void ProcessStorageRootUpdateBuffer();

  /// public interface to clean StorageRootUpdateBuffer
  void CleanStorageRootUpdateBuffer();

  /// Clean cache of newly created contracts in this epoch
  void CleanNewLibrariesCache();
};

#include "AccountStoreAtomic.tpp"
#include "AccountStoreSC.tpp"

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_ACCOUNTSTORESC_H_
