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

#include "boost/multiprecision/cpp_int.hpp"

// input parameters to Json call
struct EvmCallExtras {
  uint64_t block_timestamp{};
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
};

#endif  // ZILLIQA_SRC_LIBUTILS_EVMCALLPARAMETERS_H_
