/*
 * Copyright (C) 2020 Zilliqa
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

#include "BloomFilter.h"
#include "libMessage/Messenger.h"

bool BloomFilter::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetBloomFilter(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetBloomFilter failed.");
    return false;
  }
  return true;
}

bool BloomFilter::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetBloomFilter(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}