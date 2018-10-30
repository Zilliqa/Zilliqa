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

#include <utility>

#include "TxBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool TxBlock::Serialize(vector<unsigned char>& dst, unsigned int offset) const {
  if (m_header.GetNumMicroBlockHashes() != m_microBlockHashes.size()) {
    LOG_GENERAL(WARNING, "Header microblock hash count ("
                             << m_header.GetNumMicroBlockHashes()
                             << ") != actual count ("
                             << m_microBlockHashes.size() << ")");
    return false;
  }

  if (!Messenger::SetTxBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlock failed.");
    return false;
  }

  return true;
}

bool TxBlock::Deserialize(const vector<unsigned char>& src,
                          unsigned int offset) {
  if (!Messenger::GetTxBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetTxBlock failed.");
    return false;
  }

  if (m_header.GetNumMicroBlockHashes() != m_microBlockHashes.size()) {
    LOG_GENERAL(WARNING, "Header microblock hash count ("
                             << m_header.GetNumMicroBlockHashes()
                             << ") != actual count ("
                             << m_microBlockHashes.size() << ")");
    return false;
  }

  return true;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
TxBlock::TxBlock() {}

TxBlock::TxBlock(const vector<unsigned char>& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init TxBlock.");
  }
}

TxBlock::TxBlock(const TxBlockHeader& header,
                 const vector<bool>& isMicroBlockEmpty,
                 const vector<BlockHash>& microBlockHashes,
                 const vector<uint32_t>& shardIds, CoSignatures&& cosigs)
    : m_header(header),
      m_isMicroBlockEmpty(isMicroBlockEmpty),
      m_microBlockHashes(microBlockHashes),
      m_shardIds(shardIds) {
  if (m_header.GetNumMicroBlockHashes() != m_microBlockHashes.size()) {
    LOG_GENERAL(WARNING,
                "Num of microblocks get from header "
                    << m_header.GetNumMicroBlockHashes()
                    << " is not equal to the size of m_microBlockHashes "
                    << m_microBlockHashes.size());
  }

  m_cosigs = move(cosigs);
}

TxBlock::TxBlock(const TxBlockHeader& header,
                 const vector<bool>& isMicroBlockEmpty,
                 const vector<BlockHash>& microBlockHashes,
                 const vector<uint32_t>& shardIds)
    : m_header(header),
      m_isMicroBlockEmpty(isMicroBlockEmpty),
      m_microBlockHashes(microBlockHashes),
      m_shardIds(shardIds) {
  if (m_header.GetNumMicroBlockHashes() != m_microBlockHashes.size()) {
    LOG_GENERAL(WARNING,
                "Num of microblocks get from header "
                    << m_header.GetNumMicroBlockHashes()
                    << " is not equal to the size of m_microBlockHashes "
                    << m_microBlockHashes.size());
  }
}

const TxBlockHeader& TxBlock::GetHeader() const { return m_header; }

const std::vector<bool>& TxBlock::GetIsMicroBlockEmpty() const {
  return m_isMicroBlockEmpty;
}

const vector<BlockHash>& TxBlock::GetMicroBlockHashes() const {
  return m_microBlockHashes;
}

const vector<uint32_t>& TxBlock::GetShardIds() const { return m_shardIds; }

bool TxBlock::operator==(const TxBlock& block) const {
  return ((m_header == block.m_header) &&
          (m_microBlockHashes == block.m_microBlockHashes));
}

bool TxBlock::operator<(const TxBlock& block) const {
  return std::tie(block.m_header, block.m_microBlockHashes) >
         std::tie(m_header, m_microBlockHashes);
}

bool TxBlock::operator>(const TxBlock& block) const { return block < *this; }