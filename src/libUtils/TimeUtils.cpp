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

#include "TimeUtils.h"
#include <mutex>
#include "depends/common/CommonIO.h"

using namespace std::chrono;
using namespace boost::multiprecision;
static std::mutex gmtimeMutex;

system_clock::time_point r_timer_start() { return system_clock::now(); }

double r_timer_end(system_clock::time_point start_time) {
  duration<double, std::micro> difference = system_clock::now() - start_time;
  return difference.count();
}

uint64_t get_time_as_int() {
  microseconds microsecs =
      duration_cast<microseconds>(system_clock::now().time_since_epoch());
  return static_cast<uint64_t>(microsecs.count());
}

struct tm* gmtime_safe(const time_t* timer) {
  std::lock_guard<std::mutex> guard(gmtimeMutex);
  return gmtime(timer);
}

long int get_ms(const time_point<system_clock> time) {
  return duration_cast<milliseconds>(
             time - system_clock::from_time_t(system_clock::to_time_t(time)))
      .count();
}

std::string microsec_timestamp_to_readable(const uint64_t& timestamp) {
  std::chrono::microseconds dur(timestamp);
  std::chrono::time_point<std::chrono::system_clock> dt(dur);
  return g3::localtime_formatted(
      dt, {g3::internal::date_formatted + " " + g3::internal::time_formatted});
}

bool is_timestamp_in_range(const uint64_t& timestamp, const uint64_t& loBound,
                           const uint64_t& hiBound) {
  return timestamp >= loBound && timestamp <= hiBound;
}
