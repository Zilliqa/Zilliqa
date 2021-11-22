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

#ifndef ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_

#include <string>
#include <unordered_map>
#include <vector>
#include "common/BaseType.h"

namespace DNSUtils {
enum class DNSListType : int {
  UPPER_SEED = 0,
  L2L_DATA_PROVIDERS,
  MULTIPLIERS
};

using IPPubkeyMap = std::unordered_map<uint128_t, bytes>;

void InitDNSCacheList();

void ShutDownDNSCacheList();

void AttemptPopulateLookupsDNSCache();

std::string GetPubKeyUrl(const std::string& ip, const std::string& url);

bool GetDNSCacheList(IPPubkeyMap& mapOfIPPubkeyFromDNS, DNSListType listType);

}  // namespace DNSUtils

#endif  // ZILLIQA_SRC_LIBUTILS_DNSUTILS_H_
