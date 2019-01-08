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

#include "BlockHeaderBase.h"
#include "libCrypto/Sha2.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

BlockHeaderBase::BlockHeaderBase() : m_version(0) {}

BlockHeaderBase::BlockHeaderBase(const uint32_t& version,
                                 const CommitteeHash& committeeHash,
                                 const BlockHash& prevHash)
    : m_version(version),
      m_committeeHash(committeeHash),
      m_prevHash(prevHash) {}

BlockHash BlockHeaderBase::GetMyHash() const {
  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  bytes vec;
  Serialize(vec, 0);
  sha2.Update(vec);
  const bytes& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

const uint32_t& BlockHeaderBase::GetVersion() const { return m_version; }

void BlockHeaderBase::SetVersion(const uint32_t& version) {
  m_version = version;
}

const CommitteeHash& BlockHeaderBase::GetCommitteeHash() const {
  return m_committeeHash;
}

void BlockHeaderBase::SetCommitteeHash(const CommitteeHash& committeeHash) {
  m_committeeHash = committeeHash;
}

const BlockHash& BlockHeaderBase::GetPrevHash() const { return m_prevHash; }

void BlockHeaderBase::SetPrevHash(const BlockHash& prevHash) {
  m_prevHash = prevHash;
}

bool BlockHeaderBase::operator==(const BlockHeaderBase& header) const {
  return std::tie(m_version, m_committeeHash, m_prevHash) ==
         std::tie(header.m_version, header.m_committeeHash, header.m_prevHash);
}

bool BlockHeaderBase::operator<(const BlockHeaderBase& header) const {
  return m_version < header.m_version;
}

bool BlockHeaderBase::operator>(const BlockHeaderBase& header) const {
  return header < *this;
}
