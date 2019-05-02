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

#ifndef __ACCOUNTSTORESC_H__
#define __ACCOUNTSTORESC_H__

#include <json/json.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>

#include "AccountStoreBase.h"
#include "libUtils/DetachedFunction.h"

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

  std::mutex m_mutexUpdateAccounts;

  /// the blocknum while executing each txn
  uint64_t m_curBlockNum;

  /// the current contract address for each hop of invoking
  Address m_curContractAddr;

  /// the current sender address for each hop of invoking
  Address m_curSenderAddr;

  /// the transfer amount while executing each txn
  uint128_t m_curAmount;

  /// the gas limit while executing each txn
  uint64_t m_curGasLimit;

  /// the gas price while executing each txn
  uint128_t m_curGasPrice;

  /// the gas price while executing each txn will be used in calculating the
  /// shard allocation of sender/recipient during chain call
  unsigned int m_curNumShards;

  /// whether is processed by ds node while executing each txn
  bool m_curIsDS;

  /// the interpreter path for each hop of invoking
  std::string m_root_w_version;

  /// the depth of chain call while executing the current txn
  unsigned int m_curDepth = 0;

  /// for contract execution timeout
  std::mutex m_MutexCVCallContract;
  std::condition_variable cv_callContract;
  std::atomic<bool> m_txnProcessTimeout;

  /// Contract Deployment
  /// verify the return from scilla_checker for deployment is valid
  bool ParseContractCheckerOutput(const std::string& checkerPrint,
                                  TransactionReceipt& receipt);

  /// verify the return from scilla_runner for deployment is valid
  bool ParseCreateContract(uint64_t& gasRemained,
                           const std::string& runnerPrint,
                           TransactionReceipt& receipt);
  /// convert the interpreter output into parsable json object for deployment
  bool ParseCreateContractOutput(Json::Value& jsonOutput,
                                 const std::string& runnerPrint,
                                 TransactionReceipt& receipt);
  /// parse the output from interpreter for deployment
  bool ParseCreateContractJsonOutput(const Json::Value& _json,
                                     uint64_t& gasRemained,
                                     TransactionReceipt& receipt);

  /// Contract Calling
  /// verify the return from scilla_runner for calling is valid
  bool ParseCallContract(uint64_t& gasRemained, const std::string& runnerPrint,
                         TransactionReceipt& receipt, bool temp,
                         bool first = true);
  /// convert the interpreter output into parsable json object for calling
  bool ParseCallContractOutput(Json::Value& jsonOutput,
                               const std::string& runnerPrint,
                               TransactionReceipt& receipt);
  /// parse the output from interpreter for calling and update states
  bool ParseCallContractJsonOutput(const Json::Value& _json,
                                   uint64_t& gasRemained,
                                   TransactionReceipt& receipt, bool first,
                                   bool temp);

  /// Utility functions
  /// get the json format file for the current blocknum
  Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;
  /// get the command for invoking the scilla_checker while deploying
  std::string GetContractCheckerCmdStr(const std::string& root_w_version);
  /// get the command for invoking the scilla_runner while deploying
  std::string GetCreateContractCmdStr(const std::string& root_w_version,
                                      const uint64_t& available_gas);
  /// get the command for invoking the scilla_runner while calling
  std::string GetCallContractCmdStr(const std::string& root_w_version,
                                    const uint64_t& available_gas);
  /// updating m_root_w_version
  bool PrepareRootPathWVersion(const uint32_t& scilla_version);

  /// generate input files for interpreter to deploy contract
  bool ExportCreateContractFiles(const Account& contract);

  /// generate the files for initdata, contract state, blocknum for interpreter
  /// to call contract
  bool ExportContractFiles(const Account& contract);
  /// generate the files for message from txn for interpreter to call contract
  bool ExportCallContractFiles(const Account& contract,
                               const Transaction& transaction);
  /// generate the files for message from previous contract output for
  /// interpreter to call another contract
  bool ExportCallContractFiles(const Account& contract,
                               const Json::Value& contractData);

  /// Amount Transfer
  /// add amount transfer to the m_accountStoreAtomic
  bool TransferBalanceAtomic(const Address& from, const Address& to,
                             const uint128_t& delta);
  /// commit the existing transfers in m_accountStoreAtomic to update the
  /// balance of accounts
  void CommitTransferAtomic();
  /// discard the existing transfers in m_accountStoreAtomic
  void DiscardTransferAtomic();

 protected:
  AccountStoreSC();

 public:
  /// Initialize the class
  void Init() override;

  /// external interface for processing txn
  bool UpdateAccounts(const uint64_t& blockNum, const unsigned int& numShards,
                      const bool& isDS, const Transaction& transaction,
                      TransactionReceipt& receipt, bool temp = false);
  /// external interface for calling timeout for txn processing
  void NotifyTimeout();
};

#include "AccountStoreAtomic.tpp"
#include "AccountStoreSC.tpp"

#endif  // __ACCOUNTSTORESC_H__
