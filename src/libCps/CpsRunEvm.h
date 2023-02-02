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
class Address;
class CpsAccountStoreInterface;
class CpsContext;
class CpsExecutor;
class CpsRunEvm final : public CpsRun {
  using Address = dev::h160;

 public:
  CpsRunEvm(evm::EvmArgs proto_args, CpsExecutor& executor, CpsContext& ctx,
            CpsRun::Type type);
  virtual CpsExecuteResult Run(TransactionReceipt& receipt) override;
  void ProvideFeedback(const CpsRun& previousRun,
                       const CpsExecuteResult& results) override;
  bool IsResumable() const override;
  bool HasFeedback() const override;

 private:
  std::optional<evm::EvmResult> InvokeEvm();
  void HandleApply(const evm::EvmResult& evmResult,
                   TransactionReceipt& receipt);

  CpsExecuteResult HandleTrap(const evm::EvmResult& evm_result);
  CpsExecuteResult HandleCallTrap(const evm::EvmResult& evm_result);
  CpsExecuteResult ValidateCallTrap(const evm::TrapData_Call& callData,
                                    uint64_t remainingGas);

  CpsExecuteResult HandleCreateTrap(const evm::EvmResult& evm_result);
  CpsExecuteResult ValidateCreateTrap(const evm::TrapData_Create& createData,
                                      uint64_t remainingGas);
  void InstallCode(const Address& address, const std::string& code);

 private:
  evm::EvmArgs mProtoArgs;
  CpsExecutor& mExecutor;
  CpsContext& mCpsContext;
};

}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_CPSRUNEVM_H_
