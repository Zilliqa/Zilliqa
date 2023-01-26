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

// Soecial macro to keep code space small.
// we will susume these in the library next version.

namespace metric_sdk = opentelemetry::sdk::metrics;

namespace evm {

zil::metrics::uint64Counter_t &GetInvocationsCounter() {
  static auto counter = Metrics::GetInstance().CreateInt64Metric(
      "zilliqa.evm.invoke", "invocations_count", "Metrics for AccountStore",
      "calls");
  return counter;
}

// Define it as a const because we need to set a view for the boundaries with
// the same name
const char *EVM_HISTOGRAM = "zilliqa.evm.histogram";

zil::metrics::doubleHistogram_t &GetHistogramCounter() {
  static auto histogram = Metrics::GetMeter()->CreateDoubleHistogram(
      EVM_HISTOGRAM, "evm latency histogram", "ms");
  return histogram;
}

}  // namespace evm

#define LOCAL_EMT(MSG)                                                        \
  if (zil::metrics::Filter::GetInstance().Enabled(                            \
          zil::metrics::FilterClass::ACCOUNTSTORE_EVM)) {                     \
    Metrics::GetInstance().CaptureEMT(                                        \
        span, zil::metrics::FilterClass::ACCOUNTSTORE_EVM,                    \
        zil::trace::FilterClass::ACC_EVM, evm::GetInvocationsCounter(), MSG); \
  }

#define LOCAL_CALL_INCREMENT() \
  INCREMENT_METHOD_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM)

#define LOCAL_INCREMENT_CALLS_COUNTER(PARAMETER_KEY, PARAMETER_VALUE)     \
  INCREMENT_CALLS_COUNTER(evm::GetInvocationsCounter(), ACCOUNTSTORE_EVM, \
                          PARAMETER_KEY, PARAMETER_VALUE);

#define LOCAL_CALLS_LATENCY_MARKER()                 \
  CALLS_LATENCY_MARKER(evm::GetInvocationsCounter(), \
                       evm::GetHistogramCounter(),   \
                       zil::metrics::FilterClass::ACCOUNTSTORE_EVM);

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTSTORE_LOCALMETRICSEVM_H_