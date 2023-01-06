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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_TXBLOCKHEADER_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_TXBLOCKHEADER_H_

#include <Schnorr.h>
#include "BlockHashSet.h"
#include "BlockHeaderBase.h"

/// Stores information on the header part of the Tx block.
class TxBlockHeader : public BlockHeaderBase {
  uint64_t m_gasLimit{};
  uint64_t m_gasUsed{};
  uint128_t m_rewards;
  uint64_t m_blockNum{};  // Block index, starting from 0 in the genesis block
  TxBlockHashSet m_hashset;
  uint32_t m_numTxs{};   // Total number of txs included in the block
  PubKey m_minerPubKey;  // Leader of the committee who proposed this block
  uint64_t
      m_dsBlockNum{};  // DS Block index at the time this Tx Block was proposed

 public:
  /// Default constructor.
  TxBlockHeader(uint64_t gasLimit = 0, uint64_t gasUsed = 0,
                const uint128_t& rewards = 0,
                uint64_t blockNum = INIT_BLOCK_NUMBER,
                const TxBlockHashSet& blockHashSet = {}, uint32_t numTxs = 0,
                const PubKey& minerPubKey = {},
                uint64_t dsBlockNum = INIT_BLOCK_NUMBER, uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

  /// Returns the current limit for gas expenditure per block.
  uint64_t GetGasLimit() const { return m_gasLimit; }

  /// Returns the total gas used by transactions in this block.
  uint64_t GetGasUsed() const { return m_gasUsed; }

  /// Returns the rewards generated in this block. If normal epoch, then is the
  /// sum of txnFees from all microblock, if vacuous epoch, is the total rewards
  /// generated during coinbase
  const uint128_t& GetRewards() const { return m_rewards; }

  /// Returns the number of ancestor blocks.
  uint64_t GetBlockNum() const { return m_blockNum; }

  /// Returns the digest that represents the root of the Merkle tree that stores
  /// all state uptil this block.
  const StateHash& GetStateRootHash() const {
    return m_hashset.m_stateRootHash;
  }

  /// Returns the digest that represents the hash of state delta attached to
  /// finalblock.
  const StateHash& GetStateDeltaHash() const {
    return m_hashset.m_stateDeltaHash;
  }

  /// Returns the digest that represents the hash of all the extra micro block
  /// information in the finalblock.
  const MBInfoHash& GetMbInfoHash() const { return m_hashset.m_mbInfoHash; }

  /// Returns the number of transactions in this block.
  uint32_t GetNumTxs() const { return m_numTxs; }

  /// Returns the public key of the leader of the committee that composed this
  /// block.
  const PubKey& GetMinerPubKey() const { return m_minerPubKey; }

  /// Returns the parent DS block number.
  uint64_t GetDSBlockNum() const { return m_dsBlockNum; }

  /// Equality comparison operator.
  bool operator==(const TxBlockHeader& header) const;

#if 0
  /// Less-than comparison operator.
  bool operator<(const TxBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const TxBlockHeader& header) const;
#endif

  friend std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t);
};

std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t);

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_TXBLOCKHEADER_H_
