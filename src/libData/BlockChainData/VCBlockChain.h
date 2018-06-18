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

#ifndef __VCBLOCKCHAIN_H__
#define __VCBLOCKCHAIN_H__

#include <mutex>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "libData/BlockData/Block/VCBlock.h"
#include "libData/DataStructures/CircularArray.h"
#include "libPersistence/BlockStorage.h"

/// Transient storage for VC blocks.
class VCBlockChain
{
    std::mutex m_mutexVCBlocks;
    CircularArray<VCBlock> m_vcBlocks;

public:
    /// Constructor.
    VCBlockChain();

    /// Destructor.
    ~VCBlockChain();

    /// Reset
    void Reset();

    /// Returns the last stored block.
    VCBlock GetLastBlock();

    /// Returns the block at the specified block number.
    VCBlock GetBlock(const boost::multiprecision::uint256_t& blocknum);

    /// Adds a block to the chain.
    int AddBlock(const VCBlock& block);
};

#endif // __VCBLOCKCHAIN_H__