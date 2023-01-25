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

}  // namespace libCps
