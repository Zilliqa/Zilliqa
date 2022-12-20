#include "libCps/CpsExecutor.h"
#include "libCps/Amount.h"
#include "libCps/CpsExecuteValidator.h"

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/EvmProcessContext.h"
#include "libUtils/Logger.h"

namespace libCps {

CpsExecutor::CpsExecutor(AccountStore& account_store)
    : m_account_store(account_store) {}

CpsExecuteResult CpsExecutor::Run(const EvmProcessContext& context) {
  if (context.GetTranID() != dev::h256{}) {
    LOG_GENERAL(WARNING, "...");
  }
  if (m_account_store.GetAccount(Address{}) != nullptr) {
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
