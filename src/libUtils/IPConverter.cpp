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
#include <boost/algorithm/string.hpp>

using namespace std;

namespace IPConverter {

bool GetIPPortFromSocket(string socket, string& ip, int& port) {
  std::vector<std::string> addr_parts;
  boost::algorithm::split(addr_parts, socket, boost::algorithm::is_any_of(":"));
  // Check IPv6
  if (socket[0] == '[') {
    // Strip first char '['
    addr_parts.front() =
        addr_parts.front().substr(1, addr_parts.front().length());
    // Reconstructs IP
    ip = addr_parts.front();
    std::for_each(addr_parts.begin() + 1, addr_parts.end() - 1,
                  [&](const std::string& piece) { ip += (":" + piece); });
    // Strip last char ']'
    if (ip[ip.length() - 1] == ']') {
      ip = ip.substr(0, ip.length() - 1);
    } else {
      return false;
    }

  } else if (addr_parts.size() == 2) {
    ip = addr_parts[0];
  } else {
    return false;
  }

  try {
    port = boost::lexical_cast<int>(addr_parts.back());
    return true;
  } catch (boost::bad_lexical_cast&) {
    return false;
  }
  // Defense
  return false;
}

const std::string ToStrFromNumericalIP(const uint128_t& ip) {
  char str[INET_ADDRSTRLEN];
  struct sockaddr_in serv_addr;
  serv_addr.sin_addr.s_addr = ip.convert_to<unsigned long>();
  inet_ntop(AF_INET, &(serv_addr.sin_addr), str, INET_ADDRSTRLEN);
  return std::string(str);
}

void LogUnsupported(const string& ip) {
  SWInfo::LogBrandBugReport();
  cerr << "Error: Unknown address type " << ip << ", unsupported protocol\n"
       << endl;
}

void LogInvalidIP(const string& ip) {
  SWInfo::LogBrandBugReport();
  cerr << "Error: address " << ip
       << " does not contain a character string "
          "representing a valid network address\n"
       << endl;
}

void LogInternalErr(const string& ip) {
  SWInfo::LogBrandBugReport();
  cerr << "Internal Error: cannot process the input IP address " << ip << ".\n"
       << std::endl;
}

bool ToNumericalIPFromStr(const std::string& ipStr, uint128_t& ipInt) {
  boost::asio::ip::address Addr;
  try {
    Addr = boost::asio::ip::address::from_string(ipStr);
  } catch (const std::exception& e) {
    LogInvalidIP(ipStr);
    return false;
  }
  ipInt = 0;
  if (Addr.is_v4()) {
    ipInt = convertBytesToInt(Addr.to_v4());
    return true;
  } else if (Addr.is_v6()) {
    ipInt = convertBytesToInt(Addr.to_v6());
    return true;
  }

  LogUnsupported(ipStr);
  return false;
}

bool ResolveDNS(const std::string& url, const uint32_t& port,
                uint128_t& ipInt) {
  try {
    boost::asio::io_service my_io_service;
    boost::asio::ip::tcp::resolver resolver(my_io_service);
    boost::asio::ip::tcp::resolver::query query(
        url, boost::lexical_cast<std::string>(port));
    boost::asio::ip::tcp::resolver::iterator iter = resolver.resolve(query);
    boost::asio::ip::tcp::resolver::iterator end;  // End marker.
    while (iter != end) {
      boost::asio::ip::tcp::endpoint endpoint = *iter++;
      if (endpoint.address().is_v4()) {
        return ToNumericalIPFromStr(endpoint.address().to_string(), ipInt);
      }
    }
  } catch (std::exception& e) {
    return false;
  }

  return false;
}

}  // namespace IPConverter
