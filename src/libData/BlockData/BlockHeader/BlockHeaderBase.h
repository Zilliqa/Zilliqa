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

#ifndef __BLOCKHEADERBASE_H__
#define __BLOCKHEADERBASE_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

// Hash for the committee that generated the block
using CommitteeHash = dev::h256;

const uint64_t INIT_BLOCK_NUMBER = (uint64_t)-1;

/// [TODO] Base class for all supported block header types
class BlockHeaderBase : public SerializableDataBlock {
 protected:
  // TODO: pull out all common code from ds, micro and tx block header
  CommitteeHash m_committeeHash;

 public:
  // Constructors
  BlockHeaderBase();
  BlockHeaderBase(const CommitteeHash& committeeHash);

  /// Calculate my hash
  BlockHash GetMyHash() const;

  const CommitteeHash& GetCommitteeHash() const;

  friend std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t);
};

inline std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t) {
  os << "<BlockHeaderBase>" << std::endl
     << "m_committeeHash : " << t.m_committeeHash;

  return os;
}

#endif  // __BLOCKHEADERBASE_H__
