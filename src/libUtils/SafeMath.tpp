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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include "Logger.h"
#include "SafeMath.h"

template <class T>
bool SafeMath<T>::add(const T& a, const T& b, T& result) {
  if (IsSignedInt(a)) {
    return add_signint(a, b, result);
  }

  if (IsUnsignedInt(a)) {
    return add_unsignint(a, b, result);
  }

  LOG_GENERAL(WARNING, "Data type " << typeid(a).name() << " not supported!");
  return false;
}

template <class T>
bool SafeMath<T>::sub(const T& a, const T& b, T& result) {
  if (IsSignedInt(a)) {
    return sub_signint(a, b, result);
  }

  if (IsUnsignedInt(a)) {
    return sub_unsignint(a, b, result);
  }

  LOG_GENERAL(WARNING, "Data type " << typeid(a).name() << " not supported!");
  return false;
}

template <class T>
bool SafeMath<T>::mul(const T& a, const T& b, T& result) {
  if (a == 0 || b == 0) {
    result = 0;
    return true;
  }

  if (IsSignedInt(a)) {
    return mul_signint(a, b, result);
  }

  if (IsUnsignedInt(a)) {
    return mul_unsignint(a, b, result);
  }

  LOG_GENERAL(WARNING, "Data type " << typeid(a).name() << " not supported!");
  return false;
}

template <class T>
bool SafeMath<T>::div(const T& a, const T& b, T& result) {
  if (b == 0) {
    LOG_GENERAL(WARNING, "Denominator cannot be zero!");
    return false;
  }

  if (IsSignedInt(a)) {
    return div_signint(a, b, result);
  }

  if (IsUnsignedInt(a)) {
    return div_unsignint(a, b, result);
  }

  LOG_GENERAL(WARNING, "Data type " << typeid(a).name() << " not supported!");
  return false;
}

template <class T>
bool SafeMath<T>::power_core(const T& base, const T& exponent, T& result) {
  if (exponent == 0) {
    result = 1;
    return true;
  }

  if (exponent < 0) {
    LOG_GENERAL(WARNING, "Doesn't support pow with negative index");
    return false;
  }

  T temp = base, count = exponent - 1;

  while (count > 0) {
    if (!SafeMath::mul(temp, base, temp)) {
      LOG_GENERAL(WARNING, "SafeMath::pow failed");
      return false;
    }
    --count;
  }

  result = temp;
  return true;
}

// if isCritical is true then will call LOG FATAL,
// Now only used for declare constant variable in Constants.cpp
template <class T>
T SafeMath<T>::power(const T& base, const T& exponent, bool isCritical) {
  T ret{};
  if (!SafeMath::power_core(base, exponent, ret)) {
    LOG_GENERAL(isCritical ? FATAL : WARNING,
                "SafeMath::power failed ret: " << ret << " base " << base);
    if (isCritical) {
      throw std::runtime_error("[Critical] SafeMath::power failed");
    }
    return ret;
  }
  return ret;
}

template <class T>
bool SafeMath<T>::IsSignedInt(const T& a) {
  return (typeid(a) == typeid(int8_t) || typeid(a) == typeid(int16_t) ||
          typeid(a) == typeid(int32_t) || typeid(a) == typeid(int64_t) ||
          typeid(a) == typeid(boost::multiprecision::int128_t) ||
          typeid(a) == typeid(boost::multiprecision::int256_t) ||
          typeid(a) == typeid(boost::multiprecision::int512_t) ||
          typeid(a) == typeid(boost::multiprecision::int1024_t));
}

template <class T>
bool SafeMath<T>::IsUnsignedInt(const T& a) {
  return typeid(a) == typeid(uint8_t) || typeid(a) == typeid(uint16_t) ||
         typeid(a) == typeid(uint32_t) || typeid(a) == typeid(uint64_t) ||
         typeid(a) == typeid(uint128_t) || typeid(a) == typeid(uint256_t) ||
         typeid(a) == typeid(boost::multiprecision::uint512_t) ||
         typeid(a) == typeid(boost::multiprecision::uint1024_t);
}

template <class T>
bool SafeMath<T>::add_signint(const T& a, const T& b, T& result) {
  if (a > 0 && b > std::numeric_limits<T>::max() - a) {
    LOG_GENERAL(WARNING, "Addition Overflow!");
    return false;
  }

  if (a < 0 && b < std::numeric_limits<T>::min() - a) {
    LOG_GENERAL(WARNING, "Addition Underflow!");
    return false;
  }

  result = a + b;
  return true;
}

template <class T>
bool SafeMath<T>::add_unsignint(const T& a, const T& b, T& result) {
  result = a + b;

  if (result < a) {
    LOG_GENERAL(WARNING, "Addition Overflow!");
    return false;
  }

  return true;
}

template <class T>
bool SafeMath<T>::sub_signint(const T& a, const T& b, T& result) {
  if (a >= 0 && b < a - std::numeric_limits<T>::max()) {
    LOG_GENERAL(WARNING, "Subtraction Overflow!");
    return false;
  }

  if (a < 0 && b > a - std::numeric_limits<T>::min()) {
    LOG_GENERAL(WARNING, "Subtraction Underflow!");
    return false;
  }

  result = a - b;
  return true;
}

template <class T>
bool SafeMath<T>::sub_unsignint(const T& a, const T& b, T& result) {
  if (b > a) {
    LOG_GENERAL(WARNING,
                "For unsigned subtraction, minuend should be greater than "
                "subtrahend!");
    return false;
  }

  result = a - b;
  return true;
}

template <class T>
bool SafeMath<T>::mul_signint(const T& a, const T& b, T& result) {
  if ((a == std::numeric_limits<T>::min() && b == (T)-1) ||
      (a == (T)-1 && b == std::numeric_limits<T>::min())) {
    LOG_GENERAL(WARNING, "Multiplication Overflow!");
    return false;
  }

  bool good;

  if (a < 0 && b < 0) {
    good = (a >= std::numeric_limits<T>::max() / b);
  } else if (a < 0 && b > 0) {
    good = (a >= std::numeric_limits<T>::min() / b);
  } else if (a > 0 && b < 0) {
    good = (b >= std::numeric_limits<T>::min() / a);
  } else {
    good = (a <= std::numeric_limits<T>::max() / b);
  }

  if (!good) {
    LOG_GENERAL(WARNING, "Multiplication Underflow/Overflow!");
    return false;
  }

  result = a * b;
  return true;
}

template <class T>
bool SafeMath<T>::mul_unsignint(const T& a, const T& b, T& result) {
  T c = a * b;

  if (c / a != b) {
    LOG_GENERAL(WARNING, "Multiplication Underflow/Overflow!");
    return false;
  }

  result = c;
  return true;
}

template <class T>
bool SafeMath<T>::div_signint(const T& a, const T& b, T& result) {
  if (a == std::numeric_limits<T>::min() && b == (T)-1) {
    LOG_GENERAL(WARNING, "Division Overflow!");
    return false;
  }

  T c = a / b;

  result = c;
  return true;
}

template <class T>
bool SafeMath<T>::div_unsignint(const T& a, const T& b, T& result) {
  T c = a / b;

  if (a != b * c + a % b) {
    return false;
  }

  result = c;
  return true;
}
