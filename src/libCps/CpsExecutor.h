/*
 * Copyright (C) 2022 Zilliqa
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

/*
 * CpsExecutor is the gateway for each transaction that invokes any contract
 * operation. It keeps a stack of CpsRun jobs that are processed till all is
 * done or an error occurs. There are two methods used as entry points:
 * RunFromScilla and RunFromEvm.
 */

#ifndef ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_
#define ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_

#include "CpsAccountStoreInterface.h"
#include "CpsExecuteResult.h"

#include <memory>
#include <variant>
#include <vector>

class EvmProcessContext;
struct ScillaProcessContext;
class TransactionReceipt;

namespace libCps {
struct CpsContext;
class CpsRun;
class CpsExecutor final {
  using Address = dev::h160;

 public:
  CpsExecutor(CpsAccountStoreInterface& account_store,
              TransactionReceipt& receipt);
  ~CpsExecutor();
  CpsExecuteResult RunFromEvm(EvmProcessContext& context);
  CpsExecuteResult RunFromScilla(ScillaProcessContext& context);
  void PushRun(std::shared_ptr<CpsRun> run);
  CpsAccountStoreInterface& GetAccStoreIface() { return mAccountStore; }
  void TxTraceClear();
  std::string& CurrentTrace();

 private:
  CpsExecuteResult PreValidateEvmRun(const EvmProcessContext& context) const;
  CpsExecuteResult PreValidateScillaRun(
      const ScillaProcessContext& context) const;
  void InitRun();
  void RefundGas(
      const std::variant<EvmProcessContext, ScillaProcessContext>& context,
      uint64_t gasRemainedCore);
  void TakeGasFromAccount(
      const std::variant<EvmProcessContext, ScillaProcessContext>& context);
  CpsExecuteResult processLoop(const CpsContext& context);
  uint64_t GetRemainedGasCore(const CpsExecuteResult& execResult) const;

 private:
  CpsAccountStoreInterface& mAccountStore;
  TransactionReceipt& mTxReceipt;
  std::vector<std::shared_ptr<CpsRun>> m_queue;
  std::string m_txTrace;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_
