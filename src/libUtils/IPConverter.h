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

#include "common/BaseType.h"

#include <arpa/inet.h>
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <string>
#include "libUtils/SWInfo.h"

/// Utility class for converter from ip address string to numerical
/// represetation.

namespace IPConverter {

enum IPv { IPv4, IPv6 };

const std::string ToStrFromNumericalIP(const uint128_t&);

bool GetIPPortFromSocket(std::string, std::string&, int&);

void LogUnsupported(const std::string&);

void LogInvalidIP(const std::string&);

void LogInternalErr(const std::string&);

template <class ip_t>
uint128_t convertBytesToInt(ip_t ip_v) {
  uint8_t i = 0;
  uint128_t ipInt = 0;
  for (const unsigned char b : ip_v.to_bytes()) {
    ipInt = ipInt | (uint128_t)b << i * 8;
    i++;
  }
  return ipInt;
}

template <typename ip_s>
bool convertIP(const char* in, ip_s& ip_addr, const IPv v) {
  int res;
  if (v == IPv4) {
    res = inet_pton(AF_INET, in, &ip_addr);
  } else {
    res = inet_pton(AF_INET6, in, &ip_addr);
  }

  if (res == 1) {
    return true;
  } else if (res == 0) {
    LogInvalidIP(std::string(in));
    return false;
  } else {
    SWInfo::LogBrandBugReport();
    LogInternalErr(std::string(in));
    return false;
  }
}

bool ToNumericalIPFromStr(const std::string&, uint128_t&);

bool ResolveDNS(const std::string& url, const uint32_t& port, uint128_t& ipInt);
}  // namespace IPConverter

#endif  // __IP_CONVERTER_H__
