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

#include "BitVector.h"

using namespace std;

unsigned int BitVector::GetBitVectorLengthInBytes(unsigned int length_in_bits) {
  return (((length_in_bits & 0x07) > 0) ? (length_in_bits >> 3) + 1
                                        : length_in_bits >> 3);
}

unsigned int BitVector::GetBitVectorSerializedSize(
    unsigned int length_in_bits) {
  return 2 + GetBitVectorLengthInBytes(length_in_bits);
}

std::vector<bool> BitVector::GetBitVector(const bytes& src, unsigned int offset,
                                          unsigned int expected_length) {
  std::vector<bool> result;
  unsigned int actual_length = 0;
  unsigned int actual_length_bytes = 0;

  if ((src.size() - offset) >= 2) {
    actual_length = (src.at(offset) << 8) + src.at(offset + 1);
    actual_length_bytes = GetBitVectorLengthInBytes(actual_length);
  }

  if ((actual_length_bytes == expected_length) &&
      ((src.size() - offset - 2) >= actual_length_bytes)) {
    result.reserve(actual_length);
    for (unsigned int index = 0; index < actual_length; index++) {
      result.push_back(src.at(offset + 2 + (index >> 3)) &
                       (1 << (7 - (index & 0x07))));
    }
  }

  return result;
}

std::vector<bool> BitVector::GetBitVector(const bytes& src,
                                          unsigned int offset) {
  std::vector<bool> result;
  unsigned int actual_length = 0;
  unsigned int actual_length_bytes = 0;

  if ((src.size() - offset) >= 2) {
    actual_length = (src.at(offset) << 8) + src.at(offset + 1);
    actual_length_bytes = GetBitVectorLengthInBytes(actual_length);
  }

  if ((src.size() - offset - 2) >= actual_length_bytes) {
    result.reserve(actual_length);
    for (unsigned int index = 0; index < actual_length; index++) {
      result.push_back(src.at(offset + 2 + (index >> 3)) &
                       (1 << (7 - (index & 0x07))));
    }
  }

  return result;
}

unsigned int BitVector::SetBitVector(bytes& dst, unsigned int offset,
                                     const std::vector<bool>& value) {
  const unsigned int length_needed = GetBitVectorSerializedSize(value.size());

  if ((offset + length_needed) > dst.size()) {
    dst.resize(offset + length_needed);
  }
  fill(dst.begin() + offset, dst.begin() + offset + length_needed, 0x00);

  dst.at(offset) = value.size() >> 8;
  dst.at(offset + 1) = value.size();

  unsigned int index = 0;
  for (bool b : value) {
    if (b) {
      dst.at(offset + 2 + (index >> 3)) |= (1 << (7 - (index & 0x07)));
    }
    index++;
  }

  return length_needed;
}
