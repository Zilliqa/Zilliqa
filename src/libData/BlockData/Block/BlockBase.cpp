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

#include <utility>

#include "BlockBase.h"
#include "libUtils/BitVector.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

BlockBase::BlockBase() : m_timestamp(0) {}

bool BlockBase::Serialize([[gnu::unused]] bytes& dst,
                          [[gnu::unused]] unsigned int offset) const {
  return true;
}

bool BlockBase::Deserialize([[gnu::unused]] const bytes& src,
                            [[gnu::unused]] unsigned int offset) {
  return true;
}

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
