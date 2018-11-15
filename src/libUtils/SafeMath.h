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

#ifndef __SafeMath_H__
#define __SafeMath_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include "Logger.h"

template <class T>
class SafeMath {
 public:
  static bool mul(const T& a, const T& b, T& result) {
    if (a == 0 || b == 0) {
      result = 0;
      return true;
    }

    T c = a * b;
    if (c / a != b) {
      LOG_GENERAL(WARNING, "Multiplication Underflow/Overflow!");
      return false;
    }
    result = c;
    return true;
  }

  static bool power(const T& base, const T& exponent, T& result) {
    if (exponent == 0) {
      result = 1;
      return true;
    }

    if (exponent < 0) {
      LOG_GENERAL(WARNING, "Doesn't support pow with negative index");
      return false;
    }

    T temp = base, count = exponent;

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
  static T power(const T& base, const T& exponent, bool isCritical = false) {
    if (exponent == 0) {
      return 1;
    }

    if (exponent < 0) {
      LOG_GENERAL(WARNING, "Doesn't support pow with negative index");
      return 0;
    }

    T ret = base, count = exponent;

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

  static bool div(const T& a, const T& b, T& result) {
    if (b == 0) {
      LOG_GENERAL(WARNING, "Denominator cannot be zero!");
      return false;
    }

    T c = a / b;
    if (a != b * c + a % b) {
      return false;
    }

    result = c;
    return true;
  }

  static bool sub(const T& a, const T& b, T& result) {
    if (a == b) {
      result = 0;
      return true;
    }

    T aa = a, bb = b;
    bool bPos = true;

    if (a < b) {
      bPos = false;
      aa = b;
      bb = a;
    }

    if (aa == 0) {
      result = bPos ? (0 - bb) : bb;
      return true;
    }

    if (bb == 0) {
      result = bPos ? aa : (0 - aa);
      return true;
    }

    T c = aa - bb;

    if (aa > 0 && bb < 0 && (c < aa || c < (0 - bb))) {
      if (bPos) {
        LOG_GENERAL(WARNING, "Subtraction Overflow!");
      } else {
        LOG_GENERAL(WARNING, "Subtraction Underflow!");
      }
      return false;
    }

    result = bPos ? c : (0 - c);
    return true;
  }

  static bool add(const T& a, const T& b, T& result) {
    T c = a + b;

    if (a > 0 && b > 0 && (c < a || c < b)) {
      LOG_GENERAL(WARNING, "Addition Overflow!");
      return false;
    } else if (a < 0 && b < 0 && (c > a || c > b)) {
      LOG_GENERAL(WARNING, "Addition Underflow!");
      return false;
    }

    result = c;
    return true;
  }
};

#endif  //__SafeMath_H__
