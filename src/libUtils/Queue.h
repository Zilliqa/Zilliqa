/*
 * Copyright (C) 2021 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBUTILS_QUEUE_H_
#define ZILLIQA_SRC_LIBUTILS_QUEUE_H_

#include <condition_variable>
#include <deque>
#include <mutex>
#include "libUtils/Logger.h"

namespace utility {

/// Std deque+mutex queue
template <class T>
class Queue {
  using Mutex = std::mutex;
  using Condition = std::condition_variable;

 public:
  explicit Queue(size_t maxSize = std::numeric_limits<size_t>::max())
      : m_maxSize(maxSize), m_stopped(false) {}

  size_t size() const {
    std::unique_lock<Mutex> lk(m_mutex);
    return m_queue.size();
  }

  bool bounded_push(T item) {
    {
      std::lock_guard<Mutex> lk(m_mutex);
      LOG_GENERAL(INFO, "bounded_push1() m_stopped = " << m_stopped << " queue size = " << m_queue.size());
      if (m_stopped || m_queue.size() >= m_maxSize) {
      LOG_GENERAL(INFO, "bounded_push1() error m_stopped = " << m_stopped << " queue size = " << m_queue.size());
        return false;
      }
      m_queue.push_back(std::move(item));
    }
    m_condition.notify_one();
    return true;
  }

  bool bounded_push(T &&item, size_t &queue_size) {
    {
      std::lock_guard<Mutex> lk(m_mutex);
      queue_size = m_queue.size();
      LOG_GENERAL(INFO, "bounded_push2() m_stopped = " << m_stopped << " queue size = " << queue_size);
      if (m_stopped || queue_size >= m_maxSize) {
        LOG_GENERAL(INFO, "bounded_push2() error m_stopped = " << m_stopped << " queue size = " << queue_size);
        return false;
      }
      m_queue.push_back(std::move(item));
      ++queue_size;
    }
    m_condition.notify_one();
    return true;
  }

  bool pop(T &item, size_t &queue_size) {
    std::unique_lock<Mutex> lk(m_mutex);
    m_condition.wait(lk, [this] { return !m_queue.empty() || m_stopped; });
    queue_size = m_queue.size();
    if (m_stopped) {
      LOG_GENERAL(INFO, "pop() error m_stopped = " << m_stopped << " queue size = " << queue_size);
      return false;
    }
    LOG_GENERAL(INFO, "pop() m_stopped = " << m_stopped << " queue size = " << queue_size);
    item = std::move(m_queue.front());
    m_queue.pop_front();
    return true;
  }

bool try_pop(T &item) {
  std::unique_lock<Mutex> lk(m_mutex);
  if (m_stopped || m_queue.empty()) {
      LOG_GENERAL(INFO, "try_pop() error m_stopped = " << m_stopped << " queue size = " << m_queue.size());
      return false;
  }
  item = std::move(m_queue.front());
  size_t queue_size = m_queue.size();
  LOG_GENERAL(INFO, "try_pop() m_stopped = " << m_stopped << " queue size = " << queue_size);
  m_queue.pop_front();
  return true;
}

  void stop() {
    {
      std::lock_guard<Mutex> lk(m_mutex);
      m_stopped = true;
      m_queue.clear();
    }
    m_condition.notify_all();
  }

  void reset() {
    std::lock_guard<Mutex> lk(m_mutex);
    m_stopped = false;
    m_queue.clear();
  }

 private:
  const size_t m_maxSize;
  mutable Mutex m_mutex;
  Condition m_condition;
  std::deque<T> m_queue;
  bool m_stopped;
};

}  // namespace utility

#endif  // ZILLIQA_SRC_LIBUTILS_QUEUE_H_
