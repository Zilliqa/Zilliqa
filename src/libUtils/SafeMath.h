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

#ifndef __SAFEMATH_H__
#define __SAFEMATH_H__

template <class T>
class SafeMath {
 public:
  static bool add(const T& a, const T& b, T& result);
  static bool sub(const T& a, const T& b, T& result);
  static bool mul(const T& a, const T& b, T& result);
  static bool div(const T& a, const T& b, T& result);
  static bool power_core(const T& base, const T& exponent, T& result);
  static T power(const T& base, const T& exponent, bool isCritical = false);

 private:
  static bool IsSignedInt(const T& a);
  static bool IsUnsignedInt(const T& a);
  static bool add_signint(const T& a, const T& b, T& result);
  static bool add_unsignint(const T& a, const T& b, T& result);
  static bool sub_signint(const T& a, const T& b, T& result);
  static bool sub_unsignint(const T& a, const T& b, T& result);
  static bool mul_signint(const T& a, const T& b, T& result);
  static bool mul_unsignint(const T& a, const T& b, T& result);
  static bool div_signint(const T& a, const T& b, T& result);
  static bool div_unsignint(const T& a, const T& b, T& result);
};

#include "SafeMath.tpp"

#endif  //__SAFEMATH_H__
