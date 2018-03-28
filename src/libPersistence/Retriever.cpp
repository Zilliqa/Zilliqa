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

#include "Retriever.h"

#include <algorithm>
#include <exception>
#include <stdlib.h>
#include <vector>

#include <boost/filesystem.hpp>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/BlockStorage.h"

using namespace boost::filesystem;
namespace filesys = boost::filesystem;

Retriever::Retriever(Mediator& mediator)
    : m_mediator(mediator)
{
}

void Retriever::RetrieveDSBlocks(bool& result)
{
    LOG_MARKER();

    std::list<DSBlockSharedPtr> blocks;
    if (!BlockStorage::GetBlockStorage().GetAllDSBlocks(blocks))
    {
        LOG_MESSAGE("FAIL: RetrieveDSBlocks Incompleted");
        result = false;
        return;
    }

    /// Check whether the termination of last running happens before the last DSEpoch properly ended.
    std::vector<unsigned char> isDSIncompleted;
    if (BlockStorage::GetBlockStorage().GetMetadata(MetaType::DSINCOMPLETED,
                                                    isDSIncompleted))
    {
        if (isDSIncompleted[0] == '1')
        {
            LOG_MESSAGE("Has incompleted DS Block");
            blocks.pop_back();
            if (BlockStorage::GetBlockStorage().DeleteDSBlock(blocks.size()))
            {
                BlockStorage::GetBlockStorage().PutMetadata(
                    MetaType::DSINCOMPLETED, {'0'});
            }
            hasIncompletedDS = true;
        }
    }
    else
    {
        LOG_MESSAGE("No GetMetadata or failed");
        result = false;
        return;
    }

    for (const auto& block : blocks)
    {
        m_mediator.m_dsBlockChain.AddBlock(*block);
    }

    result = true;
}

void Retriever::RetrieveTxBlocks(bool& result)
{
    LOG_MARKER();
    std::list<TxBlockSharedPtr> blocks;
    if (!BlockStorage::GetBlockStorage().GetAllTxBlocks(blocks))
    {
        LOG_MESSAGE("FAIL: RetrieveTxBlocks Incompleted");
        result = false;
        return;
    }

    // truncate the extra final blocks at last
    int totalSize = blocks.size();
    int extra_txblocks = totalSize % NUM_FINAL_BLOCK_PER_POW;
    for (int i = 0; i < extra_txblocks; ++i)
    {
        BlockStorage::GetBlockStorage().DeleteTxBlock(totalSize - i);
        blocks.pop_back();
    }

    for (const auto& block : blocks)
        m_mediator.m_txBlockChain.AddBlock(*block);

    result = true;
}

#ifndef IS_LOOKUP_NODE
bool Retriever::RetrieveTxBodiesDB()
{
    filesys::path p(PERSISTENCE_PATH + "/" + TX_BODY_SUBDIR);
    if (filesys::exists(p))
    {
        std::vector<std::string> dbNames;
        for (auto& entry :
             boost::make_iterator_range(filesys::directory_iterator(p), {}))
        {
            LOG_MESSAGE("Load txBodyDB: " << entry.path().filename().string());
            dbNames.push_back(entry.path().filename().string());
        }
        std::sort(dbNames.begin(), dbNames.end());

        // keep at most NUM_DS_KEEP_TX_BODY num of DB, ignore the temp one if exists
        for (unsigned int i = 0;
             i < (dbNames.size() <= NUM_DS_KEEP_TX_BODY
                      ? (hasIncompletedDS ? dbNames.size() - 1 : dbNames.size())
                      : NUM_DS_KEEP_TX_BODY);
             i++)
        {
            if (!BlockStorage::GetBlockStorage().PushBackTxBodyDB(
                    std::stoi(dbNames[i])))
            {
                return false;
            }
        }

        // remove the temp txbodydb if it exists
        if (dbNames.size() > NUM_DS_KEEP_TX_BODY)
        {
            if (dbNames.size() == NUM_DS_KEEP_TX_BODY + 1)
            {
                filesys::remove_all(p.string() + "/"
                                    + dbNames[NUM_DS_KEEP_TX_BODY]);
            }
            else
            {
                LOG_MESSAGE("We got extra txBody Database, Investigate why!");
                return false;
            }
        }
        else if (hasIncompletedDS)
        {
            filesys::remove_all(p.string() + "/" + dbNames.back());
        }
    }
    else
    {
        LOG_MESSAGE("Subdirectory Not found");
        return false;
    }

    return true;
}
#else // IS_LOOKUP_NODE
bool Retriever::CleanExtraTxBodies()
{
    LOG_MARKER();
    std::list<TxnHash> txnHashes;
    if (BlockStorage::GetBlockStorage().GetAllTxBodiesTmp(txnHashes))
    {
        for (auto i : txnHashes)
        {
            if (!BlockStorage::GetBlockStorage().DeleteTxBody(i))
            {
                LOG_MESSAGE("FAIL: To delete TxHash in TxBodiesTmpDB");
                return false;
            }
        }
    }
    return BlockStorage::GetBlockStorage().ResetDB(BlockStorage::TX_BODY_TMP);
}
#endif // IS_LOOKUP_NODE

bool Retriever::RetrieveStates()
{
    LOG_MARKER();
    return AccountStore::GetInstance().RetrieveFromDisk();
}

bool Retriever::ValidateStates()
{
    LOG_MARKER();
    return m_mediator.m_txBlockChain.GetLastBlock()
               .GetHeader()
               .GetStateRootHash()
        == AccountStore::GetInstance().GetStateRootHash();
}

void Retriever::CleanAll()
{
    if (BlockStorage::GetBlockStorage().ResetAll())
    {
        LOG_MESSAGE("Reset DB Succeed");
    }
    else
    {
        LOG_MESSAGE("FAIL: Reset DB Failed");
    }
}