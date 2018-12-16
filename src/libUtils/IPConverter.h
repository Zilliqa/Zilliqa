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

#ifndef __IP_CONVERTER_H__
#define __IP_CONVERTER_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <string>
#include <boost/asio.hpp>
#include <arpa/inet.h>

/// Utility class for converter from ip address string to numerical
/// represetation.
class IPConverter {
private:
  static void LogInvalidIP();
  void LogBrand();
  void LogBugReport();
  void LogUnsupported();
  void LogInternalErr();
 public:
  enum IPv {IPv4, IPv6};
  static const std::string ToStrFromNumericalIP(
      const boost::multiprecision::uint128_t& ip);

  template <typename ip_s>
  bool convertIP(const char* in, ip_s& ip_addr, const IPv v) {
    int res;
    if (v == IPv4) {
      res = inet_pton(AF_INET, in, &ip_addr);
    }
    else {
      res = inet_pton(AF_INET6, in, &ip_addr);
    }

    if (res == 1) {
      return true;
    } else if (res == 0) {
      LogInvalidIP();
      return false;
    } else {
      LogBrand();
      LogBugReport();
      LogInternalErr();
      return false;
    }
  }

  static int ToNumericalIPFromStr(
      const std::string& , boost::multiprecision::uint128_t& );
};

#endif  // __IP_CONVERTER_H__
