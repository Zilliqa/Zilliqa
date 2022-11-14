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

#ifndef ZILLIQA_SRC_LIBUTILS_EVMCALLPARAMETERS_H_
#define ZILLIQA_SRC_LIBUTILS_EVMCALLPARAMETERS_H_

#include <string>

#include "common/BaseType.h"

// input parameters to Json call
struct EvmCallExtras {
  uint128_t block_timestamp{};
  uint64_t block_gas_limit{};
  uint64_t block_difficulty{};
  uint64_t block_number{};
  std::string gas_price{};
};

struct EvmCallParameters {
  std::string m_contract;
  std::string m_caller;
  std::string m_code;
  std::string m_data;
  uint64_t m_available_gas = {0};
  boost::multiprecision::uint256_t m_apparent_value = {0};
  EvmCallExtras m_extras;
  bool m_onlyEstimateGas = false;
};

// template<typename T>
inline std::ostream& operator<<(std::ostream& o, const EvmCallExtras& a) {
  o << "block_timestamp: " << a.block_timestamp << std::endl;
  o << "block_gas_limit: " << a.block_gas_limit << std::endl;
  o << "block_difficulty:  " << a.block_difficulty << std::endl;
  o << "block_number:  " << a.block_number << std::endl;
  o << "gas_price:  " << a.gas_price << std::endl;

  return o;
}

// template<typename T>
inline std::ostream& operator<<(std::ostream& o, const EvmCallParameters& a) {
  o << "m_contract: " << a.m_contract << std::endl;
  o << "m_caller: " << a.m_caller << std::endl;
  o << "m_code: " << a.m_code << std::endl;
  o << "m_data: " << a.m_data << std::endl;
  o << "m_available_gas: " << a.m_available_gas << std::endl;
  o << "m_apparent_value: " << a.m_apparent_value << std::endl;
  o << "m_extras: { " << a.m_extras << " }" << std::endl;
  o << "m_onlyEstimateGas: " << a.m_onlyEstimateGas << std::endl;

  return o;
}

#endif  // ZILLIQA_SRC_LIBUTILS_EVMCALLPARAMETERS_H_
