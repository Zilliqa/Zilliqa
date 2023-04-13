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

#include "libCps/CpsRunTransfer.h"
#include "libCps/CpsAccountStoreInterface.h"
#include "libCps/CpsContext.h"
#include "libCps/CpsExecutor.h"

#include "libUtils/Logger.h"

namespace libCps {
CpsRunTransfer::CpsRunTransfer(CpsExecutor& executor, const CpsContext& ctx,
                               CpsExecuteResult::ResultType&& prevRunResult,
                               const Address& from, const Address& to,
                               const Amount& amount)
    : CpsRun(executor.GetAccStoreIface(), CpsRun::Domain::None,
             CpsRun::Transfer),
      mCpsContext(ctx),
      mPreviousRunResult(std::move(prevRunResult)),
      mFrom(from),
      mTo(to),
      mAmount(amount) {}

CpsExecuteResult CpsRunTransfer::Run(TransactionReceipt& /*receipt*/) {
  LOG_MARKER();
  if (mCpsContext.isStatic) {
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }
  if (!mAccountStore.TransferBalanceAtomic(mFrom, mTo, mAmount)) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }
  mAccountStore.AddAddressToUpdateBufferAtomic(mFrom);
  mAccountStore.AddAddressToUpdateBufferAtomic(mTo);
  return {TxnStatus::NOT_PRESENT, true, mPreviousRunResult};
}

}  // namespace libCps