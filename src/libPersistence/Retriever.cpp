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
#include "libNode/Node.h"
#include "libMediator/Mediator.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libPersistence/BlockStorage.h"

#include <unordered_map>

Retriever::Retriever(Node * node) : m_node(node) {}

bool Retriever::RetrieveDSBlocks()
{
	std::list<DSBlockSharedPtr> blocks;
	if(!BlockStorage::GetBlockStorage().GetAllDSBlocks(blocks))
	{
		LOG_MESSAGE("RetrieveDSBlocks Incompleted");
		return false;
	}
	for(const auto & block : blocks)
		m_node->m_mediator.m_dsBlockChain.AddBlock(*block);
	return true;
}

bool Retriever::RetrieveTxBlocks()
{
	std::list<TxBlockSharedPtr> blocks;
	if(!BlockStorage::GetBlockStorage().GetAllTXBlocks(blocks))
	{
		LOG_MESSAGE("RetrieveTxBlocks Incompleted");
		return false;
	}

	// truncate the extra final blocks at last
	for(int i = 0; i < blocks.size() % NUM_FINAL_BLOCK_PER_POW; ++i)
	{
		blocks.pop_back();
	}

	for(const auto & block : blocks)
			m_node->m_mediator.m_txBlockChain.AddBlock(*block);
}

bool Retriever::RetrieveTxBodies()
{
	boost::multiprecision::uint256_t blockSize = m_node->m_mediator.m_txBlockChain.GetBlockCount();

	for(boost::multiprecision::uint256_t blockNum; blockNum < blockSize; ++blockNum)
	{
		std::vector<TxnHash> txnHashes = m_node->m_mediator.m_txBlockChain.GetBlock(blockNum).GetHeader().GetMicroBlockHashes();
		std::list<Transaction> transactions;
		for(auto & txnHash : txnHashes)
		{
			TxBodySharedPtr txBody;
			if(!GetTxBody(txhHash, txBody))
			{
				LOG_MESSAGE("RetrieveTxBodies Incompleted");
				m_node->m_committedTransactions.clear();
				return false;
			}
			transactions.push_back(*txBody);
			//Rebuild the AccountStore with UpdateAccounts from these transactions.
			//Compare with the state retrieved from database directly to make an validation.
			AccountStore::GetInstancde().UpdateAccounts(*txBody);
		}
		//m_node->m_committedTransactions.insert(blockNum, transactions);
	}
}

bool Retriever::RetrieveLastStates()
{
	//TODO
}