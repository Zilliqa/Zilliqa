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

#ifndef __BITVECTOR_H__
#define __BITVECTOR_H__

#include "common/BaseType.h"

class BitVector {
 public:
  static unsigned int GetBitVectorLengthInBytes(unsigned int length_in_bits);
  static unsigned int GetBitVectorSerializedSize(unsigned int length_in_bits);
  static std::vector<bool> GetBitVector(const bytes& src, unsigned int offset,
                                        unsigned int expected_length);
  static std::vector<bool> GetBitVector(const bytes& src, unsigned int offset);
  static unsigned int SetBitVector(bytes& dst, unsigned int offset,
                                   const std::vector<bool>& value);
};

#endif  // __BITVECTOR_H__
