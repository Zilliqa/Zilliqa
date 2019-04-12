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

#ifndef __DSBLOCK_H__
#define __DSBLOCK_H__

#include <array>

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/DSBlockHeader.h"

/// Stores the DS header and signature.
class DSBlock : public BlockBase {
  DSBlockHeader m_header;

 public:
  /// Default constructor.
  DSBlock();  // creates a dummy invalid placeholder block -- blocknum is
              // maxsize of uint256

  /// Constructor for loading DS block information from a byte stream.
  DSBlock(const bytes& src, unsigned int offset);

  /// Constructor with specified DS block parameters.
  DSBlock(const DSBlockHeader& header, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the reference to the DSBlockHeader part of the DS block.
  const DSBlockHeader& GetHeader() const;

  /// Equality comparison operator.
  bool operator==(const DSBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const DSBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const DSBlock& block) const;

  friend std::ostream& operator<<(std::ostream& os, const DSBlock& t);
};

inline std::ostream& operator<<(std::ostream& os, const DSBlock& t) {
  const BlockBase& blockBase(t);

  os << "<DSBlock>" << std::endl << blockBase << std::endl << t.m_header;
  return os;
}

#endif  // __DSBLOCK_H__
