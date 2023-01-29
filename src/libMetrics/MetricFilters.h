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

#ifndef ZILLIQA_SRC_LIBMETRICS_METRICFILTERS_H_
#define ZILLIQA_SRC_LIBMETRICS_METRICFILTERS_H_

// Currently maxes out at 64 filters, in order to increase developer should
// change the type of the mask from uint64_t to uint128_t or uint256_t if
// the number of filters ever increases beyond 64.
//
// Do not override the default numbering of these items, the algorithms rely
// upon these definitions being consecutive, so no assigning new numbers.

// To extend filter classes, you may add items, the total number is limited to
// 64 (bit mask)
#define METRICS_FILTER_CLASSES(M) \
  M(EVM_CLIENT)                   \
  M(EVM_CLIENT_LOW_LEVEL)         \
  M(SCILLA_IPC)                   \
  M(EVM_RPC)                      \
  M(LOOKUP_SERVER)                \
  M(MSG_DISPATCH)                 \
  M(ACCOUNTSTORE_EVM)             \
  M(ACCOUNTSTORE_SCILLA)          \
  M(ACCOUNTSTORE_HISTOGRAMS)\
  M(API_SERVER)

namespace zil {
namespace metrics {
enum class FilterClass {
#define ENUM_FILTER_CLASS(C) C,
  METRICS_FILTER_CLASSES(ENUM_FILTER_CLASS)
#undef ENUM_FILTER_CLASS
      FILTER_CLASS_END
};
}  // namespace metrics
}  // namespace zil

#endif  // ZILLIQA_SRC_LIBMETRICS_METRICFILTERS_H_
