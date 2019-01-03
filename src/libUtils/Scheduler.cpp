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

#include "Scheduler.h"
#include "ReverseLock.h"

using namespace std;

Scheduler::Scheduler() {}

Scheduler::~Scheduler() {}

void Scheduler::ServiceQueue() {
  unique_lock<mutex> lock(newTaskMutex);

  while (true) {
    try {
      while (taskQueue.empty()) {
        newTaskScheduled.wait(lock);
      }

      while (!taskQueue.empty()) {
        std::chrono::time_point<std::chrono::system_clock> timeToWaitFor =
            taskQueue.begin()->first;
        if (newTaskScheduled.wait_until(lock, timeToWaitFor) ==
            std::cv_status::timeout) {
          break;
        }
      }

      if (taskQueue.empty()) {
        continue;
      }

      std::function<void(void)> f = taskQueue.begin()->second;
      taskQueue.erase(taskQueue.begin());

      {
        ReverseLock<unique_lock<mutex>> rlock(lock);
        f();
      }
    } catch (...) {
      throw;
    }
  }
  newTaskScheduled.notify_one();
}

void Scheduler::ScheduleAt(std::function<void(void)> f,
                           chrono::time_point<chrono::system_clock> t) {
  {
    lock_guard<mutex> lock(newTaskMutex);
    taskQueue.emplace(t, f);
  }
  newTaskScheduled.notify_one();
}

void Scheduler::ScheduleAfter(std::function<void(void)> f,
                              int64_t deltaMilliSeconds) {
  ScheduleAt(
      f, chrono::system_clock::now() + chrono::milliseconds(deltaMilliSeconds));
}

static void SchedulePeriodicallyHelper(Scheduler* s,
                                       std::function<void(void)> f,
                                       int64_t deltaMilliSeconds) {
  f();
  s->ScheduleAfter(bind(&SchedulePeriodicallyHelper, s, f, deltaMilliSeconds),
                   deltaMilliSeconds);
}

void Scheduler::SchedulePeriodically(std::function<void(void)> f,
                                     int64_t deltaMilliSeconds) {
  ScheduleAfter(bind(&SchedulePeriodicallyHelper, this, f, deltaMilliSeconds),
                deltaMilliSeconds);
}
