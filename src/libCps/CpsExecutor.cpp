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
#include "libCps/CpsUtils.h"

#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libUtils/GasConv.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& accountStore,
                         TransactionReceipt& receipt)
    : mAccountStore(accountStore), mTxReceipt(receipt) {}

CpsExecuteResult CpsExecutor::PreValidateEvmRun(
    const EvmProcessContext& context) const {
  const auto owned = mAccountStore.GetBalanceForAccountAtomic(
      ProtoToAddress(context.GetEvmArgs().origin()));

  const auto amountResult = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amountResult.isSuccess) {
    return amountResult;
  }
  const auto gasResult = CpsExecuteValidator::CheckGasLimit(context);
  if (!gasResult.isSuccess) {
    return gasResult;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsExecutor::PreValidateScillaRun(
    const ScillaProcessContext& context) const {
  const auto owned = mAccountStore.GetBalanceForAccountAtomic(context.origin);
  const auto amountResult = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amountResult.isSuccess) {
    return amountResult;
  }
  const auto gasResult = CpsExecuteValidator::CheckGasLimit(context);
  if (!gasResult.isSuccess) {
    return gasResult;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecutor::~CpsExecutor() = default;

void CpsExecutor::InitRun() { mAccountStore.DiscardAtomics(); }

CpsExecuteResult CpsExecutor::RunFromScilla(
    ScillaProcessContext& clientContext) {
  InitRun();
  const auto preValidateResult = PreValidateScillaRun(clientContext);
  if (!preValidateResult.isSuccess) {
    return preValidateResult;
  }

  CpsContext cpsCtx{.origSender = clientContext.origin,
                    .isStatic = false,
                    .estimate = false,
                    .evmExtras = CpsUtils::FromScillaContext(clientContext),
                    .scillaExtras = clientContext};
  (void)cpsCtx;
  return {};
}

CpsExecuteResult CpsExecutor::RunFromEvm(EvmProcessContext& clientContext) {
  InitRun();

  const auto preValidateResult = PreValidateEvmRun(clientContext);
  if (!preValidateResult.isSuccess) {
    return preValidateResult;
  }

  TakeGasFromAccount(clientContext);

  CpsContext cpsCtx{ProtoToAddress(clientContext.GetEvmArgs().origin()),
                    clientContext.GetDirect(),
                    clientContext.GetEvmArgs().estimate(),
                    clientContext.GetEvmArgs().extras(),
                    CpsUtils::FromEvmContext(clientContext)};
  const auto runType =
      IsNullAddress(ProtoToAddress(clientContext.GetEvmArgs().address()))
          ? CpsRun::Create
          : CpsRun::Call;
  auto evmRun = std::make_shared<CpsRunEvm>(clientContext.GetEvmArgs(), *this,
                                            cpsCtx, runType);
  m_queue.push_back(std::move(evmRun));

  mAccountStore.BufferCurrentContractStorageState();

  CpsExecuteResult runResult;
  while (!m_queue.empty()) {
    const auto currentRun = std::move(m_queue.back());
    m_queue.pop_back();
    runResult = currentRun->Run(mTxReceipt);
    if (!runResult.isSuccess) {
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

  // Increase nonce regardless of processing result
  const auto sender = clientContext.GetTransaction().GetSenderAddr();
  mAccountStore.IncreaseNonceForAccountAtomic(sender);

  const auto givenGasCore =
      GasConv::GasUnitsFromEthToCore(clientContext.GetEvmArgs().gas_limit());

  uint64_t gasRemainedCore = 0;
  // EvmRun was the last one
  if (std::holds_alternative<evm::EvmResult>(runResult.result)) {
    const auto& evmResult = std::get<evm::EvmResult>(runResult.result);
    clientContext.SetEvmResult(evmResult);
    gasRemainedCore = GasConv::GasUnitsFromEthToCore(evmResult.remaining_gas());
  }
  // ScillaRun was the last one
  else {
    const auto& scillaResult = std::get<ScillaResult>(runResult.result);
    gasRemainedCore = scillaResult.gasRemained;
  }

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
    RefundGas(clientContext, gasRemainedCore);
    mAccountStore.CommitAtomics();
  }
  // Always mark run as successful in estimate mode
  if (isEstimate) {
    if (std::holds_alternative<evm::EvmResult>(runResult.result)) {
      auto& evmResult = std::get<evm::EvmResult>(runResult.result);
      // In some cases revert state may be missing (if e.g. trap validation
      // failed)
      if (isFailure && evmResult.exit_reason().exit_reason_case() ==
                         evm::ExitReason::EXIT_REASON_NOT_SET) {
        evm::ExitReason exitReason;
        exitReason.set_revert(evm::ExitReason_Revert_REVERTED);
        *evmResult.mutable_exit_reason() = exitReason;
        clientContext.SetEvmResult(evmResult);
      }
      return {TxnStatus::NOT_PRESENT, true, evmResult};
    }
    evm::EvmResult evmResult;
    evmResult.set_remaining_gas(
        GasConv::GasUnitsFromCoreToEth(gasRemainedCore));
    return {TxnStatus::NOT_PRESENT, true, std::move(evmResult)};
  }
  return runResult;
}

void CpsExecutor::TakeGasFromAccount(
    const std::variant<EvmProcessContext, ScillaProcessContext>& context) {
  CpsContext::Address address;
  Amount amount;
  // EVM initiates
  if (std::holds_alternative<EvmProcessContext>(context)) {
    const auto& evmCtx = std::get<EvmProcessContext>(context);
    uint256_t gasDepositWei;
    if (!SafeMath<uint256_t>::mul(evmCtx.GetTransaction().GetGasLimitZil(),
                                  CpsExecuteValidator::GetGasPriceWei(evmCtx),
                                  gasDepositWei)) {
      return;
    }
    address = ProtoToAddress(evmCtx.GetEvmArgs().origin());
    amount = Amount::fromWei(gasDepositWei);
  }
  // Scilla initiates
  else {
    const auto& scillaCtx = std::get<ScillaProcessContext>(context);
    uint128_t gasDepositQa;
    if (!SafeMath<uint128_t>::mul(scillaCtx.gasLimit, scillaCtx.gasPrice,
                                  gasDepositQa)) {
      return;
    }
    address = scillaCtx.origin;
    amount = Amount::fromQa(gasDepositQa);
  }

  mAccountStore.DecreaseBalanceAtomic(address, amount);
}

void CpsExecutor::RefundGas(
    const std::variant<EvmProcessContext, ScillaProcessContext>& context,
    uint64_t gasRemainedCore) {
  Amount amount;
  Address account;

  // EVM initiates
  if (std::holds_alternative<EvmProcessContext>(context)) {
    const auto& evmCtx = std::get<EvmProcessContext>(context);
    account = ProtoToAddress(evmCtx.GetEvmArgs().origin());
    uint128_t gasRefund;
    if (!SafeMath<uint128_t>::mul(gasRemainedCore,
                                  CpsExecuteValidator::GetGasPriceWei(evmCtx),
                                  gasRefund)) {
      return;
    }
    amount = Amount::fromWei(gasRefund);
  } else {
    const auto& scillaCtx = std::get<ScillaProcessContext>(context);
    account = scillaCtx.origin;
    uint128_t gasRefund;
    if (!SafeMath<uint128_t>::mul(gasRemainedCore, scillaCtx.gasPrice,
                                  gasRefund)) {
      return;
    }
    amount = Amount::fromQa(gasRefund);
  }

  mAccountStore.IncreaseBalanceAtomic(account, amount);
}

void CpsExecutor::PushRun(std::shared_ptr<CpsRun> run) {
  m_queue.push_back(std::move(run));
}

}  // namespace libCps
