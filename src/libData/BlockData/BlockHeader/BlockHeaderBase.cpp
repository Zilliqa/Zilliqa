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

using namespace std;

BlockHash BlockHeaderBase::GetMyHash() const {
  SHA256Calculator sha2;
  zbytes vec;
  Serialize(vec, 0);
  sha2.Update(vec);
  const zbytes& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

bool BlockHeaderBase::operator==(const BlockHeaderBase& header) const {
  return std::tie(m_version, m_committeeHash, m_prevHash) ==
         std::tie(header.m_version, header.m_committeeHash, header.m_prevHash);
}

std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t) {
  os << "<BlockHeaderBase>" << std::endl
     << " m_version       = " << t.GetVersion() << std::endl
     << " m_committeeHash = " << t.GetCommitteeHash() << std::endl
     << " m_prevHash      = " << t.GetPrevHash();

  return os;
}
