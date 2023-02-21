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

#ifndef ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_
#define ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_

#include "CpsAccountStoreInterface.h"
#include "CpsExecuteResult.h"

#include <memory>
#include <vector>

class EvmProcessContext;
class TransactionReceipt;

namespace libCps {
class CpsRun;
class CpsExecutor final {
 public:
  explicit CpsExecutor(CpsAccountStoreInterface& account_store,
                       TransactionReceipt& receipt);
  ~CpsExecutor();
  CpsExecuteResult Run(EvmProcessContext& context);
  void PushRun(std::shared_ptr<CpsRun> run);
  CpsAccountStoreInterface& GetAccStoreIface() { return mAccountStore; }

 private:
  CpsExecuteResult PreValidateRun(const EvmProcessContext& context) const;
  void InitRun();
  void RefundGas(const EvmProcessContext& context,
                 const CpsExecuteResult& runResult);
  void TakeGasFromAccount(const EvmProcessContext& context);

 private:
  CpsAccountStoreInterface& mAccountStore;
  TransactionReceipt& mTxReceipt;
  std::vector<std::shared_ptr<CpsRun>> m_queue;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSEXECUTOR_H_
