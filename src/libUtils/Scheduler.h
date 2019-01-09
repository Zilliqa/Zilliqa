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

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <mutex>
#include <thread>

// should be initialized this way
// CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue,
// &scheduler);
// threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>,
// "scheduler", serviceLoop));

/// [TODO] Currently unused
class Scheduler {
 public:
  Scheduler();
  ~Scheduler();

  void ScheduleAt(std::function<void(void)> f,
                  std::chrono::time_point<std::chrono::system_clock> t =
                      std::chrono::system_clock::now());

  void ScheduleAfter(std::function<void(void)> f, int64_t deltaMilliSeconds);

  void SchedulePeriodically(std::function<void(void)> f,
                            int64_t deltaMilliSeconds);

  void ServiceQueue();

 private:
  std::multimap<std::chrono::time_point<std::chrono::system_clock>,
                std::function<void(void)>>
      taskQueue;
  std::condition_variable newTaskScheduled;
  mutable std::mutex newTaskMutex;
};

#endif  // __SCHEDULER_H__
