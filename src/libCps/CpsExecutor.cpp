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
#include "libUtils/Logger.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& accountStore,
                         TransactionReceipt& receipt)
    : mAccountSore(accountStore), mTxReceipt(receipt) {}

CpsExecuteResult CpsExecutor::PreValidateRun(
    const EvmProcessContext& context) const {
  const auto owned = mAccountSore.GetBalanceForAccountAtomic(
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

void CpsExecutor::InitRun() { mAccountSore.DiscardAtomics(); }

CpsExecuteResult CpsExecutor::Run(const EvmProcessContext& context) {
  InitRun();

  const auto preValidateResult = PreValidateRun(context);
  if (!preValidateResult.isSuccess) {
    return preValidateResult;
  }

  CpsContext ctx{context.GetEvmArgs().estimate(),
                 context.GetEvmArgs().extras()};
  auto evmRun = std::make_unique<CpsRunEvm>(context.GetEvmArgs(), *this, ctx);
  m_queue.push_back(std::move(evmRun));

  CpsExecuteResult runResult;
  while (!m_queue.empty()) {
    const auto currentRun = std::move(m_queue.back());
    m_queue.pop_back();
    runResult = currentRun->Run(mAccountSore, mTxReceipt);
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

  return runResult;
}

void CpsExecutor::PushRun(std::unique_ptr<CpsRun> run) {
  m_queue.push_back(std::move(run));
}

}  // namespace libCps
