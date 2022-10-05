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

#include <ostream>
#include <string>
#include "boost/multiprecision/cpp_int.hpp"

// input parameters to Json call

struct EvmCallParameters {
  const std::string m_contract;
  const std::string m_caller;
  const std::string m_code;
  const std::string m_data;
  const uint64_t m_available_gas = {0};
  const boost::multiprecision::uint256_t m_apparent_value = {0};
  const std::string m_tag;
};

inline std::ostream& operator<<(std::ostream& os,
                                const EvmCallParameters& evmCallParameters) {
  os << "{"                                                        //
     << "Contract:" << evmCallParameters.m_contract                //
     << ", Caller:" << evmCallParameters.m_caller                  //
     << ", Code:" << evmCallParameters.m_code                      //
     << ", Data:" << evmCallParameters.m_data                      //
     << ", Available gas:" << evmCallParameters.m_available_gas    //
     << ", Apparent value:" << evmCallParameters.m_apparent_value  //
     << ", Tag:" << evmCallParameters.m_tag                        //
     << "}";
  return os;
}
#endif  // ZILLIQA_SRC_LIBUTILS_EVMCALLPARAMETERS_H_
