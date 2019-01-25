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

#ifndef __TIMESTAMPVERIFIER_H__
#define __TIMESTAMPVERIFIER_H__

#include "Logger.h"
#include "TimeUtils.h"
#include "common/Constants.h"

static bool VerifyTimestamp(const uint64_t& timestamp_in_microsec,
                            const uint64_t& timeout_in_sec) {
  uint64_t loBound, hiBound;

  loBound =
      get_time_as_int() >=
              (SYS_TIMESTAMP_VARIANCE_IN_SECONDS + timeout_in_sec) * 1000000
          ? get_time_as_int() -
                (SYS_TIMESTAMP_VARIANCE_IN_SECONDS + CONSENSUS_OBJECT_TIMEOUT) *
                    1000000
          : 0;

  if (!SafeMath<uint64_t>::add(get_time_as_int(),
                               (SYS_TIMESTAMP_VARIANCE_IN_SECONDS)*1000000,
                               hiBound)) {
    hiBound = std::numeric_limits<uint64_t>::max();
  }

  if (!is_timestamp_in_range(timestamp_in_microsec, loBound, hiBound)) {
    LOG_CHECK_FAIL("Timestamp",
                   timestamp_in_microsec
                       << "("
                       << microsec_timestamp_to_readable(timestamp_in_microsec)
                       << ")",
                   loBound << "(" << microsec_timestamp_to_readable(loBound)
                           << ") ~ " << hiBound << "("
                           << microsec_timestamp_to_readable(hiBound) << ")");
    return false;
  }

  return true;
}

#endif  // __TIMESTAMPVERIFIER_H__
