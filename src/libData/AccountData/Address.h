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

#ifndef __ADDRESS_H__
#define __ADDRESS_H__

#include <array>
#include <unordered_set>
#include <vector>

#include "common/Constants.h"
#include "depends/common/FixedHash.h"

using Address = dev::h160;  // earlier it was std::array<unsigned char,
                            // ACC_ADDR_SIZE>; ACC_ADDR_SIZE = 20
using Addresses = std::vector<Address>;
using AddressHashSet = std::unordered_set<Address>;

const Address NullAddress;

#endif  // __ADDRESS_H__
