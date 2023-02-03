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
  checkGas();

  if (GetType() == CpsRun::Create) {
    return runCreate(receipt);
  } else {
    return runCall(receipt);
  }
}

CpsExecuteResult CpsRunScilla::checkGas() {
  if (GetType() == CpsRun::Create) {
    const auto createPenalty = std::max(
        CONTRACT_CREATE_GAS,
        static_cast<unsigned int>(mArgs.code.size() + mArgs.data.size()));
    const auto scillaGas = SCILLA_CHECKER_INVOKE_GAS + SCILLA_RUNNER_INVOKE_GAS;
    const auto requiredGas = std::max(scillaGas, createPenalty);
    if (mArgs.gasLimit < requiredGas) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  } else if (GetType() == CpsRun::Call) {
    uint64_t requiredGas = std::max(
        CONTRACT_INVOKE_GAS, static_cast<unsigned int>(mArgs.data.size()));

    requiredGas += SCILLA_RUNNER_INVOKE_GAS;
    mArgs.gasLimit -= requiredGas;
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsRunScilla::runCreate(TransactionReceipt& receipt) {
  mArgs.dest =
      mAccountStore.GetAddressForContract(mArgs.from, TRANSACTION_VERSION);
  if (!mAccountStore.AddAccountAtomic(mArgs.dest)) {
    return {TxnStatus::FAIL_CONTRACT_ACCOUNT_CREATION, false, {}};
  }

  if (!mAccountStore.TransferBalanceAtomic(mArgs.from, mArgs.dest,
                                           mArgs.value)) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  if (!mAccountStore.InitContract(mArgs.dest, mArgs.code, mArgs.data,
                                  mArgs.blockNum)) {
    return {TxnStatus::FAIL_CONTRACT_INIT, false, {}};
  }

  std::vector<Address> extlibs;
  bool isLibrary = false;
  uint32_t scillaVersion;
  std::map<Address, std::pair<std::string, std::string>> extlibsExports;

  if (!mAccountStore.GetContractAuxiliaries(mArgs.dest, isLibrary,
                                            scillaVersion, extlibs)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, {}};
  }

  if (DISABLE_SCILLA_LIB && isLibrary) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, {}};
  }

  if (!ScillaHelpers::PopulateExtlibsExports(mAccountStore, scillaVersion,
                                             extlibs, extlibsExports)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, {}};
  }

  if (!ScillaHelpers::ExportCreateContractFiles(mAccountStore, mArgs.dest,
                                                isLibrary, scillaVersion,
                                                extlibsExports)) {
    return {TxnStatus::FAIL_SCILLA_LIB, false, {}};
  }

  if (!mAccountStore.SetBCInfoProvider(mArgs.blockNum, mArgs.dsBlockNum,
                                       mArgs.from, mArgs.dest, scillaVersion)) {
    return {TxnStatus::ERROR, false, {}};
  }

  mArgs.gasLimit -= SCILLA_CHECKER_INVOKE_GAS;

  const auto checkerResult = InvokeScillaInterpreter(INVOKE_TYPE::CHECKER);
  if (!checkerResult.isSuccess) {
    receipt.AddError(CHECKER_FAILED);
    return {TxnStatus::ERROR, false, ScillaResult{mArgs.gasLimit}};
  }

  std::map<std::string, zbytes> t_metadata;
  t_metadata.emplace(
      mAccountStore.GenerateContractStorageKey(mArgs.dest,
                                               SCILLA_VERSION_INDICATOR, {}),
      DataConversion::StringToCharArray(std::to_string(scillaVersion)));

  if (!ScillaHelpers::ParseContractCheckerOutput(
          mAccountStore, mArgs.dest, checkerResult.returnVal, receipt,
          t_metadata, mArgs.gasLimit, isLibrary)) {
    return {TxnStatus::ERROR, false, ScillaResult{mArgs.gasLimit}};
  }

  mArgs.gasLimit -= SCILLA_RUNNER_INVOKE_GAS;
  const auto runnerResult = InvokeScillaInterpreter(INVOKE_TYPE::RUNNER_CREATE);
  if (!runnerResult.isSuccess) {
    receipt.AddError(RUNNER_FAILED);
    return {TxnStatus::ERROR, false, ScillaResult{mArgs.gasLimit}};
  }

  if (ScillaHelpers::ParseCreateContract(mArgs.gasLimit, runnerResult.returnVal,
                                         receipt, isLibrary)) {
    return {TxnStatus::ERROR, false, ScillaResult{mArgs.gasLimit}};
  }

  t_metadata.emplace(mAccountStore.GenerateContractStorageKey(
                         mArgs.dest, CONTRACT_ADDR_INDICATOR, {}),
                     mArgs.dest.asBytes());

  if (!mAccountStore.UpdateStates(mArgs.dest, t_metadata, {}, true)) {
    return {TxnStatus::ERROR, false, ScillaResult{mArgs.gasLimit}};
  }

  mAccountStore.MarkNewLibraryCreated(mArgs.dest);

  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.from);
  mAccountStore.AddAddressToUpdateBufferAtomic(mArgs.dest);

  (void)mExecutor;
  (void)mCpsContext;
  return {TxnStatus::NOT_PRESENT, true, ScillaResult{mArgs.gasLimit}};
}

CpsExecuteResult CpsRunScilla::runCall(TransactionReceipt& receipt) {
  (void)receipt;
  return {};
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