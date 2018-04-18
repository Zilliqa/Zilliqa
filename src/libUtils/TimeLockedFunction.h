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

#ifndef __TIMELOCKEDFUNCTION_H__
#define __TIMELOCKEDFUNCTION_H__

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>

#include "libUtils/Logger.h"

using namespace std;

/// Utility class for executing a primary function and a subsequent expiry function in separate join-able threads.
class TimeLockedFunction
{
private:
    shared_ptr<promise<int>> result_promise;
    future<int> result_future;

    unique_ptr<thread> thread_main;
    unique_ptr<thread> thread_timer;

public:
    /// Template constructor.
    template<class callable1, class callable2>
    TimeLockedFunction(unsigned int expiration_in_seconds,
                       callable1&& main_func, callable2&& expiration_func,
                       bool call_expiry_always)
        : result_promise(new promise<int>)
    {
        function<typename result_of<callable1()>::type()> task_main(main_func);
        function<typename result_of<callable2()>::type()> task_expiry(
            expiration_func);

        result_future = result_promise->get_future();

        auto func_main = [task_main, task_expiry, call_expiry_always](
                             shared_ptr<promise<int>> result_promise) -> void {
            try
            {
                task_main();
                result_promise->set_value(0);
                if (call_expiry_always)
                {
                    task_expiry();
                }
            }
            catch (future_error&)
            {
                // Function returned too late
            }
        };

        thread_main = make_unique<thread>(func_main, result_promise);

        auto func_timer = [expiration_in_seconds, task_expiry](
                              shared_ptr<promise<int>> result_promise) -> void {
            try
            {
                LOG_GENERAL(INFO,
                            "I am going to sleep for "
                                + to_string(expiration_in_seconds)
                                + " seconds");
                this_thread::sleep_for(chrono::seconds(expiration_in_seconds));
                LOG_GENERAL(INFO,
                            "I have woken up from the sleep of "
                                + to_string(expiration_in_seconds)
                                + " seconds");
                result_promise->set_value(-1);
                task_expiry();
            }
            catch (future_error&)
            {
                // Function returned on time
            }
        };

        thread_timer = make_unique<thread>(func_timer, result_promise);
    }

    /// Destructor. Joins both launched threads.
    ~TimeLockedFunction()
    {
        thread_main->join();
        thread_timer->join();
        result_future.get();
    }
};

#endif // __TIMELOCKEDFUNCTION_H__