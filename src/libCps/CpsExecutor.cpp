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
#include "libCps/CpsMetrics.h"
#include "libCps/CpsRunEvm.h"
#include "libCps/CpsRunScilla.h"
#include "libCps/CpsUtils.h"

#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libUtils/GasConv.h"
#include "libUtils/SafeMath.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& accountStore,
                         TransactionReceipt& receipt)
    : mAccountStore(accountStore), mTxReceipt(receipt) {}

CpsExecuteResult CpsExecutor::PreValidateEvmRun(
    const EvmProcessContext& context) const {
  CREATE_SPAN(zil::trace::FilterClass::TXN,
              ProtoToAddress(context.GetEvmArgs().origin()).hex(),
              ProtoToAddress(context.GetEvmArgs().address()).hex(),
              ProtoToAddress(context.GetEvmArgs().origin()).hex(),
              ProtoToUint(context.GetEvmArgs().apparent_value())
                  .convert_to<std::string>())

  const auto owned = mAccountStore.GetBalanceForAccountAtomic(
      ProtoToAddress(context.GetEvmArgs().origin()));

  const auto amountResult = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amountResult.isSuccess) {
    span.SetError("Insufficient balance to initiate cps from evm");
    return amountResult;
  }
  const auto gasResult = CpsExecuteValidator::CheckGasLimit(context);
  if (!gasResult.isSuccess) {
    span.SetError("Insufficient gas to initiate cps from evm");
    return gasResult;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsExecutor::PreValidateScillaRun(
    const ScillaProcessContext& context) const {
  CREATE_SPAN(zil::trace::FilterClass::TXN, context.origin.hex(),
              context.recipient.hex(), context.origin.hex(),
              context.amount.convert_to<std::string>())

  if (!mAccountStore.AccountExistsAtomic(context.origin)) {
    LOG_GENERAL(WARNING,
                "It looks the sender doesn't exist in atomic account store");
    return {TxnStatus::INVALID_FROM_ACCOUNT, false, {}};
  }
  const auto owned = mAccountStore.GetBalanceForAccountAtomic(context.origin);
  const auto amountResult = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amountResult.isSuccess) {
    span.SetError("Insufficient balance to initiate cps from scilla");
    return amountResult;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecutor::~CpsExecutor() = default;

void CpsExecutor::InitRun() { mAccountStore.DiscardAtomics(); }

CpsExecuteResult CpsExecutor::RunFromScilla(
    ScillaProcessContext& clientContext) {
  CREATE_SPAN(zil::trace::FilterClass::TXN, clientContext.origin.hex(),
              clientContext.recipient.hex(), clientContext.origin.hex(),
              clientContext.amount.convert_to<std::string>())

  InitRun();
  const auto preValidateResult = PreValidateScillaRun(clientContext);
  if (!preValidateResult.isSuccess) {
    mTxReceipt.RemoveAllTransitions();
    mTxReceipt.SetCumGas(0);
    mTxReceipt.SetResult(false);
    mTxReceipt.update();
    mAccountStore.IncreaseNonceForAccount(clientContext.origin);
    return preValidateResult;
  }

  CpsContext cpsCtx{
      .origSender = clientContext.origin,
      .isStatic = false,
      .estimate = false,
      .gasTracker = GasTracker::CreateFromCore(clientContext.gasLimit),
      .evmExtras = CpsUtils::FromScillaContext(clientContext),
      .scillaExtras = clientContext};

  TakeGasFromAccount(clientContext);

  // Special case for transfer only
  if (clientContext.contractType == Transaction::NON_CONTRACT) {
    if (!mAccountStore.TransferBalanceAtomic(
            clientContext.origin, clientContext.recipient,
            Amount::fromQa(clientContext.amount))) {
      LOG_GENERAL(WARNING,
                  "Insufficient funds to transfer from sender to recipient in "
                  "non-contract call");
      mAccountStore.IncreaseNonceForAccount(cpsCtx.origSender);
      return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
    }
    mTxReceipt.SetCumGas(NORMAL_TRAN_GAS);
    mTxReceipt.SetResult(true);
    mTxReceipt.update();
    cpsCtx.gasTracker.DecreaseByCore(NORMAL_TRAN_GAS);
    RefundGas(clientContext, cpsCtx.gasTracker);
    mAccountStore.CommitAtomics();
    mAccountStore.IncreaseNonceForAccount(cpsCtx.origSender);
    return {TxnStatus::NOT_PRESENT, true, {}};
  }

  const auto type = clientContext.contractType == Transaction::CONTRACT_CALL
                        ? CpsRun::Call
                        : CpsRun::Create;

  auto args = ScillaArgs{.from = cpsCtx.scillaExtras.origin,
                         .dest = cpsCtx.scillaExtras.recipient,
                         .origin = cpsCtx.scillaExtras.origin,
                         .value = Amount::fromQa(cpsCtx.scillaExtras.amount),
                         .calldata =
                             ScillaArgs::CodeData{
                                 cpsCtx.scillaExtras.code,
                                 cpsCtx.scillaExtras.data,
                             },
                         .edge = 0,
                         .depth = 0};

  auto scillaRun =
      std::make_shared<CpsRunScilla>(std::move(args), *this, cpsCtx, type);

  m_queue.push_back(std::move(scillaRun));

  const auto execResult = processLoop(cpsCtx);

  TRACE_EVENT("ScillaCpsRun", "processLoop", "completed");

  const auto gasRemainedCore = GetRemainedGasCore(execResult);

  const bool isFailure = !m_queue.empty() || !execResult.isSuccess;

  LOG_GENERAL(DEBUG, "Scilla CPS run is completed with status: "
                         << (isFailure ? "failure" : "success"));
  span.SetAttribute("Failure", isFailure);
  if (isFailure) {
    LOG_GENERAL(INFO, "TxnStatus for failed run: " << execResult.txnStatus);
    mAccountStore.RevertContractStorageState();
    mAccountStore.DiscardAtomics();
    mTxReceipt.RemoveAllTransitions();
    mTxReceipt.SetCumGas(clientContext.gasLimit - gasRemainedCore);
    mTxReceipt.SetResult(false);
    mTxReceipt.update();
  } else {
    mTxReceipt.SetCumGas(clientContext.gasLimit - gasRemainedCore);
    mTxReceipt.SetResult(true);
    mTxReceipt.update();
    RefundGas(clientContext, GasTracker::CreateFromCore(gasRemainedCore));
    mAccountStore.CommitAtomics();
  }

  // Increase nonce regardless of processing result
  mAccountStore.IncreaseNonceForAccount(cpsCtx.origSender);
  // Deduct from account balance gas used for failed transaction
  if (isFailure) {
    const auto usedGasCore = clientContext.gasLimit - gasRemainedCore;
    uint128_t gasCost;
    // Convert here because we deducted in eth units.
    if (!SafeMath<uint128_t>::mul(usedGasCore, clientContext.gasPrice,
                                  gasCost)) {
      return {TxnStatus::ERROR, false, {}};
    }
    const auto amount = Amount::fromQa(gasCost);
    mAccountStore.DecreaseBalance(cpsCtx.origSender, amount);
  }
  return execResult;
}

CpsExecuteResult CpsExecutor::RunFromEvm(EvmProcessContext& clientContext) {
  CREATE_SPAN(zil::trace::FilterClass::TXN,
              ProtoToAddress(clientContext.GetEvmArgs().origin()).hex(),
              ProtoToAddress(clientContext.GetEvmArgs().address()).hex(),
              ProtoToAddress(clientContext.GetEvmArgs().origin()).hex(),
              ProtoToUint(clientContext.GetEvmArgs().apparent_value())
                  .convert_to<std::string>())
  InitRun();

  const auto preValidateResult = PreValidateEvmRun(clientContext);
  if (!preValidateResult.isSuccess) {
    mTxReceipt.SetResult(false);
    mTxReceipt.SetCumGas(0);
    mTxReceipt.update();
    mAccountStore.IncreaseNonceForAccount(
        ProtoToAddress(clientContext.GetEvmArgs().origin()));
    LOG_GENERAL(WARNING,
                "RunFromEvm: Precondition for running transaction failed");
    return preValidateResult;
  }

  LOG_GENERAL(
      DEBUG, "CpsExecutor::RunFromEvm(): From "
                 << ProtoToAddress(clientContext.GetEvmArgs().origin()).hex()
                 << " , to: "
                 << ProtoToAddress(clientContext.GetEvmArgs().address()).hex());

  TakeGasFromAccount(clientContext);

  CpsContext cpsCtx{
      ProtoToAddress(clientContext.GetEvmArgs().origin()),
      clientContext.GetDirect(),
      clientContext.GetEvmArgs().estimate(),
      GasTracker::CreateFromEth(clientContext.GetEvmArgs().gas_limit()),
      clientContext.GetEvmArgs().extras(),
      CpsUtils::FromEvmContext(clientContext)};
  const auto destAddress = ProtoToAddress(clientContext.GetEvmArgs().address());
  const auto runType =
      IsNullAddress(destAddress) ? CpsRun::Create : CpsRun::Call;
  auto evmRun = std::make_shared<CpsRunEvm>(clientContext.GetEvmArgs(), *this,
                                            cpsCtx, runType);
  this->TxTraceClear();
  m_queue.push_back(std::move(evmRun));

  auto runResult = processLoop(cpsCtx);
  TRACE_EVENT("EvmCpsRun", "processLoop", "completed");

  // right. There is a long and tedious discussion about this in slack
  // https://zilliqa-team.slack.com/archives/C042YP854RZ/p1682094771583839
  // You need to do this calculation in core units, so that we can correctly
  // represent the cumulative gas used in the receipt (which is serialised,
  // so can't easily be changed).
  const auto givenGasCore =
      GasConv::GasUnitsFromEthToCore(clientContext.GetEvmArgs().gas_limit());

  uint64_t gasRemainingCore = cpsCtx.gasTracker.GetCoreGas();

  if (std::holds_alternative<evm::EvmResult>(runResult.result)) {
    const auto& evmResult = std::get<evm::EvmResult>(runResult.result);
    clientContext.SetEvmResult(evmResult);
  }

  const bool isFailure = !m_queue.empty() || !runResult.isSuccess;
  const bool isEstimate = !clientContext.GetCommit();
  const bool isEthCall = cpsCtx.isStatic;

  LOG_GENERAL(DEBUG, "Evm CPS run is completed with status: "
                         << (isFailure ? "failure" : "success"));

  span.SetAttribute("Estimate", isEstimate);
  span.SetAttribute("EthCall", isEthCall);
  span.SetAttribute("Failure", isFailure);
  LOG_GENERAL(DEBUG, "Estimate: " << isEstimate << ", EthCall: " << isEthCall
                                  << ", Failure: " << isFailure);

  const auto usedGasCore = givenGasCore - gasRemainingCore;

  // failure or Estimate/EthCall mode
  if (isFailure || isEstimate || isEthCall) {
    mAccountStore.RevertContractStorageState();
    mAccountStore.DiscardAtomics();
    // This will get converted back up again before we report it.
    mTxReceipt.SetCumGas(usedGasCore);
    if (isFailure) {
      LOG_GENERAL(INFO, "TxnStatus for failed run: " << runResult.txnStatus);
      if (std::holds_alternative<evm::EvmResult>(runResult.result)) {
        auto const& result = std::get<evm::EvmResult>(runResult.result);
        LOG_GENERAL(INFO, EvmUtils::ExitReasonString(result.exit_reason()));
      } else {
        LOG_GENERAL(WARNING, "EVM call returned a Scilla result");
      }
      mTxReceipt.SetResult(false);
    } else {
      mTxReceipt.SetResult(true);
      mTxReceipt.clear();
    }
    mTxReceipt.update();
  } else {
    mTxReceipt.SetCumGas(usedGasCore);
    mTxReceipt.SetResult(true);
    mTxReceipt.update();
    RefundGas(clientContext, GasTracker::CreateFromCore(gasRemainingCore));
    mAccountStore.CommitAtomics();
  }
  if (!isEstimate && !isEthCall) {
    // Increase nonce regardless of processing result for transaction calls
    mAccountStore.IncreaseNonceForAccount(cpsCtx.origSender);
    // Take gas used by account even if it was a failed run
    if (isFailure) {
      uint256_t gasCost;
      // Convert here because we deducted in eth units.
      if (!SafeMath<uint256_t>::mul(
              GasConv::GasUnitsFromCoreToEth(usedGasCore),
              CpsExecuteValidator::GetGasPriceWei(clientContext), gasCost)) {
        return {TxnStatus::ERROR, false, {}};
      }
      const auto amount = Amount::fromWei(gasCost);
      mAccountStore.DecreaseBalance(cpsCtx.origSender, amount);
    }
  }
  // Always mark run as successful in estimate mode
  if (isEstimate) {
    if (std::holds_alternative<evm::EvmResult>(runResult.result)) {
      auto& evmResult = std::get<evm::EvmResult>(runResult.result);
      evmResult.set_remaining_gas(cpsCtx.gasTracker.GetEthGas());
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
    auto& scillaResults = std::get<ScillaResult>(runResult.result);
    evm::EvmResult evmResult;
    evm::ExitReason exitReason;
    if (scillaResults.isSuccess) {
      exitReason.set_succeed(evm::ExitReason_Succeed_STOPPED);
    } else {
      exitReason.set_revert(evm::ExitReason_Revert_REVERTED);
    }
    *evmResult.mutable_exit_reason() = exitReason;
    evmResult.set_remaining_gas(cpsCtx.gasTracker.GetCoreGas());
    clientContext.SetEvmResult(evmResult);
    return {TxnStatus::NOT_PRESENT, true, std::move(evmResult)};
  }

  return runResult;
}

CpsExecuteResult CpsExecutor::processLoop(const CpsContext& context) {
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
    // The gas price here is already scaled by GasConv::GetScalingFactor()
    // So we need to make sure our gas limit isn't also scaled by it.
    // We also need to round the gas limit so that we take a whole number
    // of core units.
    // - rrw 2023-04-22
    uint256_t gasLimitRounded =
        GasConv::GasUnitsFromCoreToEth(GasConv::GasUnitsFromEthToCore(
            evmCtx.GetTransaction().GetGasLimitEth()));
    if (!SafeMath<uint256_t>::mul(gasLimitRounded,
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

  LOG_GENERAL(DEBUG, "Take " << amount.toWei().str() << " Wei ("
                             << amount.toQa().str() << " Qa) from " << address
                             << " for gas deposit");
  // This is in Wei!
  mAccountStore.DecreaseBalanceAtomic(address, amount);
}

void CpsExecutor::RefundGas(
    const std::variant<EvmProcessContext, ScillaProcessContext>& context,
    const GasTracker& gasTracker) {
  Amount amount;
  Address account;

  // EVM initiates
  if (std::holds_alternative<EvmProcessContext>(context)) {
    const auto& evmCtx = std::get<EvmProcessContext>(context);
    account = ProtoToAddress(evmCtx.GetEvmArgs().origin());
    uint128_t gasRefund;
    // The gas price is already scaled by GasConv::EthToCore, so we need to make
    // sure the gas remaining isn't.
    uint128_t gasRemainedEth = gasTracker.GetEthGas();
    if (!SafeMath<uint128_t>::mul(gasRemainedEth,
                                  CpsExecuteValidator::GetGasPriceWei(evmCtx),
                                  gasRefund)) {
      return;
    }
    amount = Amount::fromWei(gasRefund);
    // Scilla initiates
  } else {
    const auto& scillaCtx = std::get<ScillaProcessContext>(context);
    account = scillaCtx.origin;
    uint128_t gasRefund;
    if (!SafeMath<uint128_t>::mul(gasTracker.GetCoreGas(), scillaCtx.gasPrice,
                                  gasRefund)) {
      return;
    }
    amount = Amount::fromQa(gasRefund);
  }

  mAccountStore.IncreaseBalanceAtomic(account, amount);
}

uint64_t CpsExecutor::GetRemainedGasCore(
    const CpsExecuteResult& execResult) const {
  // EvmRun was the last one
  if (std::holds_alternative<evm::EvmResult>(execResult.result)) {
    const auto& evmResult = std::get<evm::EvmResult>(execResult.result);
    return GasConv::GasUnitsFromEthToCore(evmResult.remaining_gas());
  }
  // ScillaRun was the last one
  else {
    const auto& scillaResult = std::get<ScillaResult>(execResult.result);
    return scillaResult.gasRemained;
  }
}

void CpsExecutor::PushRun(std::shared_ptr<CpsRun> run) {
  m_queue.push_back(std::move(run));
}

std::string& CpsExecutor::CurrentTrace() { return this->m_txTrace; }

void CpsExecutor::TxTraceClear() { this->m_txTrace.clear(); }

}  // namespace libCps
