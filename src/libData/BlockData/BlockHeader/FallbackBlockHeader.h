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

#ifndef __FALLBACKBLOCKHEADER_H__
#define __FALLBACKBLOCKHEADER_H__

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libNetwork/ShardStruct.h"

/// Stores information on the header part of the fallback block.
class FallbackBlockHeader : public BlockHeaderBase {
  uint64_t m_fallbackDSEpochNo;
  uint64_t m_fallbackEpochNo;
  unsigned char m_fallbackState;
  FallbackBlockHashSet m_hashset;
  uint16_t m_leaderConsensusId;
  Peer m_leaderNetworkInfo;
  PubKey m_leaderPubKey;
  uint32_t m_shardId;

 public:
  /// Default constructor.
  FallbackBlockHeader();  // creates a dummy invalid placeholder BlockHeader

  /// Constructor for loading fallback block header information from a byte
  /// stream.
  FallbackBlockHeader(const bytes& src, unsigned int offset);

  /// Constructor with specified fallback block header parameters.
  FallbackBlockHeader(const uint64_t& fallbackDSEpochNo,
                      const uint64_t& fallbackEpochNo,
                      const unsigned char fallbackState,
                      const FallbackBlockHashSet& hashset,
                      const uint16_t leaderConsensusId,
                      const Peer& leaderNetworkInfo, const PubKey& leaderPubKey,
                      const uint32_t shardId, const uint32_t version = 0,
                      const CommitteeHash& committeeHash = CommitteeHash(),
                      const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the DS Epoch number where view change happen
  const uint64_t& GetFallbackDSEpochNo() const;

  /// Returns the Epoch number where view change happen
  const uint64_t& GetFallbackEpochNo() const;

  /// Return the candidate leader ds state when view change happen
  unsigned char GetFallbackState() const;

  /// Returns the digest that represents the root of the Merkle tree that stores
  /// all state uptil this block.
  const StateHash& GetStateRootHash() const;

  /// Return the consensus Id of the leader
  uint16_t GetLeaderConsensusId() const;

  /// Return the IP and port of leader (at the point where fall back happen)
  const Peer& GetLeaderNetworkInfo() const;

  /// Return pub key of leader
  const PubKey& GetLeaderPubKey() const;

  /// Return the shard id where fallback happens
  uint32_t GetShardId() const;

  /// Equality operator.
  bool operator==(const FallbackBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const FallbackBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const FallbackBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os,
                                  const FallbackBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os,
                                const FallbackBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<FallbackBlockHeader>" << std::endl
     << " m_fallbackDSEpochNo = " << t.m_fallbackDSEpochNo << std::endl
     << " m_fallbackEpochNo   = " << t.m_fallbackEpochNo << std::endl
     << " m_fallbackState     = " << t.m_fallbackState << std::endl
     << " m_leaderConsensusId = " << t.m_leaderConsensusId << std::endl
     << " m_leaderNetworkInfo = " << t.m_leaderNetworkInfo << std::endl
     << " m_leaderPubKey      = " << t.m_leaderPubKey << std::endl
     << " m_shardId           = " << t.m_shardId << std::endl
     << t.m_hashset;
  return os;
}

#endif  // __FALLBACKBLOCKHEADER_H__
