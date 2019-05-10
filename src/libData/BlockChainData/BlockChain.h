/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __BLOCKCHAIN_H__
#define __BLOCKCHAIN_H__

#include <mutex>

#include "libData/BlockData/Block/DSBlock.h"
#include "libData/DataStructures/CircularArray.h"
#include "libPersistence/BlockStorage.h"

/// Transient storage for DS/Tx/ Blocks. The block should have function
/// .GetHeader().GetBlockNum()
template <class T>
class BlockChain {
  std::mutex m_mutexBlocks;
  CircularArray<T> m_blocks;

 protected:
  /// Constructor.
  BlockChain() { Reset(); }

  ~BlockChain() {}

  virtual T GetBlockFromPersistentStorage(const uint64_t& blockNum) = 0;

 public:
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

    if (m_blocks.size() > 0 &&
        (m_blocks.back().GetHeader().GetBlockNum() < blockNum)) {
      LOG_GENERAL(WARNING,
                  "BlockNum too high " << blockNum << " Dummy block used");
      return T();
    }

    else if (blockNum + m_blocks.capacity() < m_blocks.size()) {
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
        INIT_BLOCK_NUMBER == blockNumOfExistingBlock) {
      if (m_blocks.size() > 0) {
        uint64_t blockNumOfLastBlock =
            m_blocks.back().GetHeader().GetBlockNum();
        uint64_t blockNumMissed = blockNumOfNewBlock - blockNumOfLastBlock - 1;
        if (blockNumMissed > 0) {
          LOG_GENERAL(INFO,
                      "block number inconsistent, increase the size of "
                      "CircularArray, blockNumMissed: "
                          << blockNumMissed);
          m_blocks.increase_size(blockNumMissed);
        }
      }
      m_blocks.insert_new(blockNumOfNewBlock, block);
    } else {
      LOG_GENERAL(WARNING, "Failed to add " << blockNumOfNewBlock << " "
                                            << blockNumOfExistingBlock);
      return -1;
    }

    return 1;
  }
};

class DSBlockChain : public BlockChain<DSBlock> {
 public:
  DSBlock GetBlockFromPersistentStorage(const uint64_t& blockNum) {
    DSBlockSharedPtr block;
    if (!BlockStorage::GetBlockStorage().GetDSBlock(blockNum, block)) {
      LOG_GENERAL(WARNING, "BlockNum not in persistent storage "
                               << blockNum << " Dummy block used");
      return DSBlock();
    }
    return *block;
  }
};

class TxBlockChain : public BlockChain<TxBlock> {
 public:
  TxBlock GetBlockFromPersistentStorage(const uint64_t& blockNum) {
    TxBlockSharedPtr block;
    if (!BlockStorage::GetBlockStorage().GetTxBlock(blockNum, block)) {
      LOG_GENERAL(WARNING, "BlockNum not in persistent storage "
                               << blockNum << " Dummy block used");
      return TxBlock();
    }
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
