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

#ifndef __VCBLOCK_H__
#define __VCBLOCK_H__

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/VCBlockHeader.h"

/// Stores the VC header and signatures.
class VCBlock : public BlockBase {
  VCBlockHeader m_header;

 public:
  /// Default constructor.
  VCBlock();  // creates a dummy invalid placeholder block

  /// Constructor for loading VC block information from a byte stream.
  VCBlock(const bytes& src, unsigned int offset);

  /// Constructor with specified VC block parameters.
  VCBlock(const VCBlockHeader& header, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  /// Return size of serialized structure
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  /// Return 0 if successed, -1 if failed
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the reference to the VCBlockHeader part of the VC block.
  const VCBlockHeader& GetHeader() const;

  /// Equality comparison operator.
  bool operator==(const VCBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const VCBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const VCBlock& block) const;

  friend std::ostream& operator<<(std::ostream& os, const VCBlock& t);
};

inline std::ostream& operator<<(std::ostream& os, const VCBlock& t) {
  const BlockBase& blockBase(t);

  os << "<VCBlock>" << std::endl << blockBase << std::endl << t.m_header;
  return os;
}

#endif  // __VCBLOCK_H__
