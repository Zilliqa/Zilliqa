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

#include "SanityChecks.h"

using namespace std;

bool IsMessageSizeInappropriate(unsigned int messageSize, unsigned int offset,
                                unsigned int minLengthNeeded,
                                unsigned int factor, string errMsg) {
  if (minLengthNeeded > messageSize - offset) {
    LOG_GENERAL(WARNING, "[Message Size Insufficient] " << errMsg);
    return true;
  }

  if (factor != 0 && (messageSize - offset - minLengthNeeded) % factor != 0) {
    LOG_GENERAL(WARNING,
                "[Message Size not a proper multiple of factor] " << errMsg);
    return true;
  }

  return false;
}
