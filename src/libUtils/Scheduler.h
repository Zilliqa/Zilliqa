/**
* Copyright (c) 2017 Zilliqa 
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

#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>


// should be initialized this way
// CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue, &scheduler);
// threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

class Scheduler
{
public:
    Scheduler();
    ~Scheduler();

    void ScheduleAt(std::function<void (void)> f, 
        std::chrono::time_point<std::chrono::system_clock> t = std::chrono::system_clock::now());

    void ScheduleAfter(std::function<void (void)> f, int64_t deltaMilliSeconds);

    void SchedulePeriodically(std::function<void (void)> f, int64_t deltaMilliSeconds);

    void ServiceQueue();

private:
    std::multimap<std::chrono::time_point<std::chrono::system_clock>, std::function<void (void)>> taskQueue;
    std::condition_variable newTaskScheduled;
    mutable std::mutex newTaskMutex;
};

#endif // __SCHEDULER_H__