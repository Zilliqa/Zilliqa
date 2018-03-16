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

#ifndef IS_LOOKUP_NODE

Retriever::Retriever(Mediator & mediator) : m_mediator(mediator) {}

bool Retriever::RetrieveTxBlocks()
{
	LOG_MARKER();
	std::list<TxBlockSharedPtr> blocks;
	if(!BlockStorage::GetBlockStorage().GetAllTxBlocks(blocks))
	{
		LOG_MESSAGE("FAIL: RetrieveTxBlocks Incompleted");
		return false;
	}

	// truncate the extra final blocks at last
	int extra_txblocks = (int)(blocks.size() % NUM_FINAL_BLOCK_PER_POW);
	for(int i = 0; i < extra_txblocks; ++i)
	{
		blocks.pop_back();
	}

	for(const auto & block : blocks)
		m_mediator.m_txBlockChain.AddBlock(*block);
	// m_mediator.UpdateTxBlockRand();

	return true;
}

bool Retriever::RetrieveTxBodies(std::unordered_map<boost::multiprecision::uint256_t, 
                       std::list<Transaction>> & committedTransactions)
{
	LOG_MARKER();
	boost::multiprecision::uint256_t blockSize = m_mediator.m_txBlockChain.GetBlockCount();

	for(boost::multiprecision::uint256_t blockNum; blockNum < blockSize; ++blockNum)
	{
		LOG_MESSAGE("Withdraw txBodies for txBlockNum:" << blockNum);
		std::vector<TxnHash> txnHashes = m_mediator.m_txBlockChain.GetBlock(blockNum).GetMicroBlockHashes();
		std::list<Transaction> transactions;
		for(auto & txnHash : txnHashes)
		{
			LOG_MESSAGE("Withdraw txBody for txHash:" << txnHash);
			TxBodySharedPtr txBody;
			if(!BlockStorage::GetBlockStorage().GetTxBody(txnHash, txBody))
			{
				LOG_MESSAGE("FAIL: RetrieveTxBodies Incompleted");
				committedTransactions.clear();
				return false;
			}
			transactions.push_back(*txBody);
			//Rebuild the AccountStore with UpdateAccounts from these transactions.
			//Compare with the state retrieved from database directly to make an validation.
			AccountStore::GetInstance().UpdateAccounts(*txBody);
		}
		// if(blockNum == blockSize - 1)
			// committedTransactions.insert({blockNum, transactions});
	}

	return true;
}

bool Retriever::RetrieveStates()
{
	LOG_MARKER();
	return AccountStore::GetInstance().RetrieveFromDisk(m_addressToAccount);
}

bool Retriever::ValidateTxNSt()
{
	LOG_MARKER();
	return AccountStore::GetInstance().ValidateStateFromDisk(m_addressToAccount);
}

void Retriever::RetrieveDSBlocks(bool & result)
{
	LOG_MARKER();
	std::list<DSBlockSharedPtr> blocks;
    if(!BlockStorage::GetBlockStorage().GetAllDSBlocks(blocks))
    {
        LOG_MESSAGE("FAIL: RetrieveDSBlocks Incompleted");
        result = false;
        return;
    }
    for(const auto & block : blocks)
        m_mediator.m_dsBlockChain.AddBlock(*block);

    // m_mediator.UpdateDSBlockRand();

    result = true;
}

void Retriever::RetrieveTxNSt(bool & result, std::unordered_map<boost::multiprecision::uint256_t, 
                       std::list<Transaction>> & committedTransactions)
{
	LOG_MARKER();
    result = RetrieveStates();
    if(result)
    {
        result = RetrieveTxBlocks();
    }
    else
    {
        LOG_MESSAGE("FAIL: Failed to retrieve last states");
    	return;
    }
    // if(result)
    // {
    //     result = RetrieveTxBodies(committedTransactions);
    // }
    // else
    // {
    // 	LOG_MESSAGE("FAIL: Failed to retrieve transaction blocks");
    // 	return;
    // }
    if(result)
    {
        result = ValidateTxNSt();
    }
    else
    {
        LOG_MESSAGE("FAIL: Failed to retrieve transaction bodies");
        return;
    }
    if(result)
    {
    	LOG_MESSAGE("RetrieveTxNSt is Successful");
    }
    else
    {
    	LOG_MESSAGE("ERROR: Result of <RetrieveStates> and <RetrieveTxBlocks/Bodies> doesn't match");
    }
}

#endif // IS_LOOKUP_NODE