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

#ifndef __JOINABLEFUNCTION_H__
#define __JOINABLEFUNCTION_H__

#include <chrono>
#include <functional>
#include <future>
#include <vector>

/// Utility class for executing a function in separate join-able threads.
class JoinableFunction {
  std::vector<std::future<void>> futures;
  bool joined;

 public:
  /// Template constructor.
  template <class callable, class... arguments>
  JoinableFunction(int num_threads, callable&& f, arguments&&... args) {
    std::function<typename std::result_of<callable(arguments...)>::type()> task(
        std::bind(std::forward<callable>(f), std::forward<arguments>(args)...));

    for (int i = 0; i < num_threads; i++) {
      futures.push_back(std::async(std::launch::async, task));
    }

    joined = false;
  }

  /// Destructor. Calls join if it has not been called manually.
  ~JoinableFunction() {
    if (!joined) {
      join();
    }
  }

  /// Joins all the launched threads.
  void join() {
    for (auto& e : futures) {
      e.get();
    }

    joined = true;
  }
};

#endif  // __JOINABLEFUNCTION_H__
