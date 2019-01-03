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

#include "IPConverter.h"
#include <arpa/inet.h>

const std::string IPConverter::ToStrFromNumericalIP(
    const boost::multiprecision::uint128_t& ip) {
  char str[INET_ADDRSTRLEN];
  struct sockaddr_in serv_addr;
  serv_addr.sin_addr.s_addr = ip.convert_to<unsigned long>();
  inet_ntop(AF_INET, &(serv_addr.sin_addr), str, INET_ADDRSTRLEN);
  return std::string(str);
}

const boost::multiprecision::uint128_t IPConverter::ToNumericalIPFromStr(
    const std::string& ipStr) {
  struct in_addr ip_addr;
  inet_pton(AF_INET, ipStr.c_str(), &ip_addr);
  return (boost::multiprecision::uint128_t)ip_addr.s_addr;
}
