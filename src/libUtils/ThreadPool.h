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

#ifndef CONCURRENT_THREADPOOL_H
#define CONCURRENT_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

/**
 *  Set to 1 to use vector instead of queue for jobs container to improve
 *  memory locality however changes job order from FIFO to LIFO.
 */
#define CONTIGUOUS_JOBS_MEMORY 0
#if CONTIGUOUS_JOBS_MEMORY
#include <vector>
#else
#include <queue>
#endif

/**
 * Simple thread pool that creates `threadCount` threads upon its creation, and
 * pulls from a queue to get new jobs. This class requires a number of C++11
 * features be present in your compiler.
 */
class ThreadPool {
 public:
  typedef std::function<void()> Job;

  /// Constructor.
#if CONTIGUOUS_JOBS_MEMORY
  explicit ThreadPool(const unsigned int threadCount,
                      const std::string& poolName,
                      const unsigned int jobsReserveCount = 0)
#else
  explicit ThreadPool(const unsigned int threadCount,
                      const std::string& poolName)
#endif
      : _jobsLeft(0),
        _bailout(false),
        _poolName(poolName),
        _jobAvailableVar(),
        _jobsLeftMutex(),
        _queueMutex() {
    _threads.reserve(threadCount);
    for (unsigned int index = 0; index < threadCount; ++index) {
      _threads.push_back(std::thread([this] { this->Task(); }));
    }

#if CONTIGUOUS_JOBS_MEMORY
    if (jobsReserveCount > 0) {
      _queue.reserve(jobsReserveCount);
    }
#endif
  }

  /// Destructor (JoinAll on deconstruction).
  ~ThreadPool() { JoinAll(); }

  /// Adds a new job to the pool. If there are no jobs in the queue, a thread is
  /// woken up to take the job. If all threads are busy, the job is added to the
  /// end of the queue.
  void AddJob(const Job& job) {
    std::lock(_queueMutex, _jobsLeftMutex);
    std::lock_guard<std::mutex> lg1(_queueMutex, std::adopt_lock);
    std::lock_guard<std::mutex> lg2(_jobsLeftMutex, std::adopt_lock);

#if CONTIGUOUS_JOBS_MEMORY
    _queue.push_back(job);
#else
    _queue.push(job);
#endif
    ++_jobsLeft;
    _jobAvailableVar.notify_one();

    if (0 == _jobsLeft % 100) {
      LOG_GENERAL(INFO, "PoolName: " << _poolName << " JobLeft: " << _jobsLeft);
    }
  }

  /// Joins with all threads. Blocks until all threads have completed. The queue
  /// may be filled after this call, but the threads will be done. After
  /// invoking JoinAll, the pool can no longer be used.
  void JoinAll() {
    // scoped lock
    {
      std::lock_guard<std::mutex> lock(_queueMutex);
      if (_bailout) {
        return;
      }
      _bailout = true;
    }

    // note that we're done, and wake up any thread that's
    // waiting for a new job
    _jobAvailableVar.notify_all();

    for (std::thread& thread : _threads) {
      try {
        if (thread.joinable()) {
          thread.join();
        }
      } catch (const std::system_error& e) {
        LOG_GENERAL(WARNING, "Caught system_error with code "
                                 << e.code() << " meaning " << e.what()
                                 << '\n');
      }
    }
  }

  /// Gets the vector of threads themselves, in order to set the affinity, or
  /// anything else you might want to do
  std::vector<std::thread>& GetThreads() { return _threads; }

 private:
  /**
   *  Take the next job in the queue and run it.
   *  Notify the main thread that a job has completed.
   */
  void Task() {
    while (true) {
      Job job;

      // scoped lock
      {
        std::unique_lock<std::mutex> lock(_queueMutex);

        if (_bailout) {
          return;
        }

        // Wait for a job if we don't have any.
        _jobAvailableVar.wait(lock,
                              [this] { return !_queue.empty() || _bailout; });

        if (_bailout) {
          return;
        }

        // Get job from the queue
#if CONTIGUOUS_JOBS_MEMORY
        job = _queue.back();
        _queue.pop_back();
#else
        job = _queue.front();
        _queue.pop();
#endif
      }

      job();

      // scoped lock
      {
        std::lock_guard<std::mutex> lock(_jobsLeftMutex);
        --_jobsLeft;
      }
    }
  }

  std::vector<std::thread> _threads;
#if CONTIGUOUS_JOBS_MEMORY
  std::vector<Job> _queue;
#else
  std::queue<Job> _queue;
#endif

  int _jobsLeft;
  bool _bailout;
  std::string _poolName;
  std::condition_variable _jobAvailableVar;
  std::mutex _jobsLeftMutex;
  std::mutex _queueMutex;
};

#undef CONTIGUOUS_JOBS_MEMORY
#endif  // CONCURRENT_THREADPOOL_H
