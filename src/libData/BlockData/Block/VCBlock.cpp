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

#include "VCBlock.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

// creates a dummy invalid placeholder block -- blocknum is maxsize of uint256
VCBlock::VCBlock() {}

// To-do: handle exceptions. Will be deprecated.
VCBlock::VCBlock(const bytes& src, unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(WARNING, "Error. We failed to initialize VCBlock.");
  }
}

VCBlock::VCBlock(const VCBlockHeader& header, CoSignatures&& cosigs)
    : m_header(header) {
  m_cosigs = move(cosigs);
  SetTimestamp(get_time_as_int());
  SetBlockHash(m_header.GetMyHash());
}

bool VCBlock::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetVCBlock(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetVCBlock failed.");
    return false;
  }

  return true;
}

bool VCBlock::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetVCBlock(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetVCBlock failed.");
    return false;
  }

  return true;
}

const VCBlockHeader& VCBlock::GetHeader() const { return m_header; }

bool VCBlock::operator==(const VCBlock& block) const {
  return (m_header == block.m_header);
}

bool VCBlock::operator<(const VCBlock& block) const {
  return block.m_header > m_header;
}

bool VCBlock::operator>(const VCBlock& block) const { return block < *this; }