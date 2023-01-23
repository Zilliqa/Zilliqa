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

#include "Address.h"
#include "Bech32/segwit_addr.h"
#include "libUtils/DataConversion.h"

using namespace std;

namespace {
const unsigned int HEX_ADDR_SIZE = ACC_ADDR_SIZE * 2;

inline bool HasZilHrp(const string& input) {
  return input.substr(0, 4) == string("zil1");
}

AddressConversionCode ToAddressStructure(const string& input_addr,
                                         Address& retAddr) {
  string addr;
  if (input_addr.size() >= 2 && input_addr[0] == '0' && input_addr[1] == 'x') {
    addr = input_addr.substr(2);
  } else {
    addr = input_addr;
  }
  if (addr.size() != HEX_ADDR_SIZE) {
    return AddressConversionCode::WRONG_ADDR_SIZE;
  }

  zbytes tmpaddr;
  if (!DataConversion::HexStrToUint8Vec(addr, tmpaddr)) {
    return AddressConversionCode::INVALID_ADDR;
  }

  retAddr = Address{tmpaddr};
  return AddressConversionCode::OK;
}

}  // namespace

AddressConversionCode ToBase16Addr(const string& addr, Address& retAddr) {
  // Accept both bech32 or base16 string, and convert to our structure

  if (HasZilHrp(addr)) {
    zbytes tmpaddr(ACC_ADDR_SIZE);
    size_t tmpaddrSize = 0;

    if (bech32_addr_decode(tmpaddr.data(), &tmpaddrSize, "zil", addr.c_str()) ==
        0) {
      return AddressConversionCode::INVALID_BECH32_ADDR;
    }

    retAddr = Address{tmpaddr};
    return AddressConversionCode::OK;
  }

  return ToAddressStructure(addr, retAddr);
}
