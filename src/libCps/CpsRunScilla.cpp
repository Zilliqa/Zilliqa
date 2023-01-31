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
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/AccountStore/services/scilla/ScillaClient.h"
#include "libScilla/ScillaUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Metrics.h"

#include <future>

namespace libCps {

CpsRunScilla::CpsRunScilla(ScillaArgs args, CpsExecutor& executor,
                           CpsContext& ctx, CpsRun::Type type)
    : CpsRun(executor.GetAccStoreIface(), CpsRun::Domain::Scilla, type),
      mArgs(std::move(args)),
      mExecutor(executor),
      mCpsContext(ctx) {}

CpsExecuteResult CpsRunScilla::Run(TransactionReceipt& receipt) {
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