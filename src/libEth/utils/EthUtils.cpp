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

#include "EthUtils.h"
#include "common/Constants.h"
#include "libUtils/DataConversion.h"

namespace Eth {
uint64_t getGasUnitsForContractDeployment(const std::string& code,
                                          const std::string& data) {
  constexpr auto GAS_COST_FOR_ZERO_DATA = 4;
  constexpr auto GAS_COST_FOR_NON_ZERO_DATA = 16;
  constexpr auto CONTRACT_DEPLOYMENT_BASE_FEE = 32000;
  LOG_GENERAL(WARNING, "Contract size for gas units, code: "
                           << code.size() << ", data: " << data.size());
  uint64_t gas = 0;
  auto calculateGas = [&gas](std::string input) {
    if (input.size() <= 2) {
      return;
    }
    if (input[0] == 'E' && input[1] == 'V' && input[2] == 'M') {
      input = {input.begin() + 3, input.end()};
    }
    const auto bytes = DataConversion::HexStrToUint8VecRet(input);
    for (const auto& byte : bytes) {
      if (byte == 0) {
        gas += GAS_COST_FOR_ZERO_DATA;
      } else {
        gas += GAS_COST_FOR_NON_ZERO_DATA;
      }
    }
  };
  calculateGas(code);
  calculateGas(data);
  return MIN_ETH_GAS + CONTRACT_DEPLOYMENT_BASE_FEE + gas;
}
}  // namespace Eth