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

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/BlockStorage.h"

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

    std::vector<unsigned char> isDSIncompleted;
    if (BlockStorage::GetBlockStorage().GetMetadata(MetaType::DSINCOMPLETED,
                                                    isDSIncompleted))
    {
        LOG_MESSAGE("Has incompleted DS Block");
        BlockStorage::GetBlockStorage().DeleteMetadata(MetaType::DSINCOMPLETED);
        BlockStorage::GetBlockStorage().DeleteDSBlock(blocks.size() - 1);
        blocks.pop_back();
    }
    else
    {
<<<<<<< HEAD
    	LOG_MESSAGE("Has no incompleted DS Block");
=======
        LOG_MESSAGE("FAIL: Retrieve Metadata: DSINCOMPLETED Failed");
>>>>>>> e6cc7a8737ced4404287abc5ce89946a908ecc6d
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

bool Retriever::RetrieveStates()
{
    LOG_MARKER();
    return AccountStore::GetInstance().RetrieveFromDisk();
}

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
    return true;
}

bool Retriever::ValidateStates()
{
    LOG_MARKER();
    // return AccountStore::GetInstance().ValidateStateFromDisk(m_addressToAccount);
    return m_mediator.m_txBlockChain.GetLastBlock()
               .GetHeader()
               .GetStateRootHash()
        == AccountStore::GetInstance().GetStateRootHash();
}

void Retriever::CleanAll()
{
	if(BlockStorage::GetBlockStorage().ResetAll())
	{
		LOG_MESSAGE("Reset DB Succeed");
	}
	else
	{
		LOG_MESSAGE("FAIL: Reset DB Failed");
	}
}