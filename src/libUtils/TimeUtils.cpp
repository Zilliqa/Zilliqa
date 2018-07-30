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

#include "TimeUtils.h"
#include <mutex>
using namespace std::chrono;
using namespace boost::multiprecision;
static std::mutex gmtimeMutex;

system_clock::time_point r_timer_start() { return system_clock::now(); }

double r_timer_end(system_clock::time_point start_time)
{
    duration<double, std::micro> difference = system_clock::now() - start_time;
    return difference.count();
}

uint256_t get_time_as_int()
{
    microseconds microsecs
        = duration_cast<microseconds>(system_clock::now().time_since_epoch());
    return static_cast<uint256_t>(microsecs.count());
}

struct tm* gmtime_safe(const time_t* timer)
{
    std::lock_guard<std::mutex> guard(gmtimeMutex);
    return gmtime(timer);
}

long int get_ms(const time_point<high_resolution_clock> time)
{
    return duration_cast<milliseconds>(
               time - system_clock::from_time_t(system_clock::to_time_t(time)))
        .count();
}
