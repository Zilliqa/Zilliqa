#include "libCps/CpsExecutor.h"
#include "libCps/Amount.h"
#include "libCps/CpsExecuteValidator.h"

#include "libData/AccountData/EvmProcessContext.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libUtils/Logger.h"

namespace libCps {

CpsExecutor::CpsExecutor(CpsAccountStoreInterface& account_store)
    : m_account_store(account_store) {}

CpsExecuteResult CpsExecutor::preValidateRun(
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

CpsExecuteResult CpsExecutor::Run(const EvmProcessContext& context) {
  const auto pre_validate_result = preValidateRun(context);
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
