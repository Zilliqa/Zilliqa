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

#ifndef __BLOCKLINKCHAIN_H__
#define __BLOCKLINKCHAIN_H__

#include "libData/BlockData/Block.h"
#include "libData/DataStructures/CircularArray.h"
#include "libMessage/Messenger.h"
#include "libPersistence/BlockStorage.h"

typedef std::tuple<uint64_t, uint64_t, BlockType, BlockHash> BlockLink;
typedef std::shared_ptr<BlockLink> BlockLinkSharedPtr;

enum BlockLinkIndex : unsigned char {
  INDEX = 0,
  DSINDEX = 1,
  BLOCKTYPE = 2,
  BLOCKHASH = 3,
};

class BlockLinkChain {
  CircularArray<BlockLink> m_blockLinkChain;
  std::mutex m_mutexBlockLinkChain;
  std::deque<std::pair<PubKey, Peer>> m_builtDsCommittee;

 public:
  BlockLink GetFromPersistentStorage(const uint64_t& index) {
    BlockLinkSharedPtr blnkshared;

    if (!BlockStorage::GetBlockStorage().GetBlockLink(index, blnkshared)) {
      LOG_GENERAL(WARNING,
                  "Unable to find blocklink, returning dummy link " << index);
      return BlockLink();
    }

    return *blnkshared;
  }

  void Reset() { m_blockLinkChain.resize(BLOCKCHAIN_SIZE); }

  BlockLinkChain() { Reset(); };

  BlockLink GetBlockLink(const uint64_t& index) {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
    if (m_blockLinkChain.size() <= index) {
      LOG_GENERAL(WARNING,
                  "Unable to find blocklink, returning dummy link " << index);
      return BlockLink();
    } else if (index + m_blockLinkChain.capacity() < m_blockLinkChain.size()) {
      return GetFromPersistentStorage(index);
    }
    if (std::get<BlockLinkIndex::INDEX>(m_blockLinkChain[index]) != index) {
      LOG_GENERAL(WARNING, "Does not match the given index");
      return BlockLink();
    }
    return m_blockLinkChain[index];
  }

  bool AddBlockLink(const uint64_t& index, const uint64_t& dsindex,
                    const BlockType blocktype, const BlockHash& blockhash) {
    uint64_t latestIndex = GetLatestIndex();

    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);

    if ((m_blockLinkChain.size() > 0) && (index <= latestIndex)) {
      LOG_GENERAL(WARNING, "the latest index in the blocklink is greater"
                               << index << " " << latestIndex);
      return false;
    } else if (m_blockLinkChain.size() == 0 && index > 0) {
      LOG_GENERAL(WARNING, "the first index to be inserted should be 0");
      return false;
    }
    m_blockLinkChain.insert_new(
        index, std::make_tuple(index, dsindex, blocktype, blockhash));

    bytes dst;
    LOG_GENERAL(INFO, "[DBS]"
                          << "Stored " << index << " " << dsindex << " "
                          << blocktype << " " << blockhash);

    if (!Messenger::SetBlockLink(
            dst, 0, std::make_tuple(index, dsindex, blocktype, blockhash))) {
      LOG_GENERAL(WARNING, "Could not set BlockLink " << index);
      return false;
    }
    if (!BlockStorage::GetBlockStorage().PutBlockLink(index, dst)) {
      LOG_GENERAL(WARNING, "Could not save blocklink " << index);
      return false;
    }
    return true;
  }

  uint64_t GetLatestIndex() {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
    if (m_blockLinkChain.size() == 0) {
      return 0;
    }
    return std::get<BlockLinkIndex::INDEX>(m_blockLinkChain.back());
  }

  const std::deque<std::pair<PubKey, Peer>>& GetBuiltDSComm() {
    return m_builtDsCommittee;
  }

  void SetBuiltDSComm(const std::deque<std::pair<PubKey, Peer>>& dsComm) {
    m_builtDsCommittee = dsComm;
  }

  const BlockLink& GetLatestBlockLink() {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);

    return m_blockLinkChain.back();
  }
};

#endif  //__BLOCKLINKCHAIN_H__
