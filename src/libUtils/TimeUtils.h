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

#ifndef ZILLIQA_SRC_LIBUTILS_TIMEUTILS_H_
#define ZILLIQA_SRC_LIBUTILS_TIMEUTILS_H_

#include <chrono>
#include <string>

std::chrono::system_clock::time_point r_timer_start();
double r_timer_end(std::chrono::system_clock::time_point start_time);

inline uint64_t get_time_as_int() {
  std::chrono::microseconds microsecs =
      duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch());
  return static_cast<uint64_t>(microsecs.count());
}

std::string microsec_timestamp_to_readable(const uint64_t timestamp);
long int microsec_to_sec(const uint64_t timestamp);
bool is_timestamp_in_range(const uint64_t timestamp, const uint64_t loBound,
                           const uint64_t hiBound);

#endif  // ZILLIQA_SRC_LIBUTILS_TIMEUTILS_H_
