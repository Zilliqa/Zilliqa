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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_TXBLOCK_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_TXBLOCK_H_

#include "BlockBase.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"

struct MicroBlockInfo {
  BlockHash m_microBlockHash;
  TxnHash m_txnRootHash;
  uint32_t m_shardId{};

  bool operator==(const MicroBlockInfo& mbInfo) const {
    return std::tie(m_microBlockHash, m_txnRootHash, m_shardId) ==
           std::tie(mbInfo.m_microBlockHash, mbInfo.m_txnRootHash,
                    mbInfo.m_shardId);
  }
  bool operator<(const MicroBlockInfo& mbInfo) const {
    return std::tie(mbInfo.m_microBlockHash, mbInfo.m_txnRootHash,
                    mbInfo.m_shardId) >
           std::tie(m_microBlockHash, m_txnRootHash, m_shardId);
  }
  bool operator>(const MicroBlockInfo& mbInfo) const { return mbInfo < *this; }
};

std::ostream& operator<<(std::ostream& os, const MicroBlockInfo& t);

/// Stores the Tx block header and signature.

class TxBlock final : public BlockBase {
  TxBlockHeader m_header;
  std::vector<MicroBlockInfo> m_mbInfos;

 public:
  /// Default constructor.
  TxBlock() = default;  // creates a dummy invalid placeholder block -- blocknum
                        // is maxsize of uint256

  /// Constructor with predefined member values
  template <typename CoSignaturesT>
  TxBlock(const TxBlockHeader& header,
          const std::vector<MicroBlockInfo>& mbInfos, CoSignaturesT&& coSigs,
          uint64_t timestamp = get_time_as_int())
      : BlockBase{header.GetMyHash(), std::forward<CoSignaturesT>(coSigs),
                  timestamp},
        m_header(header),
        m_mbInfos(mbInfos) {}

  /// Implements the Serialize function inherited from Serializable.
  virtual bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  virtual bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  virtual bool Deserialize(const std::string& src,
                           unsigned int offset) override;

  /// Returns the reference to the TxBlockHeader part of the Tx block.
  const TxBlockHeader& GetHeader() const noexcept { return m_header; }

  /// Returns the vector of MicroBlockInfo.
  const std::vector<MicroBlockInfo>& GetMicroBlockInfos() const noexcept {
    return m_mbInfos;
  }

  /// Equality comparison operator.
  bool operator==(const TxBlock& block) const;
};

std::ostream& operator<<(std::ostream& os, const TxBlock& t);

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCK_TXBLOCK_H_
