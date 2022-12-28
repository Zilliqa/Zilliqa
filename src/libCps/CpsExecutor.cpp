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
#include "libCps/CpsExecuteValidator.h"
#include "libCps/CpsRun.h"

#include "libData/AccountData/EvmProcessContext.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libUtils/Logger.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& account_store)
    : m_account_store(account_store) {}

CpsExecuteResult CpsExecutor::PreValidateRun(
    const EvmProcessContext& context) const {
  const auto owned = m_account_store.GetBalanceForAccount(
      context.GetTransaction().GetSenderAddr());

  const auto amount_result = CpsExecuteValidator::CheckAmount(context, owned);
  if (!amount_result.is_success) {
    return amount_result;
  }
  const auto gas_result = CpsExecuteValidator::CheckGasLimit(context);
  if (!gas_result.is_success) {
    return gas_result;
  }
  return {TxnStatus::NOT_PRESENT, true};
}

CpsExecutor::~CpsExecutor() = default;

void CpsExecutor::InitRun() { m_account_store.DiscardAtomics(); }

CpsExecuteResult CpsExecutor::Run(const EvmProcessContext& context) {
  InitRun();

  const auto pre_validate_result = PreValidateRun(context);
  if (!pre_validate_result.is_success) {
    return pre_validate_result;
  }

  if (context.GetTranID() != dev::h256{}) {
    LOG_GENERAL(WARNING, "...");
  }

  TransactionReceipt receipt;
  receipt.AddAccepted(true);

  if (m_account_store
          .GetBalanceForAccount(context.GetTransaction().GetSenderAddr())
          .toQa() != 1000) {
    LOG_GENERAL(WARNING, "...");
  }

  const auto result =
      CpsExecuteValidator::CheckAmount(context, Amount::fromQa(10000));
  if (result.is_success) {
    LOG_GENERAL(WARNING, "...");
  }

  return {};
}

}  // namespace libCps
