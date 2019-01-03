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

#ifndef __MICROBLOCKHEADER_H__
#define __MICROBLOCKHEADER_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"

/// Stores information on the header part of the microblock.
class MicroBlockHeader : public BlockHeaderBase {
  uint8_t m_type;  // 0: microblock proposed by a committee, 1: final tx block
  uint32_t m_version;
  uint32_t m_shardId;
  uint64_t m_gasLimit;
  uint64_t m_gasUsed;
  boost::multiprecision::uint128_t m_rewards;
  BlockHash m_prevHash;  // Hash of the previous block
  uint64_t m_epochNum;   // Epoch Num
  MicroBlockHashSet m_hashset;
  uint32_t m_numTxs;     // Total number of txs included in the block
  PubKey m_minerPubKey;  // Leader of the committee who proposed this block
  uint64_t
      m_dsBlockNum;  // DS Block index at the time this Tx Block was proposed

 public:
  /// Default constructor.
  MicroBlockHeader();

  /// Constructor for loading existing microblock header from a byte stream.
  MicroBlockHeader(const bytes& src, unsigned int offset);

  /// Constructor with predefined member values.
  MicroBlockHeader(const uint8_t type, const uint32_t version,
                   const uint32_t shardId, const uint64_t& gasLimit,
                   const uint64_t& gasUsed,
                   const boost::multiprecision::uint128_t& rewards,
                   const BlockHash& prevHash, const uint64_t& epochNum,
                   const MicroBlockHashSet& hashset, const uint32_t numTxs,
                   const PubKey& minerPubKey, const uint64_t& dsBlockNum,
                   const CommitteeHash& committeeHash);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  // [TODO] These methods are all supposed to be moved into BlockHeaderBase, so
  // no need to add Doxygen tags for now
  const uint8_t& GetType() const;
  const uint32_t& GetVersion() const;
  const uint32_t& GetShardId() const;
  const uint64_t& GetGasLimit() const;
  const uint64_t& GetGasUsed() const;
  const boost::multiprecision::uint128_t& GetRewards() const;
  const BlockHash& GetPrevHash() const;
  const uint64_t& GetEpochNum() const;
  const uint32_t& GetNumTxs() const;
  const PubKey& GetMinerPubKey() const;
  const uint64_t& GetDSBlockNum() const;
  const TxnHash& GetTxRootHash() const;
  const StateHash& GetStateDeltaHash() const;
  const TxnHash& GetTranReceiptHash() const;
  const MicroBlockHashSet& GetHashes() const;

  // Operators
  bool operator==(const MicroBlockHeader& header) const;
  bool operator<(const MicroBlockHeader& header) const;
  bool operator>(const MicroBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os, const MicroBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os, const MicroBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<MicroBlockHeader>" << std::endl
     << "m_type : " << t.m_type << std::endl
     << "m_version : " << t.m_version << std::endl
     << "m_shardId : " << t.m_shardId << std::endl
     << "m_gasLimit : " << t.m_gasLimit << std::endl
     << "m_rewards : " << t.m_rewards << std::endl
     << "m_prevHash : " << t.m_prevHash << std::endl
     << "m_epochNum : " << t.m_epochNum << std::endl
     << "m_numTxs : " << t.m_numTxs << std::endl
     << "m_minerPubKey : " << t.m_minerPubKey << std::endl
     << "m_dsBlockNum : " << t.m_dsBlockNum << std::endl
     << t.m_hashset;
  return os;
}

#endif  // __MICROBLOCKHEADER_H__
