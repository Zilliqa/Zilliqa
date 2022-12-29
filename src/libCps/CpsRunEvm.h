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

#ifndef ZILLIQA_SRC_LIBCPS_CPSRUNEVM_H_
#define ZILLIQA_SRC_LIBCPS_CPSRUNEVM_H_

#include "libCps/CpsExecuteResult.h"
#include "libCps/CpsRun.h"
#include "libUtils/Evm.pb.h"

class TransactionReceipt;

namespace libCps {
class CpsAccountStoreInterface;
class CpsContext;
class CpsExecutor;
class CpsRunEvm final : public CpsRun {
 public:
  CpsRunEvm(evm::EvmArgs proto_args, CpsExecutor& executor,
            const CpsContext& ctx);
  virtual CpsExecuteResult Run(CpsAccountStoreInterface& account_store,
                               TransactionReceipt& receipt) override;
  void ProvideFeedback(const CpsRunEvm& previousRun,
                       const evm::EvmResult& result);
  bool IsResumable() const;

 private:
  std::optional<evm::EvmResult> InvokeEvm();
  void HandleApply(const evm::EvmResult& evmResult, TransactionReceipt& receipt,
                   CpsAccountStoreInterface& accountStore);

  CpsExecuteResult HandleTrap(const evm::EvmResult& evm_result,
                              CpsAccountStoreInterface& accountStore);
  CpsExecuteResult HandleCreateTrap(const evm::EvmResult& evm_result,
                                    CpsAccountStoreInterface& accountStore);
  CpsExecuteResult ValidateCreateTrap(const evm::TrapData_Create& createData,
                                      CpsAccountStoreInterface& accountStore,
                                      uint64_t remainingGas);

 public:
  evm::EvmArgs mProtoArgs;
  CpsExecutor& mExecutor;
  const CpsContext& mCpsContext;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSRUNEVM_H_