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

#ifndef ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_

#include <string>
#include <vector>
#include "common/BaseType.h"

enum class DnsListType : int {
  UPPER_SEED = 0,
  L2L_DATA_PROVIDERS,
  MULTIPLIERS
};

void InitDnsCacheList();

void ShutDownDnsCacheList();

void AttemptPopulateLookupsDnsCache();

void AttemptPopulateLookupsDnsCacheImmediately(DnsListType listType);

std::string GetPubKeyUrl(const std::string& ip, const std::string& url);

uint128_t ConvertIpStringToUint128(const std::string& ipStr);

bool GetIpStrListFromDnsCache(std::vector<std::string>& ipStrList,
                              DnsListType listType);

bool GetPubKeyFromDnsCache(bytes& output, const std::string& ip,
                           DnsListType listType);

#endif  // ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_
