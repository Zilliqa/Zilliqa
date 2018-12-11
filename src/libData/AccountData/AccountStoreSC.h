/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef __ACCOUNTSTORESC_H__
#define __ACCOUNTSTORESC_H__

#include <json/json.h>
#include <mutex>

#include "AccountStoreBase.h"

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
  TransactionReceipt m_curTranReceipt;

  unsigned int m_curDepth = 0;

  bool ParseContractCheckerOutput(const std::string& checkerPrint);

  bool ParseCreateContract(uint64_t& gasRemained,
                           const std::string& runnerPrint);
  bool ParseCreateContractOutput(Json::Value& jsonOutput,
                                 const std::string& runnerPrint = "");
  bool ParseCreateContractJsonOutput(const Json::Value& _json,
                                     uint64_t& gasRemained);

  bool ParseCallContract(uint64_t& gasRemained, const std::string& runnerPrint);
  bool ParseCallContractOutput(Json::Value& jsonOutput,
                               const std::string& runnerPrint = "");
  bool ParseCallContractJsonOutput(const Json::Value& _json,
                                   uint64_t& gasRemained);

  Json::Value GetBlockStateJson(const uint64_t& BlockNum) const;

  std::string GetContractCheckerCmdStr();
  std::string GetCreateContractCmdStr(const uint64_t& available_gas);
  std::string GetCallContractCmdStr(const uint64_t& available_gas);

  // Generate input for interpreter to check the correctness of contract
  void ExportCreateContractFiles(const Account& contract);

  void ExportContractFiles(const Account& contract);
  bool ExportCallContractFiles(const Account& contract,
                               const Transaction& transaction);
  void ExportCallContractFiles(const Account& contract,
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
};

#include "AccountStoreAtomic.tpp"
#include "AccountStoreSC.tpp"

#endif  // __ACCOUNTSTORESC_H__
