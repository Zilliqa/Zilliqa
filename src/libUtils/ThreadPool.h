/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

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

/// Simple thread pool that creates `threadCount` threads upon its creation, and pulls from a queue to get new jobs.
class
    ThreadPool // This class requires a number of c++11 features be present in your compiler.
{
public:
    /// Constructor.
#if CONTIGUOUS_JOBS_MEMORY
    explicit ThreadPool(const unsigned int threadCount, std::string poolName,
                        const unsigned int jobsReserveCount = 0)
        :
#else
    explicit ThreadPool(const unsigned int threadCount, std::string poolName)
        :
#endif
        _jobsLeft(0)
        , _bailout(false)
    {
        _threads.reserve(threadCount);
        for (unsigned int index = 0; index < threadCount; ++index)
        {
            _threads.push_back(std::thread([this] { this->Task(); }));
        }

#if CONTIGUOUS_JOBS_MEMORY
        if (jobsReserveCount > 0)
        {
            _queue.reserve(jobsReserveCount);
        }
#endif
    }

    /// Destructor (JoinAll on deconstruction).
    ~ThreadPool() { JoinAll(); }

    /// Adds a new job to the pool. If there are no jobs in the queue, a thread is woken up to take the job. If all threads are busy, the job is added to the end of the queue.
    void AddJob(const std::function<void()>& job)
    {
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

        if (_jobsLeft % 100 == 0)
        {
            LOG_GENERAL(INFO,
                        "PoolName: " << poolName << " JobLeft: " << _jobsLeft
                                     << '\n');
        }
    }

    /// Joins with all threads. Blocks until all threads have completed. The queue may be filled after this call, but the threads will be done. After invoking JoinAll, the pool can no longer be used.
    void JoinAll()
    {
        // scoped lock
        {
            std::lock_guard<std::mutex> lock(_queueMutex);
            if (_bailout)
            {
                return;
            }
            _bailout = true;
        }

        // note that we're done, and wake up any thread that's
        // waiting for a new job
        _jobAvailableVar.notify_all();

        for (std::thread& thread : _threads)
        {
            try
            {
                if (thread.joinable())
                {
                    thread.join();
                }
            }
            catch (const std::system_error& e)
            {
                LOG_GENERAL(WARNING,
                            "Caught system_error with code "
                                << e.code() << " meaning " << e.what() << '\n');
            }
        }
    }

    /// Waits for the pool to empty before continuing. This does not call `std::thread::join`, it only waits until all jobs have finished executing.
    void WaitAll()
    {
        std::unique_lock<std::mutex> lock(_jobsLeftMutex);
        if (_jobsLeft > 0)
        {
            _waitVar.wait(lock, [this] { return _jobsLeft == 0; });
        }
    }

    /// Gets the vector of threads themselves, in order to set the affinity, or anything else you might want to do
    std::vector<std::thread>& GetThreads() { return _threads; }

private:
    /**
     *  Take the next job in the queue and run it.
     *  Notify the main thread that a job has completed.
     */
    void Task()
    {
        while (true)
        {
            std::function<void()> job;

            // scoped lock
            {
                std::unique_lock<std::mutex> lock(_queueMutex);

                if (_bailout)
                {
                    return;
                }

                // Wait for a job if we don't have any.
                _jobAvailableVar.wait(
                    lock, [this] { return !_queue.empty() || _bailout; });

                if (_bailout)
                {
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

            _waitVar.notify_one();
        }
    }

    std::vector<std::thread> _threads;
#if CONTIGUOUS_JOBS_MEMORY
    std::vector<std::function<void()>> _queue;
#else
    std::queue<std::function<void()>> _queue;
#endif

    int _jobsLeft;
    bool _bailout;
    std::string poolName;
    std::condition_variable _jobAvailableVar;
    std::condition_variable _waitVar;
    std::mutex _jobsLeftMutex;
    std::mutex _queueMutex;
};

#undef CONTIGUOUS_JOBS_MEMORY
#endif //CONCURRENT_THREADPOOL_H
