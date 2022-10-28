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

#ifndef ZILLIQA_SRC_LIBETH_UTILS_ETHUTILS_H_
#define ZILLIQA_SRC_LIBETH_UTILS_ETHUTILS_H_

#include <cstdint>
#include "common/BaseType.h"

namespace Eth {

uint64_t getGasUnitsForContractDeployment(const std::string& code,
                                          const std::string& data);

}  // namespace Eth

#endif  // ZILLIQA_SRC_LIBETH_UTILS_ETHUTILS_H_
