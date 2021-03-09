/*
 * Copyright (C) 2021 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBUTILS_ADDRESSCONVERSION_H_
#define ZILLIQA_SRC_LIBUTILS_ADDRESSCONVERSION_H_

#include "libData/AccountData/Address.h"

enum class AddressConversionCode {
  OK = 0,
  INVALID_ADDR,
  INVALID_BECH32_ADDR,
  WRONG_ADDR_SIZE,
};

const unsigned int HEX_ADDR_SIZE = ACC_ADDR_SIZE * 2;

inline bool HasZilHrp(const std::string& input);
AddressConversionCode ToAddressStructure(const std::string& addr,
                                         Address& retAddr);
AddressConversionCode ToBase16Addr(const std::string& addr, Address& retAddr);

#endif  // ZILLIQA_SRC_LIBUTILS_ADDRESSCONVERSION_H_
