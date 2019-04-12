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

#ifndef __FALLBACKBLOCK_H__
#define __FALLBACKBLOCK_H__

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libData/BlockData/BlockHeader/FallbackBlockHeader.h"
#include "libNetwork/Peer.h"

/// Stores the fallback block header and signature

class FallbackBlock : public BlockBase {
  FallbackBlockHeader m_header;

 public:
  /// Default constructor.
  FallbackBlock();  // creates a dummy invalid placeholder block

  /// Constructor for loading finalblock block information from a byte stream.
  FallbackBlock(const bytes& src, unsigned int offset);

  /// Constructor with specified fallback block parameters.
  FallbackBlock(const FallbackBlockHeader& header, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the reference to the FallbackBlockHeader part of the fallback
  /// block.
  const FallbackBlockHeader& GetHeader() const;

  /// Equality comparison operator.
  bool operator==(const FallbackBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const FallbackBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const FallbackBlock& block) const;

  friend std::ostream& operator<<(std::ostream& os, const FallbackBlock& t);
};

inline std::ostream& operator<<(std::ostream& os, const FallbackBlock& t) {
  const BlockBase& blockBase(t);

  os << "<FallbackBlock>" << std::endl << blockBase << std::endl << t.m_header;
  return os;
}

#endif  // __FALLBACKBLOCK_H__
