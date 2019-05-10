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
  uint32_t m_version;
  CommitteeHash m_committeeHash;
  BlockHash m_prevHash;

 public:
  // Constructors
  BlockHeaderBase();
  BlockHeaderBase(const uint32_t& version, const CommitteeHash& committeeHash,
                  const BlockHash& prevHash);

  /// Calculate my hash
  BlockHash GetMyHash() const;

  /// Returns the current version of this block.
  const uint32_t& GetVersion() const;

  /// Returns the hash of the committee where the block was generated
  const CommitteeHash& GetCommitteeHash() const;

  const BlockHash& GetPrevHash() const;

  /// Sets the current version of this block.
  void SetVersion(const uint32_t& version);

  /// Sets the hash of the committee where the block was generated
  void SetCommitteeHash(const CommitteeHash& committeeHash);

  /// Sets the hash of the previous block (DirBlock or TxBlock)
  void SetPrevHash(const BlockHash& prevHash);

  // Operators
  bool operator==(const BlockHeaderBase& header) const;
  bool operator<(const BlockHeaderBase& header) const;
  bool operator>(const BlockHeaderBase& header) const;

  friend std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t);
};

inline std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t) {
  os << "<BlockHeaderBase>" << std::endl
     << " m_version       = " << t.m_version << std::endl
     << " m_committeeHash = " << t.m_committeeHash << std::endl
     << " m_prevHash      = " << t.m_prevHash;

  return os;
}

#endif  // __BLOCKHEADERBASE_H__
