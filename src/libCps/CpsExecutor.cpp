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

#include "libCps/CpsExecutor.h"
#include "libCps/Amount.h"
#include "libCps/CpsContext.h"
#include "libCps/CpsExecuteValidator.h"
#include "libCps/CpsRunEvm.h"

#include "libData/AccountData/EvmProcessContext.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libUtils/GasConv.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& accountStore,
                         TransactionReceipt& receipt)
    : mAccountStore(accountStore), mTxReceipt(receipt) {}

CpsExecuteResult CpsExecutor::PreValidateRun(
    const EvmProcessContext& context) const {
  const auto owned = mAccountStore.GetBalanceForAccountAtomic(
      context.GetTransaction().GetSenderAddr());

  const auto amountResult = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amountResult.isSuccess) {
    return amountResult;
  }
  const auto gas_result = CpsExecuteValidator::CheckGasLimit(context);
  if (!gas_result.isSuccess) {
    return gas_result;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecutor::~CpsExecutor() = default;

void CpsExecutor::InitRun() { mAccountStore.DiscardAtomics(); }

CpsExecuteResult CpsExecutor::Run(EvmProcessContext& clientContext) {
  InitRun();

  const auto preValidateResult = PreValidateRun(clientContext);
  if (!preValidateResult.isSuccess) {
    return preValidateResult;
  }

  CpsContext cpsCtx{clientContext.GetEvmArgs().estimate(),
                    clientContext.GetEvmArgs().extras()};
  auto evmRun = std::make_shared<CpsRunEvm>(
      clientContext.GetEvmArgs(), *this, cpsCtx,
      IsNullAddress(clientContext.GetTransaction().GetToAddr()));
  m_queue.push_back(std::move(evmRun));

  CpsExecuteResult runResult;
  while (!m_queue.empty()) {
    const auto currentRun = std::move(m_queue.back());
    m_queue.pop_back();
    runResult = currentRun->Run(mTxReceipt);
    if (!runResult.isSuccess) {
      LOG_GENERAL(WARNING, "Got error from processing");
      break;
    }

    // Likely rewrite that to std::variant and check if it's scilla type
    if (!m_queue.empty()) {
      CpsRunEvm* nextRun = static_cast<CpsRunEvm*>(m_queue.back().get());
      if (nextRun->IsResumable()) {
        nextRun->ProvideFeedback(*static_cast<CpsRunEvm*>(currentRun.get()),
                                 runResult.evmResult);
      }
    }
  }

  clientContext.SetEvmResult(runResult.evmResult);
  const auto gasRemainedCore =
      GasConv::GasUnitsFromEthToCore(runResult.evmResult.remaining_gas());

  // failure
  if (!m_queue.empty() || !runResult.isSuccess) {
    mAccountStore.DiscardAtomics();
    mTxReceipt.clear();
    mTxReceipt.SetCumGas(clientContext.GetTransaction().GetGasLimitZil() -
                         gasRemainedCore);
    mTxReceipt.SetResult(false);
    mTxReceipt.AddError(RUNNER_FAILED);
    mTxReceipt.update();
  } else {
    mAccountStore.CommitAtomics();
    mTxReceipt.SetCumGas(clientContext.GetTransaction().GetGasLimitZil() -
                         gasRemainedCore);
    mTxReceipt.SetResult(true);
    mTxReceipt.update();
    RefundGas(clientContext, runResult);
  }

  return runResult;
}

void CpsExecutor::RefundGas(const EvmProcessContext& context,
                            const CpsExecuteResult& runResult) {
  const auto gasRemainedCore =
      GasConv::GasUnitsFromEthToCore(runResult.evmResult.remaining_gas());
  uint128_t gasRefund;
  if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                context.GetTransaction().GetGasPriceWei(),
                                gasRefund)) {
    return;
  }

  mAccountStore.IncreaseBalance(context.GetTransaction().GetSenderAddr(),
                                Amount::fromQa(gasRefund));
}

void CpsExecutor::PushRun(std::shared_ptr<CpsRun> run) {
  m_queue.push_back(std::move(run));
}

}  // namespace libCps
