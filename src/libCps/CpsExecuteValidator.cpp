/*
 * Copyright (C) 2019 Zilliqa
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

#include "libCps/CpsExecuteValidator.h"
#include "libCps/Amount.h"

#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libData/AccountStore/services/scilla/ScillaProcessContext.h"
#include "libEth/utils/EthUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/SafeMath.h"

namespace libCps {

CpsExecuteResult CpsExecuteValidator::CheckAmount(
    const EvmProcessContext& context, const Amount& owned) {
  uint256_t gasDepositWei;
  if (!SafeMath<uint256_t>::mul(context.GetTransaction().GetGasLimitZil(),
                                GetGasPriceWei(context), gasDepositWei)) {
    return {TxnStatus::MATH_ERROR, false, {}};
  }

  const auto claimed =
      Amount::fromWei(gasDepositWei + context.GetTransaction().GetAmountWei());
  if (claimed > owned) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}
CpsExecuteResult CpsExecuteValidator::CheckGasLimit(
    const EvmProcessContext& context) {
  const auto requested_limit = context.GetEvmArgs().gas_limit();

  if (context.GetContractType() == Transaction::CONTRACT_CREATION) {
    const auto baseFee = Eth::getGasUnitsForContractDeployment(
        DataConversion::StringToCharArray(context.GetEvmArgs().code()),
        context.GetData());

    // Check if gaslimit meets the minimum requirement for contract deployment
    if (requested_limit < baseFee) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }

  } else {
    if (requested_limit < MIN_ETH_GAS) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

uint128_t CpsExecuteValidator::GetGasPriceWei(
    const EvmProcessContext& context) {
  return context.GetEstimateOnly() ? 0
                                   : context.GetTransaction().GetGasPriceWei();
}

CpsExecuteResult CpsExecuteValidator::CheckAmount(
    const ScillaProcessContext& context, const Amount& owned) {
  uint128_t gasDepositQa;
  if (!SafeMath<uint128_t>::mul(context.gasLimit, context.gasPrice,
                                gasDepositQa)) {
    return {TxnStatus::MATH_ERROR, false, {}};
  }

  const auto claimed = Amount::fromQa(gasDepositQa + context.amount);
  if (claimed > owned) {
    return {TxnStatus::INSUFFICIENT_BALANCE, false, {}};
  }

  return {TxnStatus::NOT_PRESENT, true, {}};
}

CpsExecuteResult CpsExecuteValidator::CheckGasLimit(
    const ScillaProcessContext& context) {
  // Regular charge for simple transfer
  if (context.contractType == Transaction::ContractType::NON_CONTRACT) {
    if (context.gasLimit < NORMAL_TRAN_GAS) {
      return {TxnStatus::INSUFFICIENT_GAS, false, {}};
    }
    // Contract creation
  } else if (context.contractType ==
             Transaction::ContractType::CONTRACT_CREATION) {
    uint64_t requiredGas = std::max(
        CONTRACT_CREATE_GAS,
        static_cast<unsigned int>(context.code.size() + context.data.size()));

    requiredGas += SCILLA_CHECKER_INVOKE_GAS;
    requiredGas += SCILLA_RUNNER_INVOKE_GAS;

    if (context.gasLimit < requiredGas) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }
  } else if (context.contractType == Transaction::ContractType::CONTRACT_CALL) {
    uint64_t requiredGas = std::max(
        CONTRACT_INVOKE_GAS, static_cast<unsigned int>(context.data.size()));

    requiredGas += SCILLA_RUNNER_INVOKE_GAS;
    if (context.gasLimit < requiredGas) {
      return {TxnStatus::INSUFFICIENT_GAS_LIMIT, false, {}};
    }

  } else {
    return {TxnStatus::INCORRECT_TXN_TYPE, false, {}};
  }
  return {TxnStatus::NOT_PRESENT, true, {}};
}

}  // namespace libCps
