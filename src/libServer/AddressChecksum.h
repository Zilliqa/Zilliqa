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
#ifndef __ADDRESS_CHECKSUM_H__
#define __ADDRESS_CHECKSUM_H__

#include <boost/algorithm/string.hpp>

#include "common/Constants.h"
#include "libUtils/DataConversion.h"
#include "libUtils/HashUtils.h"

class AddressChecksum {
 public:
  static const std::string GetCheckSumedAddress(std::string origAddress) {
    if (!(origAddress.size() != ACC_ADDR_SIZE * 2 + 2) &&
        !(origAddress.size() != ACC_ADDR_SIZE * 2)) {
      LOG_GENERAL(WARNING, "Size inappropriate");
      return "";
    }

    if (origAddress.size() == ACC_ADDR_SIZE * 2 + 2) {
      if (origAddress.substr(0, 2) != "0x") {
        return "";
      }

      origAddress = origAddress.erase(0, 2);
    }

    std::string lower_case_address = boost::to_lower_copy(origAddress);

    bytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(lower_case_address, tmpaddr)) {
      LOG_GENERAL(WARNING, "DataConversion::HexStrToUint8Vec Failed");
      return "";
    }
    bytes hash_s = HashUtils::BytesToHash(tmpaddr);

    uint256_t temp_1 = 1;
    std::string ret = "";

    std::string hash_str;
    if (!DataConversion::Uint8VecToHexStr(hash_s, hash_str)) {
      LOG_GENERAL(WARNING, "DataConversion::Uint8VecToHexStr Failed");
      return "";
    }

    uint256_t v("0x" + hash_str);

    for (uint i = 0; i < lower_case_address.size(); i++) {
      if (lower_case_address.at(i) >= '0' && lower_case_address.at(i) <= '9') {
        ret += lower_case_address.at(i);
      } else {
        if ((v & (temp_1 << 255 - 6 * i))) {
          ret += toupper(lower_case_address.at(i));
        } else {
          ret += lower_case_address.at(i);
        }
      }
    }

    return ret;
  }

  // lower_case_address is empty if fail checksum
  static bool VerifyChecksumAddress(std::string address,
                                    std::string& lower_case_address) {
    if (!(address.size() != ACC_ADDR_SIZE * 2 + 2) &&
        !(address.size() != ACC_ADDR_SIZE * 2)) {
      LOG_GENERAL(WARNING, "Size inappropriate");
      lower_case_address = "";
      return false;
    }

    if (address.size() == ACC_ADDR_SIZE * 2 + 2) {
      if (address.substr(0, 2) != "0x") {
        lower_case_address = "";
        return false;
      }

      address = address.erase(0, 2);
    }

    const auto& toCompare = GetCheckSumedAddress(address);
    if (toCompare != address) {
      lower_case_address = "";
      return false;
    } else {
      lower_case_address = boost::to_lower_copy(address);
      return true;
    }
  }
};

#endif  //__ADDRESS_CHECKSUM_H__
