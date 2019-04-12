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

#ifndef __TIMEUTILS_H__
#define __TIMEUTILS_H__

#include <chrono>
#include <string>

std::chrono::system_clock::time_point r_timer_start();
double r_timer_end(std::chrono::system_clock::time_point start_time);

uint64_t get_time_as_int();
struct tm* gmtime_safe(const time_t* timer);
long int get_ms(const std::chrono::time_point<std::chrono::system_clock> time);

std::string microsec_timestamp_to_readable(const uint64_t& timestamp);

bool is_timestamp_in_range(const uint64_t& timestamp, const uint64_t& loBound,
                           const uint64_t& hiBound);
#endif  // __TIMEUTILS_H__
