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
#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_LOCALMETRICSEVM_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_LOCALMETRICSEVM_H_

#include "libMetrics/Api.h"
#include "libMetrics/internal/scope.h"

// Soecial macro to keep code space small.
// we will susume these in the library next version.

namespace evm {

    Z_I64METRIC &GetInvocationsCounter() {
        static Z_I64METRIC counter{Z_FL::ACCOUNTSTORE_EVM, "evm.invocations.count",
                                   "Metrics for AccountStore", "calls"};
        return counter;
    }

    zil::metrics::uint64Counter_t
    GetNewCounter() {
        return Metrics::GetMeter()->CreateUInt64Counter("evm.messages", "count of calls to update", "calls");

    }

// Define it as a const because we need to set a view for the boundaries with
// the same name
    const char *EVM_HISTOGRAM = "evm.latency.histogram";

    Z_DBLHIST &GetHistogramCounter() {
        static std::list<double> latencieBoudaries{0, 1, 2, 3, 4, 5,
                                                   10, 20, 30, 40, 60, 120};
        static Z_DBLHIST histogram{Z_FL::ACCOUNTSTORE_EVM, EVM_HISTOGRAM,
                                   latencieBoudaries, "latency histogram", "ms"};
        return histogram;
    }

}  // namespace evm

#define CALLS_LATENCY_MARKER(COUNTER, LATENCY, FILTER_CLASS)                 \
  zil::metrics::LatencyScopeMarker sc_marker{COUNTER, LATENCY, FILTER_CLASS, \
                                             __FILE__, __FUNCTION__ };

#define LOCAL_CALL_INCREMENT() \
  INCREMENT_METHOD_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM)

#define LOCAL_INCREMENT_CALLS_COUNTER(PARAMETER_KEY, PARAMETER_VALUE)     \
  INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM, \
                          PARAMETER_KEY, PARAMETER_VALUE);


#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_LOCALMETRICSEVM_H_