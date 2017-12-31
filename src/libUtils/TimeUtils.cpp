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

#include "TimeUtils.h"

using namespace boost::multiprecision;

struct timespec r_timer_start()
{
    struct timespec start_time;
    clock_gettime(CLOCK_REALTIME, &start_time);
    return start_time;
}

double r_timer_end(struct timespec start_time)
{
    struct timespec end_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    double diffInMicroSecs = (end_time.tv_sec - start_time.tv_sec) * 1000000 + (end_time.tv_nsec - start_time.tv_nsec) / 1000;
    return diffInMicroSecs;
}

uint256_t get_time_as_int()
{
	struct timespec now_time;
	clock_gettime(CLOCK_REALTIME, &now_time);
	uint256_t microsecs = now_time.tv_sec * 100000;
	microsecs += now_time.tv_nsec / 1000;
	return microsecs;
}
