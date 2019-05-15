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

using typeToTest = uint8_t;
using typeToVerify = uint64_t;

using typeToTestSigned = make_signed<typeToTest>::type;
using typeToVerifySigned = make_signed<typeToVerify>::type;

template <class Integer>
Integer powerTest(const Integer& base, const Integer& exponent) {
  return static_cast<Integer>(powl(base, exponent));
}

enum class OperatorType { DIV = 0, EXP, OTHER };

// WiderInteger should have greater space than integer
// Also both should be unsigned or both should be signed
template <class Integer, class WiderInteger>
void test_function_exhaustive(
    const function<bool(const Integer&, const Integer&, Integer&)>
        safemath_operator,
    const function<WiderInteger(const WiderInteger&, const WiderInteger&)>
        generic_operator,
    const OperatorType op = OperatorType::OTHER) {
  constexpr WiderInteger maxValue = numeric_limits<Integer>::max();
  constexpr WiderInteger minValue = numeric_limits<Integer>::min();
  LOG_GENERAL(INFO, "Min: " << minValue << " Max: " << maxValue);
  for (WiderInteger i = minValue; i <= maxValue; ++i) {
    for (WiderInteger j = minValue; j <= maxValue; ++j) {
      Integer res{};
      WiderInteger actual_result{};
      if ((op == OperatorType::DIV && j == 0) ||
          (op == OperatorType::EXP && j < 0)) {
        continue;
      }
      actual_result = generic_operator(i, j);
      const bool& success = safemath_operator(i, j, res);
      BOOST_CHECK_MESSAGE(!(success && res != actual_result),
                          "SafeMath wrong " << res << " " << actual_result);
      if (!success) {
        // In case of failure, log the values
        if (actual_result <= maxValue && actual_result >= minValue) {
          LOG_GENERAL(INFO, "Result calculated otherwise: "
                                << actual_result << " operators: " << i << " "
                                << j);
        }
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE(safemath_exhaustive)

BOOST_AUTO_TEST_CASE(test_uint8_addition) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTest, typeToVerify>(SafeMath<typeToTest>::add,
                                                     plus<typeToVerify>());
}

BOOST_AUTO_TEST_CASE(test_uint8_subtraction) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTest, typeToVerify>(SafeMath<typeToTest>::sub,
                                                     minus<typeToVerify>());
}

BOOST_AUTO_TEST_CASE(test_int8_addition) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTestSigned, typeToVerifySigned>(
      SafeMath<typeToTestSigned>::add, plus<typeToVerifySigned>());
}

BOOST_AUTO_TEST_CASE(test_int8_subtraction) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTestSigned, typeToVerifySigned>(
      SafeMath<typeToTestSigned>::sub, minus<typeToVerifySigned>());
}

BOOST_AUTO_TEST_CASE(test_uint8_mul) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTest, typeToVerify>(
      SafeMath<typeToTest>::mul, multiplies<typeToVerify>());
}

BOOST_AUTO_TEST_CASE(test_int8_mul) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTestSigned, typeToVerifySigned>(
      SafeMath<typeToTestSigned>::mul, multiplies<typeToVerifySigned>());
}

BOOST_AUTO_TEST_CASE(test_uint8_div) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTest, typeToVerify>(
      SafeMath<typeToTest>::div, divides<typeToVerify>(), OperatorType::DIV);
}

BOOST_AUTO_TEST_CASE(test_int8_div) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTestSigned, typeToVerifySigned>(
      SafeMath<typeToTestSigned>::div, divides<typeToVerifySigned>(),
      OperatorType::DIV);
}

BOOST_AUTO_TEST_CASE(test_uint8_pow) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTest, typeToVerify>(
      SafeMath<typeToTest>::power_core, powerTest<typeToVerify>,
      OperatorType::EXP);
}

BOOST_AUTO_TEST_CASE(test_int8_pow) {
  INIT_STDOUT_LOGGER();
  test_function_exhaustive<typeToTestSigned, typeToVerifySigned>(
      SafeMath<typeToTestSigned>::power_core, powerTest<typeToVerifySigned>,
      OperatorType::EXP);
}

BOOST_AUTO_TEST_SUITE_END()
