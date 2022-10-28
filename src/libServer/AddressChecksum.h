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
#ifndef ZILLIQA_SRC_LIBSERVER_ADDRESSCHECKSUM_H_
#define ZILLIQA_SRC_LIBSERVER_ADDRESSCHECKSUM_H_

#include <boost/algorithm/string.hpp>

#include <ethash/keccak.hpp>
#include "common/Constants.h"
#include "libUtils/DataConversion.h"
#include "libUtils/HashUtils.h"

class AddressChecksum {
 public:
  static const std::string GetChecksummedAddress(std::string origAddress) {
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

    zbytes tmpaddr;
    if (!DataConversion::HexStrToUint8Vec(lower_case_address, tmpaddr)) {
      LOG_GENERAL(WARNING, "DataConversion::HexStrToUint8Vec Failed");
      return "";
    }
    zbytes hash_s = HashUtils::BytesToHash(tmpaddr);

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
          ret += std::string(1, std::toupper(lower_case_address.at(i)));
        } else {
          ret += lower_case_address.at(i);
        }
      }
    }

    return ret;
  }

  // from: https://eips.ethereum.org/EIPS/eip-55
  //  convert the address to hex, but if the ith digit is a letter (ie. it’s one
  //  of abcdef) print it in uppercase if the 4*ith bit of the hash of the
  //  lowercase hexadecimal address is 1 otherwise print it in lowercase.
  // Note: the keccak is of the ** ASCII ** of the hex values
  static const std::string GetChecksummedAddressEth(std::string origAddress) {
    if (!(origAddress.size() != ACC_ADDR_SIZE * 2 + 2) &&
        !(origAddress.size() != ACC_ADDR_SIZE * 2)) {
      LOG_GENERAL(WARNING, "Size inappropriate");
      return "";
    }

    // Strip the 0x off if it is there
    if (origAddress.size() == ACC_ADDR_SIZE * 2 + 2) {
      if (origAddress.substr(0, 2) != "0x") {
        return "";
      }
      origAddress = origAddress.erase(0, 2);
    }

    // Get lower case address
    origAddress = boost::to_lower_copy(origAddress);

    // Get keccak of this lowercased address
    auto hash_of_address =
        ethash::keccak256(reinterpret_cast<const uint8_t*>(origAddress.c_str()),
                          origAddress.size());

    for (std::size_t i = 0; i < origAddress.size(); i++) {
      // If the address could be uppercased
      if (origAddress.at(i) >= 'a' && origAddress.at(i) <= 'f') {
        // i*4th bit, so 0, 4, 8...
        // i/2 selects each byte twice, then bit 0 or 4 is selected.
        char atPoint = origAddress.at(i);
        uint8_t byte_check = hash_of_address.bytes[i / 2];
        uint8_t bit_check = (i % 2) == 0 ? 0x80 : 0x08;
        uint8_t res = byte_check & bit_check;

        // Lower -> upper according to bits
        if (res) {
          origAddress[i] = std::toupper(atPoint);
        }
      }
    }

    return origAddress;
  }

  static bool VerifyChecksumAddressEth(std::string address,
                                       std::string& lower_case_address) {
    if (!(address.size() != ACC_ADDR_SIZE * 2 + 2) &&
        !(address.size() != ACC_ADDR_SIZE * 2)) {
      LOG_GENERAL(WARNING, "Size inappropriate");
      lower_case_address = "";
      return false;
    }

    if (address.size() == ACC_ADDR_SIZE * 2 + 2) {
      if (address.substr(0, 2) != "0x") {
        LOG_GENERAL(WARNING, "Checksum does not start 0x for address");
        lower_case_address = "";
        return false;
      }

      address = address.erase(0, 2);
    }

    const auto& toCompare = GetChecksummedAddressEth(address);
    if (toCompare != address) {
      LOG_GENERAL(WARNING, "Checksum does not compare correctly (eth) "
                               << toCompare << " to " << address);
      lower_case_address = "";
      return false;
    } else {
      lower_case_address = boost::to_lower_copy(address);
      return true;
    }
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
        LOG_GENERAL(WARNING, "Checksum does not start 0x for address");
        lower_case_address = "";
        return false;
      }

      address = address.erase(0, 2);
    }

    const auto& toCompare = GetChecksummedAddress(address);
    if (toCompare != address) {
      LOG_GENERAL(WARNING, "Checksum does not compare correctly (zil) "
                               << toCompare << " to " << address);
      lower_case_address = "";
      return false;
    } else {
      lower_case_address = boost::to_lower_copy(address);
      return true;
    }
  }
};

#endif  // ZILLIQA_SRC_LIBSERVER_ADDRESSCHECKSUM_H_
