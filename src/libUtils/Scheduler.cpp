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

#include "Scheduler.h"
#include "ReverseLock.h"

using namespace std;

Scheduler::Scheduler() {}

Scheduler::~Scheduler() {}

void Scheduler::ServiceQueue()
{
    unique_lock<mutex> lock(newTaskMutex);

    while (true)
    {
        try
        {
            while (taskQueue.empty())
            {
                newTaskScheduled.wait(lock);
            }

            while (!taskQueue.empty())
            {
                std::chrono::time_point<std::chrono::system_clock> timeToWaitFor
                    = taskQueue.begin()->first;
                if (newTaskScheduled.wait_until(lock, timeToWaitFor)
                    == std::cv_status::timeout)
                {
                    break;
                }
            }

            if (taskQueue.empty())
            {
                continue;
            }

            std::function<void(void)> f = taskQueue.begin()->second;
            taskQueue.erase(taskQueue.begin());

            {
                ReverseLock<unique_lock<mutex>> rlock(lock);
                f();
            }
        }
        catch (...)
        {
            throw;
        }
    }
    newTaskScheduled.notify_one();
}

void Scheduler::ScheduleAt(std::function<void(void)> f,
                           chrono::time_point<chrono::system_clock> t)
{
    {
        lock_guard<mutex> lock(newTaskMutex);
        taskQueue.emplace(t, f);
    }
    newTaskScheduled.notify_one();
}

void Scheduler::ScheduleAfter(std::function<void(void)> f,
                              int64_t deltaMilliSeconds)
{
    ScheduleAt(f,
               chrono::system_clock::now()
                   + chrono::milliseconds(deltaMilliSeconds));
}

static void SchedulePeriodicallyHelper(Scheduler* s,
                                       std::function<void(void)> f,
                                       int64_t deltaMilliSeconds)
{
    f();
    s->ScheduleAfter(bind(&SchedulePeriodicallyHelper, s, f, deltaMilliSeconds),
                     deltaMilliSeconds);
}

void Scheduler::SchedulePeriodically(std::function<void(void)> f,
                                     int64_t deltaMilliSeconds)
{
    ScheduleAfter(bind(&SchedulePeriodicallyHelper, this, f, deltaMilliSeconds),
                  deltaMilliSeconds);
}