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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_MICROBLOCKHEADER_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_MICROBLOCKHEADER_H_

#include <Schnorr.h>
#include "BlockHashSet.h"
#include "BlockHeaderBase.h"

/// Stores information on the header part of the microblock.
class MicroBlockHeader final : public BlockHeaderBase {
  uint32_t m_shardId{};
  uint64_t m_gasLimit{};
  uint64_t m_gasUsed{};
  uint128_t m_rewards;
  uint64_t m_epochNum{};  // Epoch Num
  MicroBlockHashSet m_hashset;
  uint32_t m_numTxs{};   // Total number of txs included in the block
  PubKey m_minerPubKey;  // Leader of the committee who proposed this block
  uint64_t
      m_dsBlockNum{};  // DS Block index at the time this Tx Block was proposed

 public:
  /// Constructor with predefined member values.
  MicroBlockHeader(uint32_t shardId = 0, uint64_t gasLimit = 0,
                   uint64_t gasUsed = 0, const uint128_t& rewards = 0,
                   uint64_t epochNum = (uint64_t)-1,
                   const MicroBlockHashSet& hashset = {}, uint32_t numTxs = 0,
                   const PubKey& minerPubKey = {},
                   uint64_t dsBlockNum = INIT_BLOCK_NUMBER,
                   uint32_t version = 0,
                   const CommitteeHash& committeeHash = CommitteeHash(),
                   const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset);

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset);

  // [TODO] These methods are all supposed to be moved into BlockHeaderBase, so
  // no need to add Doxygen tags for now
  uint32_t GetShardId() const { return m_shardId; }
  uint64_t GetGasLimit() const { return m_gasLimit; }
  uint64_t GetGasUsed() const { return m_gasUsed; }
  const uint128_t& GetRewards() const { return m_rewards; }
  uint64_t GetEpochNum() const { return m_epochNum; }
  uint32_t GetNumTxs() const { return m_numTxs; }
  const PubKey& GetMinerPubKey() const { return m_minerPubKey; }
  uint64_t GetDSBlockNum() const { return m_dsBlockNum; }
  const TxnHash& GetTxRootHash() const { return m_hashset.m_txRootHash; }
  const StateHash& GetStateDeltaHash() const {
    return m_hashset.m_stateDeltaHash;
  }
  const TxnHash& GetTranReceiptHash() const {
    return m_hashset.m_tranReceiptHash;
  }
  const MicroBlockHashSet& GetHashes() const { return m_hashset; }

  // Operators
  bool operator==(const MicroBlockHeader& header) const;
};

std::ostream& operator<<(std::ostream& os, const MicroBlockHeader& t);

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_MICROBLOCKHEADER_H_
