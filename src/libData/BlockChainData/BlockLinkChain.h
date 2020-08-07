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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKCHAINDATA_BLOCKLINKCHAIN_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKCHAINDATA_BLOCKLINKCHAIN_H_

#include "libData/BlockData/Block.h"
#include "libData/DataStructures/CircularArray.h"

typedef std::tuple<uint32_t, uint64_t, uint64_t, BlockType, BlockHash>
    BlockLink;
typedef std::shared_ptr<BlockLink> BlockLinkSharedPtr;

enum BlockLinkIndex : unsigned char {
  VERSION = 0,
  INDEX = 1,
  DSINDEX = 2,
  BLOCKTYPE = 3,
  BLOCKHASH = 4,
};

class BlockLinkChain {
  CircularArray<BlockLink> m_blockLinkChain;
  std::mutex m_mutexBlockLinkChain;
  DequeOfNode m_builtDsCommittee;

 public:
  static BlockLink GetFromPersistentStorage(const uint64_t& index);
  void Reset();

  BlockLinkChain();

  BlockLink GetBlockLink(const uint64_t& index);

  bool AddBlockLink(const uint64_t& index, const uint64_t& dsindex,
                    const BlockType blocktype, const BlockHash& blockhash);
  uint64_t GetLatestIndex();

  const DequeOfNode& GetBuiltDSComm();
  void SetBuiltDSComm(const DequeOfNode& dsComm);
  const BlockLink& GetLatestBlockLink();
};

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKCHAINDATA_BLOCKLINKCHAIN_H_
