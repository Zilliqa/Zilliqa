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

#include "TxBlockChain.h"
#include "common/Constants.h"

using namespace std;
using namespace boost::multiprecision;

TxBlockChain::TxBlockChain() { m_txBlocks.resize(TX_BLOCKCHAIN_SIZE); }

TxBlockChain::~TxBlockChain() {}

void TxBlockChain::Reset() { m_txBlocks.resize(TX_BLOCKCHAIN_SIZE); }

uint256_t TxBlockChain::GetBlockCount()
{
    lock_guard<mutex> g(m_mutexTxBlocks);
    return m_txBlocks.size();
}

TxBlock TxBlockChain::GetLastBlock()
{
    lock_guard<mutex> g(m_mutexTxBlocks);
    return m_txBlocks.back();
}

TxBlock TxBlockChain::GetBlock(const uint256_t& blockNum)
{
    lock_guard<mutex> g(m_mutexTxBlocks);

    if (blockNum >= m_txBlocks.size())
    {
        throw "Blocknumber Absent";
    }
    else if (blockNum + m_txBlocks.capacity() < m_txBlocks.size())
    {
        TxBlockSharedPtr block;
        BlockStorage::GetBlockStorage().GetTxBlock(blockNum, block);
        return *block;
    }

    // To-do: We cannot even index into a vector using uint256_t
    // uint256_t might just be too big to begin with
    // Consider switching to uint64_t
    // For now we directly cast to uint64_t

    if (m_txBlocks[blockNum].GetHeader().GetBlockNum() != blockNum)
    {
        LOG_GENERAL(FATAL,
                    "assertion failed (" << __FILE__ << ":" << __LINE__ << ": "
                                         << __FUNCTION__ << ")");
    }

    return m_txBlocks[blockNum];

    // return NULL;
}

int TxBlockChain::AddBlock(const TxBlock& block)
{
    boost::multiprecision::uint256_t blockNumOfNewBlock
        = block.GetHeader().GetBlockNum();

    lock_guard<mutex> g(m_mutexTxBlocks);

    boost::multiprecision::uint256_t blockNumOfExistingBlock
        = m_txBlocks[blockNumOfNewBlock].GetHeader().GetBlockNum();

    if (blockNumOfExistingBlock < blockNumOfNewBlock
        || blockNumOfExistingBlock == (boost::multiprecision::uint256_t)-1)
    {
        m_txBlocks.insert_new(blockNumOfNewBlock, block);
    }
    else
    {
        return -1;
    }

    return 1;
    // lock_guard<mutex> g(m_mutexTxBlocks);
    // m_txBlocks.push_back(block);
}