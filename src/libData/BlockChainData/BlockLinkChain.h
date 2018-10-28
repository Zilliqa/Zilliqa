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

#include "libData/BlockData/Block.h"
#include "libData/DataStructures/CircularArray.h"
#include "libMessage/Messenger.h"
#include "libPersistence/BlockStorage.h"

#ifndef __BLOCKLINKCHAIN_H__
#define __BLOCKLINKCHAIN_H__

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

  BlockLink GetBlockLink(const uint64_t& blocknum) {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
    if (m_blockLinkChain.size() <= blocknum) {
      LOG_GENERAL(WARNING, "Unable to find blocklink, returning dummy link "
                               << blocknum);
      return BlockLink();
    } else if (blocknum + m_blockLinkChain.capacity() <
               m_blockLinkChain.size()) {
      return GetFromPersistentStorage(blocknum);
    }
    if (std::get<BlockLinkIndex::INDEX>(m_blockLinkChain[blocknum]) !=
        blocknum) {
      LOG_GENERAL(WARNING, "Does not match the given blocknum");
      return BlockLink();
    }
    return m_blockLinkChain[blocknum];
  }

  void AddBlockLink(const uint64_t& index, const uint64_t& dsindex,
                    const BlockType blocktype, const BlockHash& blockhash) {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
    m_blockLinkChain.insert_new(
        index, std::make_tuple(index, dsindex, blocktype, blockhash));

    std::vector<unsigned char> dst;
    LOG_GENERAL(INFO, "[DBS]"
                          << "Stored " << index << " " << dsindex << " "
                          << blocktype << " " << blockhash);

    if (!Messenger::SetBlockLink(
            dst, 0, std::make_tuple(index, dsindex, blocktype, blockhash))) {
      LOG_GENERAL(WARNING, "Could not set BlockLink " << index);
      return;
    }
    if (!BlockStorage::GetBlockStorage().PutBlockLink(index, dst)) {
      LOG_GENERAL(WARNING, "Could not save blocklink " << index);
    }
  }

  uint64_t GetLatestIndex() {
    std::lock_guard<std::mutex> g(m_mutexBlockLinkChain);
    if (m_blockLinkChain.size() == 0) {
      return 0;
    }
    return std::get<BlockLinkIndex::INDEX>(m_blockLinkChain.back());
  }

  std::deque<std::pair<PubKey, Peer>>& GetBuiltDSComm() {
    return m_builtDsCommittee;
  }

  void SetBuiltDSComm(const std::deque<std::pair<PubKey, Peer>>& dsComm) {
    m_builtDsCommittee = dsComm;
  }
};

#endif  //__BLOCKLINKCHAIN_H__