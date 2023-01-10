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

#ifndef ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_VCBLOCKHEADER_H_
#define ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_VCBLOCKHEADER_H_

#include <Schnorr.h>
#include "BlockHeaderBase.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"

/// Stores information on the header part of the VC block.
class VCBlockHeader final : public BlockHeaderBase {
  uint64_t m_VieWChangeDSEpochNo{};
  uint64_t m_VieWChangeEpochNo{};
  unsigned char m_ViewChangeState{};
  Peer m_CandidateLeaderNetworkInfo;
  PubKey m_CandidateLeaderPubKey;
  uint32_t m_VCCounter{};
  VectorOfNode m_FaultyLeaders;

 public:
  /// Default constructor.
  VCBlockHeader(uint64_t vieWChangeDSEpochNo = (uint64_t)-1,
                uint64_t viewChangeEpochNo = (uint64_t)-1,
                unsigned char viewChangeState = 0,
                const Peer& candidateLeaderNetworkInfo = {},
                const PubKey& candidateLeaderPubKey = {},
                uint32_t vcCounter = 0, const VectorOfNode& faultyLeaders = {},
                const uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset);

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset);

  /// Returns the DS Epoch number where view change happen
  uint64_t GetViewChangeDSEpochNo() const { return m_VieWChangeDSEpochNo; }

  /// Returns the Epoch number (Total nums of final block) where view change
  /// happen
  uint64_t GetViewChangeEpochNo() const { return m_VieWChangeEpochNo; }

  /// Return the candidate leader ds state when view change happen
  unsigned char GetViewChangeState() const { return m_ViewChangeState; }

  /// Return the IP and port of candidate (at the point where view change
  /// happen)
  const Peer& GetCandidateLeaderNetworkInfo() const {
    return m_CandidateLeaderNetworkInfo;
  }

  /// Return pub key of candidate leader
  const PubKey& GetCandidateLeaderPubKey() const {
    return m_CandidateLeaderPubKey;
  }

  /// Return the number of times view change has happened for the particular
  /// epoch and state
  uint32_t GetViewChangeCounter() const { return m_VCCounter; }

  /// Return all the faulty leaders in the current round of view change
  const VectorOfNode& GetFaultyLeaders() const { return m_FaultyLeaders; }

  /// Equality operator.
  bool operator==(const VCBlockHeader& header) const;
};

std::ostream& operator<<(std::ostream& os, const VCBlockHeader& t);

#endif  // ZILLIQA_SRC_LIBDATA_BLOCKDATA_BLOCKHEADER_VCBLOCKHEADER_H_
