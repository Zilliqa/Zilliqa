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

#include <variant>

namespace libCps {
class Amount final {
 public:
  constexpr Amount() : m_value(uint256_t{0}) {}
  constexpr static Amount fromWei(const uint256_t& wei) { return Amount{wei}; }
  constexpr static Amount fromQa(const uint256_t& qa) {
    return Amount{qa * EVM_ZIL_SCALING_FACTOR};
  }
  constexpr uint256_t toWei() const {
    if (std::holds_alternative<uint128_t>(m_value)) {
      return uint256_t{std::get<uint128_t>(m_value) * EVM_ZIL_SCALING_FACTOR};
    }
    return std::get<uint256_t>(m_value);
  }
  constexpr uint128_t toQa() const {
    if (std::holds_alternative<uint256_t>(m_value)) {
      return uint128_t{std::get<uint256_t>(m_value) / EVM_ZIL_SCALING_FACTOR};
    }
    return std::get<uint128_t>(m_value);
  }
  constexpr auto operator<=(const Amount& other) const {
    return toQa() <= other.toQa();
  }
  constexpr auto operator>(const Amount& other) const {
    return !(*this <= other);
  }
  constexpr auto operator+(const Amount& rhs) const {
    // return the sum of two numbers as biggest type to avoid overflow
    return Amount{this->toWei() + rhs.toWei()};
  }
  constexpr auto operator-(const Amount& rhs) const {
    // return the sum of two numbers as biggest type to avoid overflow
    return Amount{this->toWei() - rhs.toWei()};
  }
  constexpr auto operator==(const Amount& rhs) const {
    return this->toWei() == rhs.toWei();
  }

 private:
  constexpr Amount(const uint256_t& wei) : m_value(wei){};

 private:
  std::variant<uint128_t, uint256_t> m_value;
};
}  // namespace libCps

#endif  // ZILLIQA_SRC_LIBCPS_AMOUNT_H_