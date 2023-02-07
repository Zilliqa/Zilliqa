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
#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsContext.h"
#include "libCps/CpsExecutor.h"
#include "libCps/CpsRunTransfer.h"
#include "libCps/ScillaHelpers.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/scilla/ScillaClient.h"
#include "libScilla/ScillaUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/TimeUtils.h"

#include <future>

namespace libCps {

CpsRunScilla::CpsRunScilla(ScillaArgs args, CpsExecutor& executor,
                           CpsContext& ctx, CpsRun::Type type)
    : CpsRun(executor.GetAccStoreIface(), CpsRun::Domain::Scilla, type),
      mArgs(std::move(args)),
      mExecutor(executor),
      mCpsContext(ctx) {}

CpsExecuteResult CpsRunScilla::Run(TransactionReceipt& receipt) {
  if (GetType() != CpsRun::Call && GetType() != CpsRun::Create) {
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
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, retScillaVal};
  }

  if (!mAccountStore.TransferBalanceAtomic(mArgs.from, mArgs.dest,
                                           mArgs.value)) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, retScillaVal};
  }

  if (!mAccountStore.InitContract(mArgs.dest, codedata.code, codedata.data,
                                  mCpsContext.scillaExtras.blockNum)) {
    return {TxnStatus::FAIL_CONTRACT_INIT, false, retScillaVal};
  }

  std::vector<Address> extlibs;
  bool isLibrary = false;
  uint32_t scillaVersion;
  std::map<Address, std::pair<std::string, std::string>> extlibsExports;

  if (!mAccountStore.GetContractAuxiliaries(mArgs.dest, isLibrary,
                                            scillaVersion, extlibs)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (DISABLE_SCILLA_LIB && isLibrary) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::PopulateExtlibsExports(mAccountStore, scillaVersion,
                                             extlibs, extlibsExports)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::ExportCreateContractFiles(mAccountStore, mArgs.dest,
                                                isLibrary, scillaVersion,
                                                extlibsExports)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!mAccountStore.SetBCInfoProvider(mCpsContext.scillaExtras.blockNum,
                                       mCpsContext.scillaExtras.dsBlockNum,
                                       mCpsContext.scillaExtras.origin,
                                       mArgs.dest, scillaVersion)) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mArgs.gasLimit -= SCILLA_CHECKER_INVOKE_GAS;

  retScillaVal =
      ScillaResult{std::min(retScillaVal.gasRemained,
                            mCpsContext.scillaExtras.gasLimit - createPenalty)};

  const auto checkerResult = InvokeScillaInterpreter(INVOKE_TYPE::CHECKER);
  if (!checkerResult.isSuccess) {
    receipt.AddError(CHECKER_FAILED);
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
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mArgs.gasLimit -= SCILLA_RUNNER_INVOKE_GAS;
  const auto runnerResult = InvokeScillaInterpreter(INVOKE_TYPE::RUNNER_CREATE);
  if (!runnerResult.isSuccess) {
    receipt.AddError(RUNNER_FAILED);
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  if (ScillaHelpers::ParseCreateContract(mArgs.gasLimit, runnerResult.returnVal,
                                         receipt, isLibrary)) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  t_metadata.emplace(mAccountStore.GenerateContractStorageKey(
                         mArgs.dest, CONTRACT_ADDR_INDICATOR, {}),
                     mArgs.dest.asBytes());

  if (!mAccountStore.UpdateStates(mArgs.dest, t_metadata, {}, true)) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  mAccountStore.MarkNewLibraryCreated(mArgs.dest);

  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.from);
  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.dest);

  return {TxnStatus::NOT_PRESENT, true, retScillaVal};
}

