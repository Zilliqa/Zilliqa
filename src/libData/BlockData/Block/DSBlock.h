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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_DSBLOCK_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_DSBLOCK_H_

#include "BlockBase.h"
#include "libData/BlockData/BlockHeader/DSBlockHeader.h"

/// Stores the DS header and signature.
class DSBlock final : public BlockBase {
  DSBlockHeader m_header;

 public:
  /// Default constructor.
  DSBlock() = default;  // creates a dummy invalid placeholder block -- blocknum
                        // is maxsize of uint256

  /// Constructor with specified DS block parameters.
  template <typename CoSignaturesT>
  DSBlock(const DSBlockHeader& header, CoSignaturesT&& coSigs,
          uint64_t timestamp = get_time_as_int())
      : BlockBase{header.GetMyHash(), std::forward<CoSignaturesT>(coSigs),
                  timestamp},
        m_header(header) {}

  /// Implements the Serialize function inherited from Serializable.
  virtual bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  virtual bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  virtual bool Deserialize(const std::string& src,
                           unsigned int offset) override;

  /// Returns the reference to the DSBlockHeader part of the DS block.
  const DSBlockHeader& GetHeader() const noexcept { return m_header; }

  /// Equality comparison operator.
  bool operator==(const DSBlock& block) const;
};

std::ostream& operator<<(std::ostream& os, const DSBlock& t);

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_DSBLOCK_H_
