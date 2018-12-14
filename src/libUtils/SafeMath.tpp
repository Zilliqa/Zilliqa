/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
bool SafeMath<T>::power(const T& base, const T& exponent, T& result) {
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
  if (exponent == 0) {
    return 1;
  }

  if (exponent < 0) {
    LOG_GENERAL(WARNING, "Doesn't support pow with negative index");
    return 0;
  }

  T ret = base, count = exponent - 1;

  while (count > 0) {
    if (!SafeMath::mul(ret, base, ret)) {
      LOG_GENERAL(isCritical ? FATAL : WARNING,
                  "SafeMath::pow failed ret: " << ret << " base " << base);
      return ret;
    }
    --count;
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
         typeid(a) == typeid(boost::multiprecision::uint128_t) ||
         typeid(a) == typeid(boost::multiprecision::uint256_t) ||
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
  if (a > 0 && b < a - std::numeric_limits<T>::max()) {
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

  if (a != b * c + a % b) {
    return false;
  }

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
