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

BlockHeaderBase::BlockHeaderBase() {}

BlockHeaderBase::BlockHeaderBase(const CommitteeHash& committeeHash)
    : m_committeeHash(committeeHash) {}

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

const CommitteeHash& BlockHeaderBase::GetCommitteeHash() const {
  return m_committeeHash;
}
