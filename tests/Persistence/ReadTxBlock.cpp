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

#include <array>
#include <string>
#include <vector>

#include "libData/BlockData/Block.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE persistencetest
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testBlockStorage)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    int blocknumber;

    std::cout << "Enter block number: ";
    std::cin >> blocknumber;

    TxBlockSharedPtr block2;
    BlockStorage::GetBlockStorage().GetTxBlock(blocknumber, block2);

    LOG_GENERAL(
        INFO,
        "Block type value retrieved: " << (*block2).GetHeader().GetType());
    LOG_GENERAL(INFO,
                "Block version value retrieved: "
                    << (*block2).GetHeader().GetVersion());
    LOG_GENERAL(INFO,
                "Block timestamp value retrieved: "
                    << (*block2).GetHeader().GetTimestamp());
    LOG_GENERAL(
        INFO,
        "Block num txs value retrieved: " << (*block2).GetHeader().GetNumTxs());
}

BOOST_AUTO_TEST_SUITE_END()
