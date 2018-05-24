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

#ifndef __DETACHEDFUNCTION_H__
#define __DETACHEDFUNCTION_H__

#include "libUtils/Logger.h"
#include <functional>
#include <thread>

/// Utility class for executing a function in one or more separate detached threads.
class DetachedFunction
{
public:
    /// Retry limit for launching the detached threads.
    const static int MaxAttempt = 3;

    /// Template constructor.
    template<class callable, class... arguments>
    DetachedFunction(int num_threads, callable&& f, arguments&&... args)
    {
        std::function<typename std::result_of<callable(arguments...)>::type()>
            task(std::bind(std::forward<callable>(f),
                           std::forward<arguments>(args)...));

        int attemp_flag = false;

        for (int i = 0; i < num_threads; i++)
        {
            for (int j = 0; j < MaxAttempt; j++)
            {
                try
                {
                    if (attemp_flag == false)
                    {
                        std::thread(task)
                            .detach(); // attempt to detach a non-thread
                        attemp_flag = true;
                    }
                }
                catch (const std::system_error& e)
                {
                    LOG_GENERAL(
                        WARNING,
                        j << " times tried. Caught system_error with code "
                          << e.code() << " meaning " << e.what() << '\n');
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            attemp_flag = false;
        }
    }
};

#endif // __DETACHEDFUNCTION_H__