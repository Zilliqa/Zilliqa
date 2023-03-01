/*
 * Copyright (C) 2023 Zilliqa
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

/*
 * CpsRun is a base class used by a concrete Runner. It contains some common
 * methods and fields use by its derivatives.
 */

#ifndef ZILLIQA_SRC_LIBCPS_CPSMETRICS_H_
#define ZILLIQA_SRC_LIBCPS_CPSMETRICS_H_

#include "libMetrics/Api.h"

// General purpose macros used by CPS

inline Z_I64METRIC& GetCPSMetric() {
  static Z_I64METRIC counter{Z_FL::CPS_EVM, "cps.counter", "Calls into cps",
                             "calls"};
  return counter;
}

#define CREATE_SPAN(FILTER_CLASS, SENDER, RECIPIENT, ORIG, VALUE) \
  TRACE(FILTER_CLASS)                                             \
  span.SetAttribute("sender", SENDER);                            \
  span.SetAttribute("recipient", RECIPIENT);                      \
  span.SetAttribute("origin", ORIG);                              \
  span.SetAttribute("value", VALUE);

#endif  // ZILLIQA_SRC_LIBCPS_CPSMETRICS_H_
