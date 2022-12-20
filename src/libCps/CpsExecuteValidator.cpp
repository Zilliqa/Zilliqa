#include "CpsExecuteValidator.h"
#include "Amount.h"

#include "libData/AccountData/EvmProcessContext.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/SafeMath.h"

namespace libCps {

CpsExecuteResult CpsExecuteValidator::CheckAmount(
    const EvmProcessContext& context, const Amount& owned) {
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(context.GetTransaction().GetGasLimitZil(),
                                context.GetTransaction().GetGasPriceWei(),
                                gasDepositWei)) {
    return {TxnStatus::MATH_ERROR, false};
  }

  const auto claimed =
      Amount::fromWei(gasDepositWei + context.GetTransaction().GetAmountWei());
  if (claimed > owned) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false};
  }

  return {TxnStatus::NOT_PRESENT, true};
}
CpsExecuteResult CpsExecuteValidator::CheckGasLimit(
    const EvmProcessContext& context) {
  const auto requested_limit = context.GetTransaction().GetGasLimitEth();

  if (context.GetContractType() == Transaction::CONTRACT_CREATION) {
    const auto baseFee = Eth::getGasUnitsForContractDeployment(
        context.GetCode(), context.GetData());

    // Check if gaslimit meets the minimum requirement for contract deployment
    if (requested_limit < baseFee) {
      LOG_GENERAL(WARNING,
                  "Gas limit " << requested_limit << " less than " << baseFee);
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false};
    }

  } else {
    if (requested_limit < MIN_ETH_GAS) {
      LOG_GENERAL(WARNING, "Gas limit " << requested_limit << " less than "
                                        << MIN_ETH_GAS);
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false};
    }
  }
  return {};
}

}  // namespace libCps
