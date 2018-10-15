/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef __BLOCKCHAIN_H__
#define __BLOCKCHAIN_H__

#include <mutex>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

#include "libData/BlockData/Block/DSBlock.h"
#include "libData/DataStructures/CircularArray.h"
#include "libPersistence/BlockStorage.h"

/// Transient storage for DS/Tx/VC blocks.
template <class T>
class BlockChain {
  std::mutex m_mutexBlocks;
  CircularArray<T> m_blocks;

 protected:
  /// Constructor.
  BlockChain() { Reset(); }

  virtual T GetBlockFromPersistentStorage(const uint64_t& blockNum) = 0;

 public:
  /// Destructor.
  ~BlockChain() {}

  /// Reset
  void Reset() { m_blocks.resize(BLOCKCHAIN_SIZE); }

  /// Returns the number of blocks.
  uint64_t GetBlockCount() {
    std::lock_guard<std::mutex> g(m_mutexBlocks);
    return m_blocks.size();
  }

  /// Returns the last stored block.
  T GetLastBlock() {
    std::lock_guard<std::mutex> g(m_mutexBlocks);
    try {
      return m_blocks.back();
    } catch (...) {
      return T();
    }
  }

  /// Returns the block at the specified block number.
  T GetBlock(const uint64_t& blockNum) {
    std::lock_guard<std::mutex> g(m_mutexBlocks);

    if (blockNum >= m_blocks.size()) {
      LOG_GENERAL(WARNING, "Block number "
                               << blockNum
                               << " absent, a dummy block will be used and "
                                  "abnormal behavior may happen!");
      return T();
    } else if (blockNum + m_blocks.capacity() < m_blocks.size()) {
      return GetBlockFromPersistentStorage(blockNum);
    }

    if (m_blocks[blockNum].GetHeader().GetBlockNum() != blockNum) {
      LOG_GENERAL(WARNING,
                  "BlockNum : " << blockNum << " != GetBlockNum() : "
                                << m_blocks[blockNum].GetHeader().GetBlockNum()
                                << ", a dummy block will be used and abnormal "
                                   "behavior may happen!");
      return T();
    }

    return m_blocks[blockNum];
  }

  /// Adds a block to the chain.
  int AddBlock(const T& block) {
    uint64_t blockNumOfNewBlock = block.GetHeader().GetBlockNum();

    std::lock_guard<std::mutex> g(m_mutexBlocks);

    uint64_t blockNumOfExistingBlock =
        m_blocks[blockNumOfNewBlock].GetHeader().GetBlockNum();

    if (blockNumOfExistingBlock < blockNumOfNewBlock ||
        blockNumOfExistingBlock == (uint64_t)-1) {
      m_blocks.insert_new(blockNumOfNewBlock, block);
    } else {
      return -1;
    }

    return 1;
  }
};

class DSBlockChain : public BlockChain<DSBlock> {
 public:
  DSBlock GetBlockFromPersistentStorage(const uint64_t& blockNum) {
    DSBlockSharedPtr block;
    BlockStorage::GetBlockStorage().GetDSBlock(blockNum, block);
    return *block;
  }
};

class TxBlockChain : public BlockChain<TxBlock> {
 public:
  TxBlock GetBlockFromPersistentStorage(const uint64_t& blockNum) {
    TxBlockSharedPtr block;
    BlockStorage::GetBlockStorage().GetTxBlock(blockNum, block);
    return *block;
  }
};

class VCBlockChain : public BlockChain<VCBlock> {
 public:
  VCBlock GetBlockFromPersistentStorage([
      [gnu::unused]] const uint64_t& blockNum) {
    throw "vc block persistent storage not supported";
  }
};

class FallbackBlockChain : public BlockChain<FallbackBlock> {
 public:
  FallbackBlock GetBlockFromPersistentStorage([
      [gnu::unused]] const uint64_t& blockNum) {
    throw "fallback block persistent storage not supported";
  }
};

#endif  // __BLOCKCHAIN_H__