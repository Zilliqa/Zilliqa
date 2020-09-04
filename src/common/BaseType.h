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

#ifndef ZILLIQA_SRC_COMMON_BASETYPE_H_
#define ZILLIQA_SRC_COMMON_BASETYPE_H_

#include <stdint.h>
#include <map>
#include <utility>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

using bytes = std::vector<uint8_t>;
using uint128_t = boost::multiprecision::uint128_t;
using uint256_t = boost::multiprecision::uint256_t;
using GovDSShardVotesMap =
    std::map<uint32_t, std::pair<std::map<uint32_t, uint32_t>,
                                 std::map<uint32_t, uint32_t>>>;
using GovProposalIdVotePair = std::pair<uint32_t, uint32_t>;
#endif  // ZILLIQA_SRC_COMMON_BASETYPE_H_
