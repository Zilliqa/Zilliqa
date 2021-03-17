/*
 * Copyright (C) 2021 Zilliqa
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

#include "DnsUtils.h"
#include "DataConversion.h"
#include "Logger.h"

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

using namespace std;

namespace {
string GetToPubKeyUrl(const string &url, const string &ip) {
  auto tmpIp = ip;
  std::replace(tmpIp.begin(), tmpIp.end(), '.', '-');
  return "pub-" + tmpIp + url.substr(url.find_first_of('.'));
}
}  // namespace

bool ObtainIpListFromDns(std::vector<uint128_t> &ipList, const std::string &url,
                         bool toClearList) {
  vector<string> ipStrList;
  if (!ObtainIpStrListFromDns(ipStrList, url)) {
    LOG_GENERAL(WARNING, "Failed to obtain ip string list");
    return false;
  }

  if (toClearList) ipList.clear();

  ipList.reserve(ipList.size() + ipStrList.size());
  std::transform(ipStrList.begin(), ipStrList.end(), std::back_inserter(ipList),
                 [](const string &s) { return ConvertIpStringToUint128(s); });
  return true;
}

bool ObtainIpStrListFromDns(vector<string> &ipStrList, const string &url,
                            bool toClearList) {
  struct addrinfo hints {
  }, *res, *result;
  int errcode;
  char addrstr[100];
  void *ptr = nullptr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  int retry = 0;
  int MAX_RETRY = 3;

  while (retry < MAX_RETRY) {
    errcode = getaddrinfo(url.c_str(), NULL, &hints, &result);

    if (errcode != 0) {
      LOG_GENERAL(WARNING, "Unable to connect "
                               << url << " Err: " << errcode
                               << ", Msg: " << gai_strerror(errcode)
                               << ", retry: " << retry);
      if (retry < MAX_RETRY) {
        ++retry;
        continue;
      } else {
        return false;
      }
    }
    // If success
    break;
  }

  res = result;

  if (toClearList) ipStrList.clear();

  while (res) {
    inet_ntop(res->ai_family, res->ai_addr->sa_data, addrstr, 100);

    switch (res->ai_family) {
      case AF_INET:
        ptr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
        break;
      case AF_INET6:
        ptr = &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr;
        break;
    }

    if (ptr) {
      inet_ntop(res->ai_family, ptr, addrstr, 100);
      auto ipType = res->ai_family == PF_INET6 ? 6 : 4;

      LOG_GENERAL(INFO, "IPv" << ipType << " address: " << addrstr);

      ipStrList.emplace_back(addrstr);
    }
    res = res->ai_next;
    ptr = nullptr;
  }

  freeaddrinfo(result);

  return true;
}

bool ObtainPubKeyFromUrl(bytes &output, const string &ip, const string &url) {
  auto pubKeyUrl = GetToPubKeyUrl(url, ip);

  unsigned char query_buffer[256];

  int resLen = 0;
  int retry = 0;
  int MAX_RETRY = 3;
  while (retry < MAX_RETRY) {
    resLen = res_query(pubKeyUrl.c_str(), C_IN, ns_t_txt, query_buffer,
                       sizeof(query_buffer));

    if (resLen < 0) {
      LOG_GENERAL(WARNING, "Failed to query pub key from "
                               << pubKeyUrl << " Retry: " << retry);
      if (retry < MAX_RETRY) {
        ++retry;
        continue;
      } else {
        return false;
      }
    }
    break;
  }

  ns_msg nsMsg;
  ns_initparse(query_buffer, resLen, &nsMsg);

  if (ns_msg_count(nsMsg, ns_s_an) <= 0) {
    LOG_GENERAL(WARNING, "No data found from pub key from " << pubKeyUrl);
    return false;
  }

  ns_rr rr;
  ns_parserr(&nsMsg, ns_s_an, 0, &rr);
  auto rrData = ns_rr_rdata(rr) + 1;

  string p(rrData, rrData + rr.rdlength - 1);

  if (!DataConversion::HexStrToUint8Vec(p, output)) {
    LOG_GENERAL(WARNING, "Invalid data obtained from pub key " << pubKeyUrl);
    return false;
  }
  return true;
}

uint128_t ConvertIpStringToUint128(const string &ipStr) {
  struct in_addr ip_addr {};
  inet_pton(AF_INET, ipStr.c_str(), &ip_addr);
  return (uint128_t)ip_addr.s_addr;
}
