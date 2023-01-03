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

#ifndef ZILLIQA_SRC_LIBUTILS_IPCONVERTER_H_
#define ZILLIQA_SRC_LIBUTILS_IPCONVERTER_H_

#include "common/BaseType.h"

#include <arpa/inet.h>
#include <string>

/// Utility class for converter from ip address string to numerical
/// represetation.

namespace IPConverter {

const std::string ToStrFromNumericalIP(const uint128_t&);

bool GetIPPortFromSocket(std::string, std::string&, int&);

bool ToNumericalIPFromStr(const std::string&, uint128_t&);

bool ResolveDNS(const std::string& url, const uint32_t& port, uint128_t& ipInt);
}  // namespace IPConverter

#endif  // ZILLIQA_SRC_LIBUTILS_IPCONVERTER_H_
