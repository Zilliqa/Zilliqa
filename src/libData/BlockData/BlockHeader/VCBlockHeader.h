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

#ifndef __VCBLOCKHEADER_H__
#define __VCBLOCKHEADER_H__

#include <array>
#include "common/BaseType.h"

#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libNetwork/Peer.h"
#include "libNetwork/ShardStruct.h"

/// Stores information on the header part of the VC block.
class VCBlockHeader : public BlockHeaderBase {
  uint64_t m_VieWChangeDSEpochNo;
  uint64_t m_VieWChangeEpochNo;
  unsigned char m_ViewChangeState;
  Peer m_CandidateLeaderNetworkInfo;
  PubKey m_CandidateLeaderPubKey;
  uint32_t m_VCCounter;
  VectorOfNode m_FaultyLeaders;

 public:
  /// Default constructor.
  VCBlockHeader();  // creates a dummy invalid placeholder BlockHeader --
                    // blocknum is maxsize of uint256

  /// Constructor for loading VC block header information from a byte stream.
  VCBlockHeader(const bytes& src, unsigned int offset);

  /// Constructor with specified VC block header parameters.
  VCBlockHeader(const uint64_t& vieWChangeDSEpochNo,
                const uint64_t& viewChangeEpochNo,
                const unsigned char viewChangeState,
                const Peer& candidateLeaderNetworkInfo,
                const PubKey& candidateLeaderPubKey, const uint32_t vcCounter,
                const VectorOfNode& faultyLeaders, const uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset);

  /// Returns the DS Epoch number where view change happen
  const uint64_t& GetViewChangeDSEpochNo() const;

  /// Returns the Epoch number (Total nums of final block) where view change
  /// happen
  const uint64_t& GetViewChangeEpochNo() const;

  /// Return the candidate leader ds state when view change happen
  unsigned char GetViewChangeState() const;

  /// Return the IP and port of candidate (at the point where view change
  /// happen)
  const Peer& GetCandidateLeaderNetworkInfo() const;

  /// Return pub key of candidate leader
  const PubKey& GetCandidateLeaderPubKey() const;

  /// Return the number of times view change has happened for the particular
  /// epoch and state
  uint32_t GetViewChangeCounter() const;

  /// Return all the faulty leaders in the current round of view change
  const VectorOfNode& GetFaultyLeaders() const;

  /// Equality operator.
  bool operator==(const VCBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const VCBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const VCBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os, const VCBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os, const VCBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<VCBlockHeader>" << std::endl
     << " m_VieWChangeDSEpochNo        = " << t.m_VieWChangeDSEpochNo
     << std::endl
     << " m_VieWChangeEpochNo          = " << t.m_VieWChangeEpochNo << std::endl
     << " m_ViewChangeState            = " << t.m_ViewChangeState << std::endl
     << " m_CandidateLeaderNetworkInfo = " << t.m_CandidateLeaderNetworkInfo
     << std::endl
     << " m_CandidateLeaderPubKey      = " << t.m_CandidateLeaderPubKey
     << std::endl
     << " m_VCCounter                  = " << t.m_VCCounter << std::endl;
  for (const auto& node : t.m_FaultyLeaders) {
    os << " FaultyLeader                 = " << node.first << " " << node.second
       << std::endl;
  }
  return os;
}

#endif  // __VCBLOCKHEADER_H__
