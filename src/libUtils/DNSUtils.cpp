/*
 * Copyright (C) 2022 Zilliqa
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

#include "DNSUtils.h"
#include "DataConversion.h"
#include "Logger.h"

#include "common/Constants.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/IPConverter.h"

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <condition_variable>

using namespace std;

namespace DNSUtils {

atomic<bool> isShuttingDown;

using ListOfIPFromDNS = vector<string>;

struct DNSCacheList {
  mutex dataAccessMutex;

  ListOfIPFromDNS listOfIPFromDNS;
  IPPubkeyMap listOfPubKeys;
};

unordered_map<DNSListType, string> addressesOfDNS;

// End of DS consensus, a separate thread will be spawned to query the DNS list
// This is to avoid delay on the node's SendMessages when DNS query is a
// blocking call. At the next DS consensus, we will update our local list to the
// last updated cache And the above starts over again
unordered_map<DNSListType, DNSCacheList> cacheDataMapOfDNS;

bool QueryIPStrListFromDNS(DNSListType listType, const string &url) {
  LOG_MARKER();

  struct addrinfo hints {
  }, *res, *result;
  int errcode = 0;
  char addrstr[100];
  void *ptr = nullptr;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  for (unsigned int retry = 0; retry < QUERY_DNS_MAX_TRIES; ++retry) {
    if (isShuttingDown) break;

    errcode = getaddrinfo(url.c_str(), NULL, &hints, &result);
    if (errcode != 0) {
      LOG_GENERAL(WARNING, "Failed to query from "
                               << url << " Err: " << errcode
                               << ", Msg: " << gai_strerror(errcode)
                               << ", retry: " << retry);
      continue;
    }
    break;  // If we are here, means success
  }

  if (isShuttingDown) return false;

  if (errcode != 0) {
    LOG_GENERAL(WARNING, "Failed to query from "
                             << url << ", Err: " << gai_strerror(errcode));
    return false;
  }

  res = result;

  auto &listOfIPFromDNS = cacheDataMapOfDNS[listType].listOfIPFromDNS;
  listOfIPFromDNS.clear();

  while (res) {
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
      LOG_GENERAL(DEBUG, "Address: " << addrstr);
      listOfIPFromDNS.emplace_back(addrstr);
    }
    res = res->ai_next;
    ptr = nullptr;
  }

  freeaddrinfo(result);

  return true;
}

// Pub key are stored in the TXT record
bool QueryPubkeyFromUrl(bytes &output, const string &pubKeyUrl) {
  LOG_MARKER();

  unsigned char query_buffer[256];
  int resLen = 0;

  for (unsigned int retry = 0; retry < QUERY_DNS_MAX_TRIES; ++retry) {
    if (isShuttingDown) return false;

    resLen = res_query(pubKeyUrl.c_str(), C_IN, ns_t_txt, query_buffer,
                       sizeof(query_buffer));
    if (resLen < 0) {
      LOG_GENERAL(WARNING, "Failed to query from pubKey from "
                               << pubKeyUrl << " Retry: " << retry);
      continue;
    }
    break;  // If we are here, means success
  }

  if (isShuttingDown) return false;

  if (resLen < 0) {
    LOG_GENERAL(WARNING, "Failed to query from pubKey from "
                             << pubKeyUrl << " Res: " << resLen);
    return false;
  }

  ns_msg nsMsg;
  ns_initparse(query_buffer, resLen, &nsMsg);

  if (ns_msg_count(nsMsg, ns_s_an) <= 0) {
    LOG_GENERAL(WARNING, "No data found from pubKey from " << pubKeyUrl);
    return false;
  }

  ns_rr rr;
  ns_parserr(&nsMsg, ns_s_an, 0, &rr);
  auto rrData = ns_rr_rdata(rr) + 1;

  string p(rrData, rrData + rr.rdlength - 1);

  if (p.compare("0") == 0 || p.empty()) {
    LOG_GENERAL(WARNING, "Returned pubKey is 0 or empty: " << pubKeyUrl);
    return false;
  }

  if (!DataConversion::HexStrToUint8Vec(p, output)) {
    LOG_GENERAL(WARNING, "Invalid data obtained from pubKey " << pubKeyUrl);
    return false;
  }
  return true;
}

void QueryDNSList(DNSListType listType) {
  LOG_MARKER();
  if (isShuttingDown) return;

  const auto &url = addressesOfDNS[listType];
  if (url.empty()) {
    LOG_GENERAL(INFO, "DNS address is empty for type " << (int)listType);
    return;
  }

  auto &dataListCache = cacheDataMapOfDNS[listType];

  if (!dataListCache.dataAccessMutex.try_lock()) {
    LOG_GENERAL(INFO,
                "Another thread is querying " << addressesOfDNS[listType]);
    return;
  }

  if (!QueryIPStrListFromDNS(listType, url)) {
    LOG_GENERAL(WARNING, "Failed to obtain IP list from "
                             << addressesOfDNS[listType]
                             << ", try again on another DS epoch");
    dataListCache.dataAccessMutex.unlock();
    return;
  }

  vector<uint128_t> currentIPKeys;
  currentIPKeys.reserve(dataListCache.listOfIPFromDNS.size());

  auto &listOfPubKeys = dataListCache.listOfPubKeys;

  // Adding new pubKeys to our dns cache
  for (const auto &ipStr : dataListCache.listOfIPFromDNS) {
    uint128_t ipKey;
    if (!IPConverter::ToNumericalIPFromStr(ipStr, ipKey)) {
      LOG_GENERAL(WARNING, "Unable to change IP to ipKey: " << ipKey);
      continue;
    }

    LOG_GENERAL(INFO, "IP Str: " << ipStr << ", IPKey: " << ipKey);

    currentIPKeys.emplace_back(ipKey);

    if (listOfPubKeys.find(ipKey) != listOfPubKeys.end()) {
      // Already exists, don't need to query again, unlikely to change pubKey
      // for an ip
      continue;
    }

    auto pubKeyUrl = GetPubKeyUrl(ipStr, url);
    bytes pubKeyResult;
    if (QueryPubkeyFromUrl(pubKeyResult, pubKeyUrl)) {
      listOfPubKeys[ipKey] = pubKeyResult;
    }
  }

  // Remove pubKeys that are no longer in the dns list
  for (auto itr = listOfPubKeys.begin(); itr != listOfPubKeys.end();) {
    if (find(currentIPKeys.begin(), currentIPKeys.end(), itr->first) ==
        currentIPKeys.end()) {
      itr = listOfPubKeys.erase(itr);
    } else {
      ++itr;
    }
  }

  dataListCache.dataAccessMutex.unlock();
}

void InitDNSCacheList() {
  cacheDataMapOfDNS[DNSListType::UPPER_SEED];
  cacheDataMapOfDNS[DNSListType::L2L_DATA_PROVIDERS];
  cacheDataMapOfDNS[DNSListType::MULTIPLIERS];
  cacheDataMapOfDNS[DNSListType::LOOKUPS];

  addressesOfDNS[DNSListType::UPPER_SEED] = UPPER_SEED_DNS;
  addressesOfDNS[DNSListType::L2L_DATA_PROVIDERS] = L2L_DATA_PROVIDERS_DNS;
  addressesOfDNS[DNSListType::MULTIPLIERS] = MULTIPLIER_DNS;
  addressesOfDNS[DNSListType::LOOKUPS] = LOOKUP_DNS;

  isShuttingDown = false;
}

void ShutDownDNSCacheList() { isShuttingDown = true; }

void AttemptPopulateLookupsDNSCache() {
  LOG_MARKER();
  DetachedFunction(1, QueryDNSList, DNSListType::UPPER_SEED);
  DetachedFunction(1, QueryDNSList, DNSListType::L2L_DATA_PROVIDERS);
  DetachedFunction(1, QueryDNSList, DNSListType::MULTIPLIERS);
  DetachedFunction(1, QueryDNSList, DNSListType::LOOKUPS);
}

string GetPubKeyUrl(const std::string &ip, const std::string &url) {
  /*
    URL = "zilliqa-seedpubs.dev.z7a.xyz"
    IP = "54.148.35.87"
    Pubkey URL= "pub-54-148-35-87.dev.z7a.xyz"
  */

  auto tmpIp = ip;
  std::replace(tmpIp.begin(), tmpIp.end(), '.', '-');
  return "pub-" + tmpIp + url.substr(url.find_first_of('.'));
}

bool GetDNSCacheList(IPPubkeyMap &mapOfIPPubkeyFromDNS, DNSListType listType) {
  auto &cacheList = cacheDataMapOfDNS[listType];
  if (!cacheList.dataAccessMutex.try_lock()) {
    LOG_GENERAL(INFO, "Unable to obtain data from "
                          << addressesOfDNS[listType]
                          << ", data are still being queried");
    return false;
  }

  auto isEmptyCache = cacheList.listOfPubKeys.empty();

  if (isEmptyCache) {
    LOG_GENERAL(INFO, "DNS cache is empty for " << addressesOfDNS[listType]);
  } else {
    mapOfIPPubkeyFromDNS = cacheList.listOfPubKeys;
  }

  cacheList.dataAccessMutex.unlock();
  return !isEmptyCache;
}
}  // namespace DNSUtils
