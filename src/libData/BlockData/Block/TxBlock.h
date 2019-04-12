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

#ifndef __TXBLOCK_H__
#define __TXBLOCK_H__

#include <array>

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#include "libNetwork/Peer.h"

struct MicroBlockInfo {
  BlockHash m_microBlockHash;
  TxnHash m_txnRootHash;
  uint32_t m_shardId;

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

  friend std::ostream& operator<<(std::ostream& os, const MicroBlockInfo& t);
};

inline std::ostream& operator<<(std::ostream& os, const MicroBlockInfo& t) {
  os << "<MicroBlockInfo>" << std::endl
     << " t.m_microBlockHash = " << t.m_microBlockHash << std::endl
     << " t.m_txnRootHash    = " << t.m_txnRootHash << std::endl
     << " t.m_shardId        = " << t.m_shardId;
  return os;
}

/// Stores the Tx block header and signature.

class TxBlock : public BlockBase {
  TxBlockHeader m_header;
  std::vector<MicroBlockInfo> m_mbInfos;

 public:
  /// Default constructor.
  TxBlock();  // creates a dummy invalid placeholder block -- blocknum is
              // maxsize of uint256

  /// Constructor for loading Tx block information from a byte stream.
  TxBlock(const bytes& src, unsigned int offset);

  /// Constructor with specified Tx block parameters.
  TxBlock(const TxBlockHeader& header,
          const std::vector<MicroBlockInfo>& mbInfos, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the reference to the TxBlockHeader part of the Tx block.
  const TxBlockHeader& GetHeader() const;

  /// Returns the vector of MicroBlockInfo.
  const std::vector<MicroBlockInfo>& GetMicroBlockInfos() const;

  /// Equality comparison operator.
  bool operator==(const TxBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const TxBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const TxBlock& block) const;

  friend std::ostream& operator<<(std::ostream& os, const TxBlock& t);
};

inline std::ostream& operator<<(std::ostream& os, const TxBlock& t) {
  const BlockBase& blockBase(t);

  os << "<TxBlock>" << std::endl
     << blockBase << std::endl
     << t.m_header << std::endl;

  for (const auto& info : t.m_mbInfos) {
    os << info << std::endl;
  }

  return os;
}

#endif  // __TXBLOCK_H__
