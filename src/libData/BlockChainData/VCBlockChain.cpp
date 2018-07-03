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

#include "VCBlockChain.h"
#include "common/Constants.h"

using namespace std;
using namespace boost::multiprecision;

VCBlockChain::VCBlockChain() { m_vcBlocks.resize(VC_BLOCKCHAIN_SIZE); }

VCBlockChain::~VCBlockChain() {}

void VCBlockChain::Reset() { m_vcBlocks.resize(VC_BLOCKCHAIN_SIZE); }

VCBlock VCBlockChain::GetLastBlock()
{
    lock_guard<mutex> g(m_mutexVCBlocks);
    return m_vcBlocks.back();
}

VCBlock VCBlockChain::GetBlock(const uint256_t& blockNum)
{
    lock_guard<mutex> g(m_mutexVCBlocks);

    if (blockNum >= m_vcBlocks.size())
    {
        throw "Blocknumber Absent";
    }
    else if (blockNum + m_vcBlocks.capacity() < m_vcBlocks.size())
    {
        throw "vc block persistent storage not supported";
        //DSBlockSharedPtr block;
        //BlockStorage::GetBlockStorage().GetDSBlock(blockNum, block);
        //return *block;
    }

    // To-do: We cannot even index into a vector using uint256_t
    // uint256_t might just be too big to begin with
    // Consider switching to uint64_t
    // For now we directly cast to uint64_t

    assert(m_vcBlocks[blockNum].GetHeader().GetBlockNum() == blockNum);
    return m_vcBlocks[blockNum];
}

int VCBlockChain::AddBlock(const VCBlock& block)
{
    m_vcBlocks.push_back(block);
    return 1;
}