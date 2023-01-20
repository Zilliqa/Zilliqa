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

#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
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
      ProtoToAddress(context.GetEvmArgs().origin()));

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

  TakeGasFromAccount(clientContext);

  LOG_GENERAL(WARNING, "CPS ENTER");
  LOG_GENERAL(WARNING, "CPS ENTER FROM: " << ProtoToAddress(
                           clientContext.GetEvmArgs().origin()));
  LOG_GENERAL(WARNING, "CPS ENTER TO: " << ProtoToAddress(
                           clientContext.GetEvmArgs().address()));
  LOG_GENERAL(WARNING, "CPS ENTER VALUE: " << ProtoToUint(
                           clientContext.GetEvmArgs().apparent_value()));
  LOG_GENERAL(WARNING, "CPS ENTER CODE SIZE: "
                           << clientContext.GetEvmArgs().code().size());
  LOG_GENERAL(WARNING, "CPS ENTER DATA SIZE: "
                           << clientContext.GetEvmArgs().data().size());
  LOG_GENERAL(WARNING,
              "CPS ENTER GAS: " << clientContext.GetEvmArgs().gas_limit());
  LOG_GENERAL(WARNING, "CPS ENTER Funds: "
                           << mAccountStore
                                  .GetBalanceForAccountAtomic(ProtoToAddress(
                                      clientContext.GetEvmArgs().origin()))
                                  .toQa()
                                  .convert_to<std::string>());

  CpsContext cpsCtx{ProtoToAddress(clientContext.GetEvmArgs().origin()),
                    clientContext.GetDirect(),
                    clientContext.GetEvmArgs().estimate(),
                    clientContext.GetEvmArgs().extras()};
  const auto runType =
      IsNullAddress(ProtoToAddress(clientContext.GetEvmArgs().address()))
          ? CpsRun::Create
          : CpsRun::Call;
  auto evmRun = std::make_shared<CpsRunEvm>(clientContext.GetEvmArgs(), *this,
                                            cpsCtx, runType);
  m_queue.push_back(std::move(evmRun));

  LOG_GENERAL(WARNING,
              "EXEC NONCE IS: " << mAccountStore.GetNonceForAccountAtomic(
                  clientContext.GetTransaction().GetSenderAddr()));

  mAccountStore.BufferCurrentContractStorageState();

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
      CpsRun* nextRun = m_queue.back().get();
      if (nextRun->IsResumable()) {
        nextRun->ProvideFeedback(*currentRun.get(), runResult);
      }
    }
  }

  LOG_GENERAL(WARNING,
              "CPS LEAVE GAS LEFT: " << runResult.evmResult.remaining_gas());

  LOG_GENERAL(WARNING, "CPS Leave Funds: "
                           << mAccountStore
                                  .GetBalanceForAccountAtomic(ProtoToAddress(
                                      clientContext.GetEvmArgs().origin()))
                                  .toQa()
                                  .convert_to<std::string>());

  // Increase nonce regardless of processing result
  const auto sender = clientContext.GetTransaction().GetSenderAddr();
  mAccountStore.IncreaseNonceForAccountAtomic(sender);

  LOG_GENERAL(WARNING,
              "EXEC NONCE 2 IS: " << mAccountStore.GetNonceForAccountAtomic(
                  clientContext.GetTransaction().GetSenderAddr()));

  clientContext.SetEvmResult(runResult.evmResult);
  const auto givenGasCore =
      GasConv::GasUnitsFromEthToCore(clientContext.GetEvmArgs().gas_limit());
  const auto gasRemainedCore =
      GasConv::GasUnitsFromEthToCore(runResult.evmResult.remaining_gas());

  const bool isFailure = !m_queue.empty() || !runResult.isSuccess;
  const bool isEstimate = !clientContext.GetCommit();
  // failure or Estimate mode
  if (isFailure || isEstimate) {
    mAccountStore.RevertContractStorageState();
    mAccountStore.DiscardAtomics();
    mTxReceipt.clear();
    mTxReceipt.SetCumGas(givenGasCore - gasRemainedCore);
    if (isFailure) {
      mTxReceipt.SetResult(false);
      mTxReceipt.AddError(RUNNER_FAILED);
    } else {
      mTxReceipt.SetResult(true);
    }
    mTxReceipt.update();
  } else {
    mTxReceipt.SetCumGas(givenGasCore - gasRemainedCore);
    mTxReceipt.SetResult(true);
    mTxReceipt.update();
    RefundGas(clientContext, runResult);
    mAccountStore.CommitAtomics();

    LOG_GENERAL(WARNING, "CPS Leave Funds After Refund: "
                             << mAccountStore
                                    .GetBalanceForAccountAtomic(ProtoToAddress(
                                        clientContext.GetEvmArgs().origin()))
                                    .toQa()
                                    .convert_to<std::string>());
  }
  // Always mark run as successful in estimate mode
  if (isEstimate) {
    return {TxnStatus::NOT_PRESENT, true, runResult.evmResult};
  }
  return runResult;
}

void CpsExecutor::TakeGasFromAccount(const EvmProcessContext& context) {
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(context.GetTransaction().GetGasLimitZil(),
                                CpsExecuteValidator::GetGasPriceWei(context),
                                gasDepositWei)) {
    return;
  }

  LOG_GENERAL(
      WARNING,
      "Gas to be taken Qa: "
          << Amount::fromWei(gasDepositWei).toQa().convert_to<std::string>());
  mAccountStore.DecreaseBalanceAtomic(context.GetTransaction().GetSenderAddr(),
                                      Amount::fromWei(gasDepositWei));
}

void CpsExecutor::RefundGas(const EvmProcessContext& context,
                            const CpsExecuteResult& runResult) {
  const auto gasRemainedCore =
      GasConv::GasUnitsFromEthToCore(runResult.evmResult.remaining_gas());
  uint128_t gasRefund;
  if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                CpsExecuteValidator::GetGasPriceWei(context),
                                gasRefund)) {
    return;
  }
  LOG_GENERAL(
      WARNING,
      "Gas to be refunded Qa: "
          << Amount::fromWei(gasRefund).toQa().convert_to<std::string>());

  mAccountStore.IncreaseBalanceAtomic(context.GetTransaction().GetSenderAddr(),
                                      Amount::fromWei(gasRefund));
}

void CpsExecutor::PushRun(std::shared_ptr<CpsRun> run) {
  m_queue.push_back(std::move(run));
}

}  // namespace libCps
