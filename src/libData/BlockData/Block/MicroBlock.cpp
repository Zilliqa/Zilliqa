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

#include "MicroBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

bool MicroBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Header txn count (" << m_header.GetNumTxs()
                                              << ") != txn hash count ("
                                              << m_tranHashes.size() << ")");
    return false;
  }

  if (!Messenger::SetMicroBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlock failed.");
    return false;
  }

  return true;
}

bool MicroBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetMicroBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetMicroBlock failed.");
    return false;
  }

  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Header txn count (" << m_header.GetNumTxs()
                                              << ") != txn hash count ("
                                              << m_tranHashes.size() << ")");
    return false;
  }

  return true;
}

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
MicroBlock::MicroBlock() {}

MicroBlock::MicroBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "We failed to init MicroBlock.");
  }
}

MicroBlock::MicroBlock(const MicroBlockHeader& header,
                       const vector<TxnHash>& tranHashes, CoSignatures&& cosigs)
    : m_header(header), m_tranHashes(tranHashes) {
  if (m_header.GetNumTxs() != m_tranHashes.size()) {
    LOG_GENERAL(WARNING, "Num of Txns get from header "
                             << m_header.GetNumTxs()
                             << " is not equal to the size of m_tranHashes "
                             << m_tranHashes.size());
  }

  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

const MicroBlockHeader& MicroBlock::GetHeader() const { return m_header; }

const vector<TxnHash>& MicroBlock::GetTranHashes() const {
  return m_tranHashes;
}

bool MicroBlock::operator==(const MicroBlock& block) const {
  return ((m_header == block.m_header) && (m_tranHashes == block.m_tranHashes));
}

bool MicroBlock::operator<(const MicroBlock& block) const {
  return std::tie(block.m_header, block.m_tranHashes) >
         std::tie(m_header, m_tranHashes);
}

bool MicroBlock::operator>(const MicroBlock& block) const {
  return block < *this;
}