CpsExecuteResult CpsRunScilla::runCall(TransactionReceipt& receipt) {
  const auto callPenalty =
      std::max(CONTRACT_INVOKE_GAS,
               static_cast<unsigned int>(mCpsContext.scillaExtras.data.size()));
  auto retScillaVal = ScillaResult{std::min(
      mCpsContext.scillaExtras.gasLimit - callPenalty, mArgs.gasLimit)};

  if (!mAccountStore.AccountExistsAtomic(mArgs.dest)) {
    return {TxnStatus::INVALID_TO_ACCOUNT, false, retScillaVal};
  }

  const auto currBalance = mAccountStore.GetBalanceForAccountAtomic(mArgs.from);
  if (mArgs.value > currBalance) {
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
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (DISABLE_SCILLA_LIB && isLibrary) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (!ScillaHelpers::PopulateExtlibsExports(mAccountStore, scillaVersion,
                                             extlibs, extlibsExports)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
  }

  if (std::holds_alternative<ScillaArgs::CodeData>(mArgs.calldata)) {
    const auto& calldata = std::get<ScillaArgs::CodeData>(mArgs.calldata);
    if (!ScillaHelpers::ExportCallContractFiles(
            mAccountStore, mArgs.from, mArgs.dest, calldata.data, mArgs.value,
            scillaVersion, extlibsExports)) {
      return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
    }

  } else {
    const auto& jsonData = std::get<Json::Value>(mArgs.calldata);
    if (!ScillaHelpers::ExportCallContractFiles(mAccountStore, mArgs.from,
                                                jsonData, scillaVersion,
                                                extlibsExports)) {
      return {TxnStatus::FAIL_SCILLA_LIB, false, retScillaVal};
    }
  }

  if (!mAccountStore.SetBCInfoProvider(mCpsContext.scillaExtras.blockNum,
                                       mCpsContext.scillaExtras.dsBlockNum,
                                       mCpsContext.scillaExtras.origin,
                                       mArgs.dest, scillaVersion)) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  const auto runnerResult = InvokeScillaInterpreter(INVOKE_TYPE::RUNNER_CALL);

  if (!runnerResult.isSuccess) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  const auto parseCallResults = ScillaHelpers::ParseCallContract(
      mAccountStore, mArgs, runnerResult.returnVal, receipt, scillaVersion);

  if (!parseCallResults.success) {
    return {TxnStatus::ERROR, false, retScillaVal};
  }

  // Only transfer funds when accepted is true
  if (parseCallResults.accepted) {
    if (!mAccountStore.TransferBalanceAtomic(mArgs.from, mArgs.dest,
                                             mArgs.value)) {
      return {TxnStatus::INSUFFICIENT_BALANCE, false, retScillaVal};
    }
  }

  auto availableGas = mArgs.gasLimit;

  for (const auto& nextRunInput : parseCallResults.entries) {
    if (availableGas < CONTRACT_INVOKE_GAS) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, retScillaVal};
    }
    availableGas -= CONTRACT_INVOKE_GAS;
    if (!mAccountStore.AccountExistsAtomic(nextRunInput.nextAddress)) {
      mAccountStore.AddAccountAtomic(nextRunInput.nextAddress);
    }
    // If next run is non-contract -> transfer only
    if (!nextRunInput.isNextContract) {
      auto nextRun = std::make_shared<CpsRunTransfer>(
          mExecutor, mCpsContext, mArgs.from, nextRunInput.nextAddress,
          nextRunInput.amount);
      mExecutor.PushRun(std::move(nextRun));
    } else {
      const auto newArgs = ScillaArgs{.from = mArgs.dest,
                                      .dest = nextRunInput.nextAddress,
                                      .origin = mArgs.origin,
                                      .value = nextRunInput.amount,
                                      .calldata = nextRunInput.nextInputMessage,
                                      .edge = mArgs.edge,
                                      .depth = mArgs.depth + 1,
                                      .gasLimit = availableGas};

      auto nextRun = std::make_shared<CpsRunScilla>(
          std::move(newArgs), mExecutor, mCpsContext, CpsRun::Call);
      mExecutor.PushRun(nextRun);
    }
  }

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

  auto func2 = [this, &interprinterPrint, type, &scillaVersion, &isLibrary,
                &callAlreadyFinished]() mutable -> void {
    switch (type) {
      case INVOKE_TYPE::CHECKER:
        if (!ScillaClient::GetInstance().CallChecker(
                scillaVersion,
                ScillaUtils::GetContractCheckerJson(
                    mAccountStore.GetScillaRootVersion(), isLibrary,
                    mArgs.gasLimit),
                interprinterPrint)) {
        }
        break;
      case INVOKE_TYPE::RUNNER_CREATE:
        if (!ScillaClient::GetInstance().CallRunner(
                scillaVersion,
                ScillaUtils::GetCreateContractJson(
                    mAccountStore.GetScillaRootVersion(), isLibrary,
                    mArgs.gasLimit, mArgs.value.toQa()),
                interprinterPrint)) {
        }
        break;
      case INVOKE_TYPE::RUNNER_CALL:
        if (!ScillaClient::GetInstance().CallRunner(
                scillaVersion,
                ScillaUtils::GetCallContractJson(
                    mAccountStore.GetScillaRootVersion(), mArgs.gasLimit,
                    mArgs.value.toQa(), isLibrary),
                interprinterPrint)) {
        }
        break;
      case INVOKE_TYPE::DISAMBIGUATE:
        if (!ScillaClient::GetInstance().CallDisambiguate(
                scillaVersion, ScillaUtils::GetDisambiguateJson(),
                interprinterPrint)) {
        }
        break;
    }
    callAlreadyFinished = true;
    mAccountStore.GetScillaCondVariable().notify_all();
  };
  DetachedFunction(1, func2);

  {
    std::unique_lock<std::mutex> lk(mAccountStore.GetScillaMutex());
    if (!callAlreadyFinished) {
      mAccountStore.GetScillaCondVariable().wait(lk);
    } else {
      LOG_GENERAL(INFO, "Call functions already finished!");
    }
  }

  if (mAccountStore.GetProcessTimeout()) {
    LOG_GENERAL(WARNING, "Txn processing timeout!");

    ScillaClient::GetInstance().CheckClient(scillaVersion, true);
    return {};
  }
  return {true, std::move(interprinterPrint)};
}

}  // namespace libCps