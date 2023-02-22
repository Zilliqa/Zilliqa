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

#include "libCps/CpsUtils.h"
#include "common/Constants.h"
#include "libCrypto/EthCrypto.h"
#include "libData/AccountStore/services/evm/EvmProcessContext.h"
#include "libUtils/DataConversion.h"
#include "libUtils/EvmUtils.h"
#include "libUtils/GasConv.h"

namespace libCps {

evm::EvmEvalExtras CpsUtils::FromScillaContext(
    const ScillaProcessContext& scillaCtx) {
  evm::EvmEvalExtras extras;
  extras.set_chain_id(ETH_CHAINID);
  extras.set_block_timestamp(scillaCtx.blockTimestamp.convert_to<uint64_t>());
  extras.set_block_gas_limit(DS_MICROBLOCK_GAS_LIMIT *
                             GasConv::GetScalingFactor());
  extras.set_block_difficulty(scillaCtx.blockDifficulty);
  extras.set_block_number(scillaCtx.blockNum);
  uint256_t gasPrice = (scillaCtx.gasPrice * EVM_ZIL_SCALING_FACTOR) /
                       GasConv::GetScalingFactor();
  // The following ensures we get 'at least' that high price as it was before
  // dividing by GasScalingFactor
  gasPrice += EVM_ZIL_SCALING_FACTOR;
  *extras.mutable_gas_price() = UIntToProto(gasPrice);

  return extras;
}

ScillaProcessContext CpsUtils::FromEvmContext(
    const EvmProcessContext& evmContext) {
  uint256_t gasPrice =
      ProtoToUint(evmContext.GetEvmArgs().extras().gas_price());
  uint256_t value = ProtoToUint(evmContext.GetEvmArgs().apparent_value());
  return ScillaProcessContext{
      .origin = ProtoToAddress(evmContext.GetEvmArgs().origin()),
      .recipient = ProtoToAddress(evmContext.GetEvmArgs().address()),
      .code = StripEVM(
          DataConversion::StringToCharArray(evmContext.GetEvmArgs().code())),
      .data = DataConversion::StringToCharArray(evmContext.GetEvmArgs().data()),
      .amount = uint128_t{value / EVM_ZIL_SCALING_FACTOR},
      .gasPrice = uint128_t{(gasPrice * GasConv::GetScalingFactor()) /
                            EVM_ZIL_SCALING_FACTOR},
      .gasLimit =
          GasConv::GasUnitsFromEthToCore(evmContext.GetEvmArgs().gas_limit()),
      .blockNum = 0,
      .dsBlockNum = 0,
      .blockTimestamp = 0,
      .blockDifficulty = 0,
      // Not relevant
      .contractType = Transaction::ContractType::ERROR,
  };
}

}  // namespace libCps