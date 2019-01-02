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

#ifndef __TIMELOCKEDFUNCTION_H__
#define __TIMELOCKEDFUNCTION_H__

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "libUtils/Logger.h"

/// Utility class for executing a primary function and a subsequent expiry
/// function in separate join-able threads.
class TimeLockedFunction {
 private:
  std::shared_ptr<std::promise<int>> result_promise;
  std::future<int> result_future;

  std::unique_ptr<std::thread> thread_main;
  std::unique_ptr<std::thread> thread_timer;

 public:
  /// Template constructor.
  template <class callable1, class callable2>
  TimeLockedFunction(unsigned int expiration_in_seconds, callable1&& main_func,
                     callable2&& expiration_func, bool call_expiry_always)
      : result_promise(new std::promise<int>) {
    std::function<typename std::result_of<callable1()>::type()> task_main(
        main_func);
    std::function<typename std::result_of<callable2()>::type()> task_expiry(
        expiration_func);

    result_future = result_promise->get_future();

    auto func_main =
        [task_main, task_expiry, call_expiry_always](
            std::shared_ptr<std::promise<int>> result_promise) -> void {
      try {
        task_main();
        result_promise->set_value(0);
        if (call_expiry_always) {
          task_expiry();
        }
      } catch (std::future_error&) {
        // Function returned too late
      }
    };

    thread_main = std::make_unique<std::thread>(func_main, result_promise);

    auto func_timer =
        [expiration_in_seconds, task_expiry](
            std::shared_ptr<std::promise<int>> result_promise) -> void {
      try {
        LOG_GENERAL(INFO, "Entering sleep for " +
                              std::to_string(expiration_in_seconds) +
                              " seconds");
        std::this_thread::sleep_for(
            std::chrono::seconds(expiration_in_seconds));
        LOG_GENERAL(INFO, "Woken up from the sleep of " +
                              std::to_string(expiration_in_seconds) +
                              " seconds");
        result_promise->set_value(-1);
        task_expiry();
      } catch (std::future_error&) {
        // Function returned on time
      }
    };

    thread_timer = std::make_unique<std::thread>(func_timer, result_promise);
  }

  /// Destructor. Joins both launched threads.
  ~TimeLockedFunction() {
    thread_main->join();
    thread_timer->join();
    result_future.get();
  }
};

#endif  // __TIMELOCKEDFUNCTION_H__
