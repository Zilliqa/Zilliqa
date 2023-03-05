 /*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_
#define ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_



 // fake the class to get things to compile for apple,
 // will be implemented in clang 16, apple still on clang 14

namespace std {
struct source_location {
  // source location construction
  static  source_location current() noexcept { return source_location{}; };
  constexpr source_location() noexcept {};

  // These are just mocked up to make things compile,we will lose
  // functionality by doing this but can rectify later on the mac.
  // source location field access
  constexpr uint_least32_t line() const noexcept { return __LINE__ ;};
  constexpr uint_least32_t column() const noexcept { return 0; };
  constexpr const char* file_name() const noexcept { return __FILE_NAME__ ;};
  constexpr const char* function_name() const noexcept { return __FUNCTION__ ;};
};
}

#endif  // ZILLIQA_SRC_LIBMETRICS_INTERNAL_SCOPE_H_
