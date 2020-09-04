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

#ifndef ZILLIQA_SRC_LIBUTILS_RANDOMGENERATOR_H_
#define ZILLIQA_SRC_LIBUTILS_RANDOMGENERATOR_H_

#include <iostream>
#include <random>

/// Utility function to get random number from range

namespace RandomGenerator {

static auto rng = [] {
  std::mt19937 rng;
  rng.seed(std::random_device()());
  return rng;
}();

template <typename T,
          template <typename>
          class Distribution = std::uniform_int_distribution,
          typename... Args>
T GetRandom(Args&&... args) {
  Distribution<T> dist(std::forward<Args>(args)...);
  return dist(rng);
}

// Get random integer from 0 to size - 1
int GetRandomInt(const int& size);

}  // namespace RandomGenerator

#endif  // ZILLIQA_SRC_LIBUTILS_RANDOMGENERATOR_H_