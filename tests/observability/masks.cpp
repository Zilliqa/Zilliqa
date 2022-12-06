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

#include <iostream>
#include "common/MetricFilters.h"
#include "libUtils/Metrics.h"



int main() {
  std::cout << "Hello, World here is an example of bit testing for the metrics flag" << std::endl;


  // read trace_configured_value from constants for a 64 bit vale that is the trace_flag
  /// eventually make this settable dynamically through a control plane or cmd.

  int counter = 0;

  // test to make sure the hex value is translated properly

  std::cout << "value of mask " << METRIC_ZILLIQA_MASK << std::endl;

  if (zil::metrics::Test::Enabled(TRACE_OFF)) std::cout << "No tracing is set" << std::endl;
  if (zil::metrics::Test::Enabled(TRACE_P2P)) std::cout << "P2P tracing on" << std::endl;
  if (zil::metrics::Test::Enabled(TRACE_DATABASE)) std::cout << "DB tracing on" << std::endl;
  if (zil::metrics::Test::Enabled(METRICS_EVM_RPC)) std::cout << "evm tracing on" << std::endl;
  if (zil::metrics::Test::Enabled(TRACE_SOME_SMELLY_CODE)) std::cout << "smelly code trace is on" << std::endl;

  std::cout << "val of counter " << counter << std::endl;

  if (zil::metrics::Test::Enabled(TRACE_DATABASE)) {
    counter++;
  }

  std::cout << "val of counter " << counter << std::endl;

  // some code later on or deeper in a function call.
  // these should be noops.

  if (zil::metrics::Test::Enabled(TRACE_DATABASE)) {
    counter++;
  }
  if (zil::metrics::Test::Enabled(TRACE_DATABASE)) { counter++; }
  if (zil::metrics::Test::Enabled(TRACE_DATABASE)) { counter++; }

  std::cout << "val of counter " << counter << std::endl;

  return 0;
}

