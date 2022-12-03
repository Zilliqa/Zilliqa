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

// Warning the code depends on this being a linear enum with no overriding the numeric sequencing
// this use a c++ template trick to navigate the enum at compile time.

namespace zil {
namespace metrics {
enum InstrumentationClass {
  TRACE_P2P,
  TRACE_DATABASE,
  METRICS_EVM_RPC,
  TRACE_SOME_SMELLY_CODE,
};
}
}


#endif  // ZILLIQA_SRC_COMMON_METRICNAMES_H_
