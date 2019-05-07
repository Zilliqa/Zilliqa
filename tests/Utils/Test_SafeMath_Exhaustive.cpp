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

#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

#include <functional>

#define BOOST_TEST_MODULE safemath_exhaustive
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

// WiderInteger should have greater space than integer
// Also both should be unsigned or both should be signed
template <class Integer, class WiderInteger>
void test_function_exhaustive(
    function<bool(const Integer&, const Integer&, Integer&)> safemath_operator,
    function<WiderInteger(WiderInteger&, WiderInteger&)> generic_operator) {
  constexpr WiderInteger maxValue = numeric_limits<Integer>::max();
  constexpr WiderInteger minValue = numeric_limits<Integer>::min();
  LOG_GENERAL(INFO, "Min: " << minValue << " Max: " << maxValue);
  for (WiderInteger i = minValue; i <= maxValue; ++i) {
    for (WiderInteger j = minValue; j <= maxValue; ++j) {
      Integer res{};
      WiderInteger actual_result{};
      if (generic_operator == divides<WiderInteger>() && j == 0) {
        continue;
      }
      actual_result = generic_operator(i, j);
      if (!safemath_operator(i, j, res)) {
        BOOST_CHECK_MESSAGE(res != actual_result, "False positive in SafeMath "
                                                      << " " << res << " "
                                                      << actual_result);
        LOG_GENERAL(INFO,
                    "Correct Result: " << actual_result <<" operators: "<<i<<" "<<j;);
      } else {
        BOOST_CHECK_MESSAGE(res == actual_result,
                            "SafeMath wrong " << res << " " << actual_result);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE(safemath_exhaustive)

BOOST_AUTO_TEST_CASE(test_uint8_addition) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<uint8_t, uint32_t>(SafeMath<uint8_t>::add,
                                              plus<uint32_t>());
}

BOOST_AUTO_TEST_CASE(test_uint8_subtraction) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<uint8_t, uint32_t>(SafeMath<uint8_t>::sub,
                                              minus<uint32_t>());
}

BOOST_AUTO_TEST_CASE(test_int8_addition) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<int8_t, int32_t>(SafeMath<int8_t>::add,
                                            plus<int32_t>());
}

BOOST_AUTO_TEST_CASE(test_int8_subtraction) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<int8_t, int32_t>(SafeMath<int8_t>::sub,
                                            minus<int32_t>());
}

BOOST_AUTO_TEST_CASE(test_uint8_mul) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<uint8_t, uint32_t>(SafeMath<uint8_t>::mul,
                                              multiplies<uint32_t>());
}

BOOST_AUTO_TEST_CASE(test_int8_mul) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<int8_t, int32_t>(SafeMath<int8_t>::mul,
                                            multiplies<int32_t>());
}

BOOST_AUTO_TEST_CASE(test_uint8_div) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<uint8_t, uint32_t>(SafeMath<uint8_t>::div,
                                              divides<uint32_t>());
}

BOOST_AUTO_TEST_CASE(test_int8_div) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<int8_t, int32_t>(SafeMath<int8_t>::div,
                                            divides<int32_t>());
}


BOOST_AUTO_TEST_SUITE_END();
