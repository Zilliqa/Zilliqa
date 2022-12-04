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

#ifndef ZILLIQA_SRC_COMMON_METRICNAMES_H_
#define ZILLIQA_SRC_COMMON_METRICNAMES_H_

// Currently maxes out at 64 filters, in order to increase developer should
// change the type of the mask from uint64_t to uint128_t or uint256_t if
// the number of filters ever increases beyond 64.
//
// Do not override the default numbering of these items, the algorithms rely
// upon these definitions being consecutive, so no assigning new numbers.

namespace zil {
namespace metrics {
enum FilterClass {
  EVM_CLIENT,
  EVM_CLIENT_LOW_LEVEL,
  SCILLA_IPC,
  EVM_RPC,
  LOOKUP_SERVER,
  ANYTHING_YOU_LIKE_LEASE_EXTEND_JUST_ADD_ANY_FILTER_YOU_LIKE_UP_TO_64_OF_THEM
};
}
}  // namespace zil

#endif  // ZILLIQA_SRC_COMMON_METRICNAMES_H_
