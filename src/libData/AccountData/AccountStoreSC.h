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
  std::unique_ptr<AccountStoreAtomic<MAP>> m_accountStoreAtomic;

  std::mutex m_mutexUpdateAccounts;

  uint64_t m_curBlockNum;
  Address m_curContractAddr;
  Address m_curSenderAddr;

  boost::multiprecision::uint128_t m_curAmount;
  uint64_t m_curGasLimit;
  boost::multiprecision::uint128_t m_curGasPrice;

  unsigned int m_curNumShards;
  bool m_curIsDS;

  std::string m_root_w_version;

  unsigned int m_curDepth = 0;

  std::mutex m_MutexCVCallContract;
  std::condition_variable cv_callContract;
  std::atomic<bool> m_txnProcessTimeout;

  bool ParseContractCheckerOutput(const std::string& checkerPrint,
                                  TransactionReceipt& receipt);

  bool ParseCreateContract(uint64_t& gasRemained,
                           const std::string& runnerPrint,
                           TransactionReceipt& receipt);
  bool ParseCreateContractJsonOutput(const Json::Value& _json,
                                     uint64_t& gasRemained,
                                     TransactionReceipt& receipt);
  bool ParseCallContract(uint64_t& gasRemained, const std::string& runnerPrint,
                         TransactionReceipt& receipt, bool first = true);
  bool ParseCallContractJsonOutput(const Json::Value& _json,
                                   uint64_t& gasRemained,
                                   TransactionReceipt& receipt, bool first);

  Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;

  std::string GetContractCheckerCmdStr(const std::string& root_w_version);
  std::string GetCreateContractCmdStr(const std::string& root_w_version,
                                      const uint64_t& available_gas);
  std::string GetCallContractCmdStr(const std::string& root_w_version,
                                    const uint64_t& available_gas);

  bool PrepareRootPathWVersion(const uint32_t& scilla_version);

  // Generate input for interpreter to check the correctness of contract
  bool ExportCreateContractFiles(const Account& contract);

  bool ExportContractFiles(const Account& contract);
  bool ExportCallContractFiles(const Account& contract,
                               const Transaction& transaction);
  bool ExportCallContractFiles(const Account& contract,
                               const Json::Value& contractData);

  bool TransferBalanceAtomic(const Address& from, const Address& to,
                             const boost::multiprecision::uint128_t& delta);
  void CommitTransferBalanceAtomic();
  void DiscardTransferBalanceAtomic();

 protected:
  AccountStoreSC();

 public:
  void Init() override;

  bool UpdateAccounts(const uint64_t& blockNum, const unsigned int& numShards,
                      const bool& isDS, const Transaction& transaction,
                      TransactionReceipt& receipt);

  bool ParseCreateContractOutput(Json::Value& jsonOutput,
                                 const std::string& runnerPrint,
                                 TransactionReceipt& receipt);
  bool ParseCallContractOutput(Json::Value& jsonOutput,
                               const std::string& runnerPrint,
                               TransactionReceipt& receipt);

  void NotifyTimeout();
};

#include "AccountStoreAtomic.tpp"
#include "AccountStoreSC.tpp"

#endif  // __ACCOUNTSTORESC_H__
