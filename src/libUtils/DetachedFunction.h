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

#ifndef ZILLIQA_SRC_LIBUTILS_DETACHEDFUNCTION_H_
#define ZILLIQA_SRC_LIBUTILS_DETACHEDFUNCTION_H_

#include <functional>
#include <thread>
#include "libMetrics/Tracing.h"
#include "libUtils/Logger.h"

/// Utility class for executing a function in one or more separate detached
/// threads.
class DetachedFunction {
 public:
  /// Retry limit for launching the detached threads.
  const static int MaxAttempt = 3;

  /// Template constructor.
  template <class callable, class... arguments>
  DetachedFunction(int num_threads, callable&& f, arguments&&... args) {
    using namespace zil::trace;

    std::function<void()> task;

    if (Tracing::HasActiveSpan()) {
      task = [f = std::forward<callable>(f),
              ... args = std::forward<arguments>(args),
              trace_info = Tracing::GetActiveSpan().GetIds()]() mutable {
        auto span = Tracing::CreateChildSpanOfRemoteTrace(
            FilterClass::FILTER_CLASS_ALL, "DetachedFunction", trace_info);
        f(std::forward<arguments>(args)...);
      };
    } else {
      task = std::bind(std::forward<callable>(f),
                       std::forward<arguments>(args)...);
    }

    bool attempt_flag = false;

    for (int i = 0; i < num_threads; i++) {
      for (int j = 0; j < MaxAttempt; j++) {
        try {
          if (!attempt_flag) {
            std::thread(task).detach();  // attempt to detach a non-thread
            attempt_flag = true;
          }
        } catch (const std::system_error& e) {
          LOG_GENERAL(WARNING,
                      j << " times tried. Caught system_error with code "
                        << e.code() << " meaning " << e.what() << '\n');
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
      attempt_flag = false;
    }
  }
};

#endif  // ZILLIQA_SRC_LIBUTILS_DETACHEDFUNCTION_H_
