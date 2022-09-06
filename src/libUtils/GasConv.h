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

#ifndef ZILLIQA_SRC_LIBUTILS_GAS_CONV_H_
#define ZILLIQA_SRC_LIBUTILS_GAS_CONV_H_

#include <boost/variant.hpp>

#include "common/BaseType.h"
#include "common/Constants.h"

class GasConv {
 public:
  // static const uint64_t SCALE_ETH_TO_INTERNAL_GAS_LIMIT = MIN_ETH_GAS /
  // NORMAL_TRAN_GAS;
  static GasConv CreateFromCore(const uint128_t& gasPrice, uint64_t gasLimit) {
    return GasConv(gasPrice, gasLimit, gasLimit * GetScalingFactor());
  }

  static GasConv CreateFromEthApi(const uint256_t& gasPrice,
                                  uint64_t gasLimit = MIN_ETH_GAS) {
    return GasConv(gasPrice, gasLimit / GetScalingFactor(), gasLimit);
  }

  uint128_t GasPriceInCore() const {
    if (m_gas_price_storage.which() == 0) {
      return boost::get<uint128_t>(m_gas_price_storage);
    } else {
      return uint128_t{(boost::get<uint256_t>(m_gas_price_storage) /
                        EVM_ZIL_SCALING_FACTOR)};
    }
  }

  uint256_t GasPriceInEthApi() const {
    if (m_gas_price_storage.which() == 0) {
      return uint256_t{(boost::get<uint128_t>(m_gas_price_storage) *
                        EVM_ZIL_SCALING_FACTOR)};
    } else {
      return boost::get<uint256_t>(m_gas_price_storage);
    }
  }
  // Returns gas limit used in entire platform
  uint64_t GasLimitInCore() const { return m_gas_limit_core; }

  // Returns gas limit received from / sent to client using ETH Api
  uint64_t GasLimitInEthApi() const { return m_gas_limit_eth; }

 private:
  static uint64_t GetScalingFactor() { return MIN_ETH_GAS / NORMAL_TRAN_GAS; }

 private:
  template <typename T>
  GasConv(const T& gasPrice, uint64_t gasLimitCore, uint64_t gasLimitEth)
      : m_gas_price_storage(gasPrice),
        m_gas_limit_core(gasLimitCore),
        m_gas_limit_eth(gasLimitEth) {}
  const boost::variant<uint128_t, uint256_t> m_gas_price_storage;
  const uint64_t m_gas_limit_core;
  const uint64_t m_gas_limit_eth;
};

#endif  // ZILLIQA_SRC_LIBUTILS_GAS_CONV_H_