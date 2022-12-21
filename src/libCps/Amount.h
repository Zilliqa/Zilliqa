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

#ifndef ZILLIQA_SRC_LIBCPS_AMOUNT_H_
#define ZILLIQA_SRC_LIBCPS_AMOUNT_H_

#include "common/BaseType.h"
#include "common/Constants.h"

namespace libCps {
class Amount final {
 public:
  static Amount fromWei(const uint256_t& wei) { return Amount{wei}; }
  static Amount fromQa(const uint256_t& qa) {
    return Amount{qa * EVM_ZIL_SCALING_FACTOR};
  }
  uint256_t toWei() const { return m_value; }
  uint128_t toQa() const { return uint128_t{m_value / EVM_ZIL_SCALING_FACTOR}; }
  bool isZero() const { return m_value.is_zero(); }
  auto operator<=(const Amount& other) const {
    return m_value <= other.m_value;
  }
  auto operator>(const Amount& other) const { return !(*this <= other); }

 private:
  Amount(const uint256_t& wei) : m_value(wei){};

 private:
  uint256_t m_value;
};
}  // namespace libCps

#endif /* ZILLIQA_SRC_LIBCPS_AMOUNT_H_ */