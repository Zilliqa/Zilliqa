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

#ifndef __TXBLOCKHEADER_H__
#define __TXBLOCKHEADER_H__

#include <array>

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"

/// Stores information on the header part of the Tx block.
class TxBlockHeader : public BlockHeaderBase {
  uint64_t m_gasLimit;
  uint64_t m_gasUsed;
  uint128_t m_rewards;
  uint64_t m_blockNum;  // Block index, starting from 0 in the genesis block
  TxBlockHashSet m_hashset;
  uint32_t m_numTxs;     // Total number of txs included in the block
  PubKey m_minerPubKey;  // Leader of the committee who proposed this block
  uint64_t
      m_dsBlockNum;  // DS Block index at the time this Tx Block was proposed

 public:
  /// Default constructor.
  TxBlockHeader();

  /// Constructor for loading Tx block header information from a byte stream.
  TxBlockHeader(const bytes& src, unsigned int offset);

  /// Constructor with specified Tx block header parameters.
  TxBlockHeader(const uint64_t& gasLimit, const uint64_t& gasUsed,
                const uint128_t& rewards, const uint64_t& blockNum,
                const TxBlockHashSet& blockHashSet, const uint32_t numTxs,
                const PubKey& minerPubKey, const uint64_t& dsBlockNum,
                const uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

  /// Returns the current limit for gas expenditure per block.
  const uint64_t& GetGasLimit() const;

  /// Returns the total gas used by transactions in this block.
  const uint64_t& GetGasUsed() const;

  /// Returns the rewards generated in this block. If normal epoch, then is the
  /// sum of txnFees from all microblock, if vacuous epoch, is the total rewards
  /// generated during coinbase
  const uint128_t& GetRewards() const;

  /// Returns the number of ancestor blocks.
  const uint64_t& GetBlockNum() const;

  /// Returns the digest that represents the root of the Merkle tree that stores
  /// all state uptil this block.
  const StateHash& GetStateRootHash() const;

  /// Returns the digest that represents the hash of state delta attached to
  /// finalblock.
  const StateHash& GetStateDeltaHash() const;

  /// Returns the digest that represents the hash of all the extra micro block
  /// information in the finalblock.
  const MBInfoHash& GetMbInfoHash() const;

  /// Returns the number of transactions in this block.
  const uint32_t& GetNumTxs() const;

  /// Returns the public key of the leader of the committee that composed this
  /// block.
  const PubKey& GetMinerPubKey() const;

  /// Returns the parent DS block number.
  const uint64_t& GetDSBlockNum() const;

  /// Equality comparison operator.
  bool operator==(const TxBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const TxBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const TxBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<TxBlockHeader>" << std::endl
     << " m_gasLimit    = " << t.m_gasLimit << std::endl
     << " m_gasUsed     = " << t.m_gasUsed << std::endl
     << " m_rewards     = " << t.m_rewards << std::endl
     << " m_blockNum    = " << t.m_blockNum << std::endl
     << " m_numTxs      = " << t.m_numTxs << std::endl
     << " m_minerPubKey = " << t.m_minerPubKey << std::endl
     << " m_dsBlockNum  = " << t.m_dsBlockNum << std::endl
     << t.m_hashset;
  return os;
}

#endif  // __TXBLOCKHEADER_H__
