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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_VCBLOCK_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_VCBLOCK_H_

#include "BlockBase.h"
#include "libData/BlockData/BlockHeader/VCBlockHeader.h"

/// Stores the VC header and signatures.
class VCBlock final : public BlockBase {
  VCBlockHeader m_header;

 public:
  /// Default constructor.
  VCBlock() = default;  // creates a dummy invalid placeholder block

  /// Constructor with specified VC block parameters.
  VCBlock(const VCBlockHeader& header, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  /// Return size of serialized structure
  virtual bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  /// Return 0 if successed, -1 if failed
  virtual bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  /// Return 0 if successed, -1 if failed
  virtual bool Deserialize(const std::string& src,
                           unsigned int offset) override;

  /// Returns the reference to the VCBlockHeader part of the VC block.
  const VCBlockHeader& GetHeader() const noexcept { return m_header; }

  /// Equality comparison operator.
  bool operator==(const VCBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const VCBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const VCBlock& block) const;

  friend std::ostream& operator<<(std::ostream& os, const VCBlock& t);
};

std::ostream& operator<<(std::ostream& os, const VCBlock& t);

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_VCBLOCK_H_
