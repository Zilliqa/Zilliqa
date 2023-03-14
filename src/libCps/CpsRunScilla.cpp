/*
 * Copyright (C) 2023 Zilliqa
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

#include "libCps/CpsRunScilla.h"
#include "CpsMetrics.h"
#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsContext.h"
#include "libCps/CpsExecutor.h"
#include "libCps/CpsRunTransfer.h"
#include "libCps/ScillaHelpers.h"
#include "libCps/ScillaHelpersCall.h"
#include "libCps/ScillaHelpersCreate.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/scilla/ScillaClient.h"
#include "libScilla/ScillaUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/TimeUtils.h"

#include "libUtils/JsonUtils.h"

#include <future>
#include <ranges>

namespace libCps {

CpsRunScilla::CpsRunScilla(ScillaArgs args, CpsExecutor& executor,
                           const CpsContext& ctx, CpsRun::Type type)
    : CpsRun(executor.GetAccStoreIface(), CpsRun::Domain::Scilla, type),
      mArgs(std::move(args)),
      mExecutor(executor),
      mCpsContext(ctx) {}

CpsExecuteResult CpsRunScilla::Run(TransactionReceipt& receipt) {
  if (GetType() != CpsRun::Call && GetType() != CpsRun::Create &&
      GetType() != CpsRun::TrapScillaCall) {
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }
  const auto gasCheckRes = checkGas();
  if (!gasCheckRes.isSuccess) {
    return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
  }

  if (GetType() == CpsRun::Create) {
    return runCreate(receipt);
  } else {
    return runCall(receipt);
  }
}

CpsExecuteResult CpsRunScilla::checkGas() {
  if (!std::holds_alternative<ScillaArgs::CodeData>(mArgs.calldata)) {
    return {TxnStatus::NOT_PRESENT, true, {}};
  }
  const auto calldata = std::get<ScillaArgs::CodeData>(mArgs.calldata);
  const auto& code = calldata.code;
  const auto& data = calldata.data;
  if (GetType() == CpsRun::Create) {
    const auto createPenalty =
        std::max(CONTRACT_CREATE_GAS,
                 static_cast<unsigned int>(code.size() + data.size()));
    const auto scillaGas = SCILLA_CHECKER_INVOKE_GAS + SCILLA_RUNNER_INVOKE_GAS;
    const auto requiredGas = std::max(scillaGas, createPenalty);
    if (mArgs.gasLimit < requiredGas) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  } else if (GetType() == CpsRun::Call) {
    const auto callPenalty =
        std::max(CONTRACT_INVOKE_GAS, static_cast<unsigned int>(data.size()));

    const auto requiredGas = std::max(SCILLA_RUNNER_INVOKE_GAS, callPenalty);
    if (mArgs.gasLimit < requiredGas) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunScilla::runCreate(TransactionReceipt& receipt) {
  CREATE_SPAN(zil::trace::FilterClass::TXN, mArgs.from.hex(), mArgs.dest.hex(),
              mCpsContext.origSender.hex(),
              mArgs.value.toQa().convert_to<std::string>());

  if (!std::holds_alternative<ScillaArgs::CodeData>(mArgs.calldata)) {
    return {TxnStatus::ERROR, false, {}};
  }
  const auto& codedata = std::get<ScillaArgs::CodeData>(mArgs.calldata);
  const auto createPenalty = std::max(
      CONTRACT_CREATE_GAS,
      static_cast<unsigned int>(codedata.code.size() + codedata.data.size()));

  // Original gas passed from user (not the one in current ctx)
  auto retScillaVal =
      ScillaResult{mCpsContext.scillaExtras.gasLimit - createPenalty};
  mArgs.dest =
      mAccountStore.GetAddressForContract(mArgs.from, TRANSACTION_VERSION);
  if (!mAccountStore.AddAccountAtomic(mArgs.dest)) {
    span.SetError("AcountCreation");
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, retScillaVal};
  }

  if (!mAccountStore.TransferBalanceAtomic(mArgs.from, mArgs.dest,
                                           mArgs.value)) {
    span.SetError("Unable to make a balance transfer");
    return {TxnStatus::INSUFFICIENT_BALANCE, false, retScillaVal};
  }

  if (!mAccountStore.InitContract(mArgs.dest, codedata.code, codedata.data,
                                  mCpsContext.scillaExtras.blockNum)) {
    span.SetError("Unable to init a contract");
    return {TxnStatus::FAIL_CONTRACT_INIT, false, retScillaVal};
  }

  std::vector<Address> extlibs;
  bool isLibrary = false;
  uint32_t scillaVersion;
  std::map<Address, std::pair<std::string, std::string>> extlibsExports;

  if (!mAccountStore.GetContractAuxiliaries(mArgs.dest, isLibrary,
                                            scillaVersion, extlibs)) {
    span.SetError("Failed Scilla Auxiliaries");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (DISABLE_SCILLA_LIB && isLibrary) {
    span.SetError("Scilla libraries disabled");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::PopulateExtlibsExports(mAccountStore, scillaVersion,
                                             extlibs, extlibsExports)) {
    span.SetError("Failed to populate export libs");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::ExportCreateContractFiles(mAccountStore, mArgs.dest,
                                                isLibrary, scillaVersion,
                                                extlibsExports)) {
    span.SetError("Unable to export create contract files");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!mAccountStore.SetBCInfoProvider(mCpsContext.scillaExtras.blockNum,
                                       mCpsContext.scillaExtras.dsBlockNum,
                                       mCpsContext.scillaExtras.origin,
                                       mArgs.dest, scillaVersion)) {
    span.SetError("Unable to set BCInfor provider");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mArgs.gasLimit -= SCILLA_CHECKER_INVOKE_GAS;

  retScillaVal =
      ScillaResult{std::min(retScillaVal.gasRemained,
                            mCpsContext.scillaExtras.gasLimit - createPenalty)};

  const auto checkerResult = InvokeScillaInterpreter(INVOKE_TYPE::CHECKER);
  if (!checkerResult.isSuccess) {
    receipt.AddError(CHECKER_FAILED);
    span.SetError("Scilla contract checker failed");
    LOG_GENERAL(WARNING, "CHECKER out: " << checkerResult.returnVal);
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  std::map<std::string, zbytes> t_metadata;
  t_metadata.emplace(
      mAccountStore.GenerateContractStorageKey(mArgs.dest,
                                               SCILLA_VERSION_INDICATOR, {}),
      DataConversion::StringToCharArray(std::to_string(scillaVersion)));

  if (!ScillaHelpers::ParseContractCheckerOutput(
          mAccountStore, mArgs.dest, checkerResult.returnVal, receipt,
          t_metadata, mArgs.gasLimit, isLibrary)) {
    span.SetError("Unable to parse contract checker result");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mArgs.gasLimit -= SCILLA_RUNNER_INVOKE_GAS;
  const auto runnerResult = InvokeScillaInterpreter(INVOKE_TYPE::RUNNER_CREATE);
  if (!runnerResult.isSuccess) {
    span.SetError("Interpreter run is not successful");
    receipt.AddError(RUNNER_FAILED);
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  if (!ScillaHelpersCreate::ParseCreateContract(
          mArgs.gasLimit, runnerResult.returnVal, receipt, isLibrary)) {
    span.SetError("Unable to parse contract create result");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  t_metadata.emplace(mAccountStore.GenerateContractStorageKey(
                         mArgs.dest, CONTRACT_ADDR_INDICATOR, {}),
                     mArgs.dest.asBytes());

  if (!mAccountStore.UpdateStates(mArgs.dest, t_metadata, {}, true)) {
    span.SetError("Unable to update account state");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mAccountStore.MarkNewLibraryCreated(mArgs.dest);

  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.from);
  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.dest);

  return {TxnStatus::NOT_PRESENT, true, retScillaVal};
}

CpsExecuteResult CpsRunScilla::runCall(TransactionReceipt& receipt) {
  LOG_GENERAL(WARNING, "Executing call from: "
                           << mArgs.from.hex() << ", to: " << mArgs.dest.hex()
                           << ", value: "
                           << mArgs.value.toQa().convert_to<std::string>());
  LOG_GENERAL(WARNING,
              "FROM has balance: "
                  << mAccountStore.GetBalanceForAccountAtomic(mArgs.from)
                         .toQa()
                         .convert_to<std::string>());
  LOG_GENERAL(
      WARNING,
      "To has balance: " << mAccountStore.GetBalanceForAccountAtomic(mArgs.dest)
                                .toQa()
                                .convert_to<std::string>());

  CREATE_SPAN(zil::trace::FilterClass::TXN, mArgs.from.hex(), mArgs.dest.hex(),
              mCpsContext.origSender.hex(),
              mArgs.value.toQa().convert_to<std::string>());

  const auto callPenalty =
      std::max(CONTRACT_INVOKE_GAS,
               static_cast<unsigned int>(mCpsContext.scillaExtras.data.size()));
  auto retScillaVal = ScillaResult{std::min(
      mCpsContext.scillaExtras.gasLimit - callPenalty, mArgs.gasLimit)};

  if (!mAccountStore.AccountExistsAtomic(mArgs.dest)) {
    span.SetError("AcountCreation");
    return {TxnStatus::INVALID_TO_ACCOUNT, false, retScillaVal};
  }

  const auto currBalance = mAccountStore.GetBalanceForAccountAtomic(mArgs.from);
  if (mArgs.value > currBalance) {
    span.SetError("Insufficient balance");
    return {TxnStatus::INSUFFICIENT_BALANCE, false, retScillaVal};
  }

  mArgs.gasLimit -= SCILLA_RUNNER_INVOKE_GAS;
  retScillaVal = ScillaResult{std::min(
      mCpsContext.scillaExtras.gasLimit - callPenalty, mArgs.gasLimit)};

  std::vector<Address> extlibs;
  bool isLibrary = false;
  uint32_t scillaVersion;
  std::map<Address, std::pair<std::string, std::string>> extlibsExports;

  if (!mAccountStore.GetContractAuxiliaries(mArgs.dest, isLibrary,
                                            scillaVersion, extlibs)) {
    span.SetError("Failed Scilla Auxiliaries");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (DISABLE_SCILLA_LIB && isLibrary) {
    span.SetError("Scilla libraries disabled");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::PopulateExtlibsExports(mAccountStore, scillaVersion,
                                             extlibs, extlibsExports)) {
    span.SetError("Failed to populate export libs");
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (std::holds_alternative<ScillaArgs::CodeData>(mArgs.calldata)) {
    const auto& calldata = std::get<ScillaArgs::CodeData>(mArgs.calldata);
    if (!ScillaHelpers::ExportCallContractFiles(
            mAccountStore, mArgs.from, mArgs.dest, calldata.data, mArgs.value,
            scillaVersion, extlibsExports)) {
      span.SetError("Unable to export call contract files");
      return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
    }

  } else {
    const auto& jsonData = std::get<Json::Value>(mArgs.calldata);
    auto jsonStr = JSONUtils::GetInstance().convertJsontoStr(jsonData);
    LOG_GENERAL(WARNING, "SENDING SCILLA CALLCODE: " << jsonStr);
    if (!ScillaHelpers::ExportCallContractFiles(mAccountStore, mArgs.dest,
                                                jsonData, scillaVersion,
                                                extlibsExports)) {
      span.SetError("Unable to export call contract files");
      return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
    }
  }

  if (!mAccountStore.SetBCInfoProvider(mCpsContext.scillaExtras.blockNum,
                                       mCpsContext.scillaExtras.dsBlockNum,
                                       mCpsContext.scillaExtras.origin,
                                       mArgs.dest, scillaVersion)) {
    span.SetError("Unable to set BCInfor provider");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  const auto runnerResult = InvokeScillaInterpreter(INVOKE_TYPE::RUNNER_CALL);

  if (!runnerResult.isSuccess) {
    span.SetError("Interpreter run is not successful");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  const auto parseCallResults = ScillaHelpersCall::ParseCallContract(
      mAccountStore, mArgs, runnerResult.returnVal, receipt, scillaVersion);

  if (!parseCallResults.success) {
    // Allow TrapScilla call to fail and let EVM handle errored run accordingly
    if (GetType() == CpsRun::TrapScillaCall) {
      return {TxnStatus::NOT_PRESENT, true, retScillaVal};
    }
    span.SetError("Parsing call result failed");
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  // Only transfer funds when accepted is true
  if (parseCallResults.accepted) {
    LOG_GENERAL(WARNING, "Contract accepted amount, transferring");
    if (!mAccountStore.TransferBalanceAtomic(mArgs.from, mArgs.dest,
                                             mArgs.value)) {
      span.SetError("Unable to transfer requested balance");
      return {TxnStatus::INSUFFICIENT_BALANCE, false, retScillaVal};
    }
  }

  auto availableGas = mArgs.gasLimit;

  LOG_GENERAL(WARNING,
              "NUMBER OF MESSAGES: " << std::size(parseCallResults.entries));
  // Check if there's another level of runs that may generate events
  if (!std::empty(parseCallResults.entries)) {
    receipt.AddEdge();
  }
  // Schedule runs for execution in reverse order since we're putting them on
  // stack, so they should be run in the same order as stored in 'entries'
  // vector
#ifdef __APPLE__
  for (int i = parseCallResults.entries.size(); i > 0; --i) {
    const auto& nextRunInput = parseCallResults.entries[i];
#else
  for (const auto& nextRunInput :
       parseCallResults.entries | std::views::reverse) {
#endif
    INC_STATUS(GetCPSMetric(), "Scilla", "NewTransition");
    if (availableGas < CONTRACT_INVOKE_GAS) {
      span.SetError("Insufficient gas limit");
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, retScillaVal};
    }
    availableGas -= CONTRACT_INVOKE_GAS;
    if (!mAccountStore.AccountExistsAtomic(nextRunInput.nextAddress)) {
      mAccountStore.AddAccountAtomic(nextRunInput.nextAddress);
    }
    retScillaVal = ScillaResult{std::min(
        mCpsContext.scillaExtras.gasLimit - callPenalty, availableGas)};
    // If next run is non-contract -> transfer only
    if (!nextRunInput.isNextContract) {
      auto nextRun = std::make_shared<CpsRunTransfer>(
          mExecutor, mCpsContext, retScillaVal, mArgs.dest,
          nextRunInput.nextAddress, nextRunInput.amount);
      mExecutor.PushRun(std::move(nextRun));
    } else {
      auto newArgs = ScillaArgs{.from = mArgs.dest,
                                .dest = nextRunInput.nextAddress,
                                .origin = mArgs.origin,
                                .value = nextRunInput.amount,
                                .calldata = nextRunInput.nextInputMessage,
                                .edge = mArgs.edge + 1,
                                .depth = mArgs.depth + 1,
                                .gasLimit = availableGas};

      auto nextRun = std::make_shared<CpsRunScilla>(
          std::move(newArgs), mExecutor, mCpsContext, CpsRun::Call);
      mExecutor.PushRun(nextRun);
    }
  }

  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.from);
  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.dest);
  LOG_GENERAL(WARNING,
              "GAS, left: " << mCpsContext.scillaExtras.gasLimit - callPenalty
                            << ", margs.gasLimit: " << mArgs.gasLimit);
  retScillaVal.isSuccess = true;
  return {TxnStatus::NOT_PRESENT, true, retScillaVal};
}

ScillaInvokeResult CpsRunScilla::InvokeScillaInterpreter(INVOKE_TYPE type) {
  bool callAlreadyFinished = false;

  bool isLibrary = false;
  std::vector<Address> extlibs;
  uint32_t scillaVersion;
  std::string interprinterPrint;

  if (!mAccountStore.GetContractAuxiliaries(mArgs.dest, isLibrary,
                                            scillaVersion, extlibs)) {
    return {};
  }
  if (!ScillaUtils::PrepareRootPathWVersion(
          scillaVersion, mAccountStore.GetScillaRootVersion())) {
    return {};
  }

  using namespace zil::trace;
  auto func2 = [this, &interprinterPrint, type, &scillaVersion, &isLibrary,
                &callAlreadyFinished,
                trace_info =
                    Tracing::GetActiveSpan().GetIds()]() mutable -> void {
    auto span = Tracing::CreateChildSpanOfRemoteTrace(
        FilterClass::FILTER_CLASS_ALL, "InvokeScilla", trace_info);

    INC_STATUS(GetCPSMetric(), "error", "Rpc exception");

    switch (type) {
      case INVOKE_TYPE::CHECKER: {
        INC_STATUS(GetCPSMetric(), "ScillaInterpreterInvoke", "checker");
        if (!ScillaClient::GetInstance().CallChecker(
                scillaVersion,
                ScillaUtils::GetContractCheckerJson(
                    mAccountStore.GetScillaRootVersion(), isLibrary,
                    mArgs.gasLimit),
                interprinterPrint)) {
        }
        break;
      }
      case INVOKE_TYPE::RUNNER_CREATE: {
        INC_STATUS(GetCPSMetric(), "ScillaInterpreterInvoke", "create");
        if (!ScillaClient::GetInstance().CallRunner(
                scillaVersion,
                ScillaUtils::GetCreateContractJson(
                    mAccountStore.GetScillaRootVersion(), isLibrary,
                    mArgs.gasLimit, mArgs.value.toQa()),
                interprinterPrint)) {
        }
        break;
      }
      case INVOKE_TYPE::RUNNER_CALL: {
        INC_STATUS(GetCPSMetric(), "ScillaInterpreterInvoke", "call");
        if (!ScillaClient::GetInstance().CallRunner(
                scillaVersion,
                ScillaUtils::GetCallContractJson(
                    mAccountStore.GetScillaRootVersion(), mArgs.gasLimit,
                    mAccountStore.GetBalanceForAccountAtomic(mArgs.dest).toQa(),
                    isLibrary),
                interprinterPrint)) {
        }
        break;
      }
      case INVOKE_TYPE::DISAMBIGUATE: {
        INC_STATUS(GetCPSMetric(), "ScillaInterpreterInvoke", "disambiguate");
        if (!ScillaClient::GetInstance().CallDisambiguate(
                scillaVersion, ScillaUtils::GetDisambiguateJson(),
                interprinterPrint)) {
        }
        break;
      }
    }
    {
      std::lock_guard lock{mAccountStore.GetScillaMutex()};
      callAlreadyFinished = true;
    }
    mAccountStore.GetScillaCondVariable().notify_all();
  };
  DetachedFunction(1, func2);

  {
    std::unique_lock<std::mutex> lock(mAccountStore.GetScillaMutex());
    mAccountStore.GetScillaCondVariable().wait(
        lock, [&callAlreadyFinished] { return callAlreadyFinished; });
    LOG_GENERAL(INFO, "Call functions already finished!");
  }

  if (mAccountStore.GetProcessTimeout()) {
    LOG_GENERAL(WARNING, "Txn processing timeout!");

    ScillaClient::GetInstance().CheckClient(scillaVersion, true);
    return {};
  }
  return {true, std::move(interprinterPrint)};
}

}  // namespace libCps