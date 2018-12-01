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

#include "BlockBase.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

BlockBase::BlockBase() {}

const uint64_t& BlockBase::GetTimestamp() const { return m_timestamp; }

void BlockBase::SetTimestamp(const uint64_t& timestamp) {
  m_timestamp = timestamp;
}

const BlockHash& BlockBase::GetBlockHash() const { return m_blockHash; }

void BlockBase::SetBlockHash(const BlockHash& blockHash) {
  m_blockHash = blockHash;
}

const Signature& BlockBase::GetCS1() const { return m_cosigs.m_CS1; }

const vector<bool>& BlockBase::GetB1() const { return m_cosigs.m_B1; }

const Signature& BlockBase::GetCS2() const { return m_cosigs.m_CS2; }

const vector<bool>& BlockBase::GetB2() const { return m_cosigs.m_B2; }

void BlockBase::SetCoSignatures(const ConsensusCommon& src) {
  m_cosigs.m_CS1 = src.GetCS1();
  m_cosigs.m_B1 = src.GetB1();
  m_cosigs.m_CS2 = src.GetCS2();
  m_cosigs.m_B2 = src.GetB2();
}

void BlockBase::SetCoSignatures(CoSignatures& cosigs) {
  m_cosigs = move(cosigs);
}