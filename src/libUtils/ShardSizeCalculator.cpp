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

#include "ShardSizeCalculator.h"
#include "libUtils/Logger.h"

uint32_t ShardSizeCalculator::CalculateShardSize(const uint32_t numberOfNodes)
{
    if (numberOfNodes < 651)
    {
        return 600;
    }
    else if (numberOfNodes >= 651)
    {
        return 651;
    }
    else if (numberOfNodes >= 1368)
    {
        return 684;
    }
    else if (numberOfNodes >= 2133)
    {
        return 711;
    }
    else if (numberOfNodes >= 2868)
    {
        return 717;
    }
    else if (numberOfNodes >= 3675)
    {
        return 735;
    }
    else if (numberOfNodes >= 4464)
    {
        return 744;
    }
    else if (numberOfNodes >= 5229)
    {
        return 747;
    }
    else if (numberOfNodes >= 6024)
    {
        return 753;
    }
    else if (numberOfNodes >= 6858)
    {
        return 762;
    }
    else if (numberOfNodes >= 7710)
    {
        return 771;
    }
    else if (numberOfNodes >= 8580)
    {
        return 780;
    }
    else if (numberOfNodes >= 9468)
    {
        return 789;
    }
    else if (numberOfNodes >= 10335)
    {
        return 795;
    }
    else if (numberOfNodes >= 11130)
    {
        return 795;
    }
    else if (numberOfNodes >= 11925)
    {
        return 795;
    }
    else if (numberOfNodes >= 12720)
    {
        return 795;
    }
    else if (numberOfNodes >= 13515)
    {
        return 795;
    }
    else if (numberOfNodes >= 14364)
    {
        return 798;
    }
    else if (numberOfNodes >= 15390)
    {
        return 810;
    }
    else if (numberOfNodes >= 16200)
    {
        return 810;
    }
    else if (numberOfNodes >= 17010)
    {
        return 810;
    }
    else if (numberOfNodes >= 17820)
    {
        return 810;
    }
    else if (numberOfNodes >= 18768)
    {
        return 816;
    }
    else if (numberOfNodes >= 19584)
    {
        return 816;
    }
    else if (numberOfNodes >= 20400)
    {
        return 816;
    }
    else
    {
        LOG_GENERAL(WARNING, "Number of nodes exceeded initial calculation.");
        return 816;
    }
}
