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

#include "FallbackBlockHeader.h"
#include "libMessage/Messenger.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

FallbackBlockHeader::FallbackBlockHeader()
    : BlockHeaderBase(),
      m_fallbackDSEpochNo((uint64_t)-1),
      m_fallbackEpochNo((uint64_t)-1),
      m_fallbackState(0),
      m_hashset(),
      m_leaderConsensusId(0),
      m_leaderNetworkInfo(),
      m_leaderPubKey(),
      m_shardId(0) {}

FallbackBlockHeader::FallbackBlockHeader(const bytes& src,
                                         unsigned int offset) {
  if (!Deserialize(src, offset)) {
    LOG_GENERAL(INFO, "Error. We failed to initialize FallbackBlockHeader.");
  }
}

FallbackBlockHeader::FallbackBlockHeader(
    const uint64_t& fallbackDSEpochNo, const uint64_t& fallbackEpochNo,
    const unsigned char fallbackState, const FallbackBlockHashSet& hashset,
    const uint16_t leaderConsensusId, const Peer& leaderNetworkInfo,
    const PubKey& leaderPubKey, const uint32_t shardId, const uint32_t version,
    const CommitteeHash& committeeHash, const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_fallbackDSEpochNo(fallbackDSEpochNo),
      m_fallbackEpochNo(fallbackEpochNo),
      m_fallbackState(fallbackState),
      m_hashset(hashset),
      m_leaderConsensusId(leaderConsensusId),
      m_leaderNetworkInfo(leaderNetworkInfo),
      m_leaderPubKey(leaderPubKey),
      m_shardId(shardId) {}

bool FallbackBlockHeader::Serialize(bytes& dst, unsigned int offset) const {
  if (!Messenger::SetFallbackBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetFallbackBlockHeader failed.");
    return false;
  }

  return true;
}

bool FallbackBlockHeader::Deserialize(const bytes& src, unsigned int offset) {
  if (!Messenger::GetFallbackBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetFallbackBlockHeader failed.");
    return false;
  }

  return true;
}

const uint64_t& FallbackBlockHeader::GetFallbackDSEpochNo() const {
  return m_fallbackDSEpochNo;
}

const uint64_t& FallbackBlockHeader::GetFallbackEpochNo() const {
  return m_fallbackEpochNo;
}

unsigned char FallbackBlockHeader::GetFallbackState() const {
  return m_fallbackState;
}

const StateHash& FallbackBlockHeader::GetStateRootHash() const {
  return m_hashset.m_stateRootHash;
}

uint16_t FallbackBlockHeader::GetLeaderConsensusId() const {
  return m_leaderConsensusId;
}

const Peer& FallbackBlockHeader::GetLeaderNetworkInfo() const {
  return m_leaderNetworkInfo;
}

const PubKey& FallbackBlockHeader::GetLeaderPubKey() const {
  return m_leaderPubKey;
}

uint32_t FallbackBlockHeader::GetShardId() const { return m_shardId; }

bool FallbackBlockHeader::operator==(const FallbackBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_fallbackEpochNo, m_fallbackDSEpochNo, m_fallbackState,
                   m_hashset, m_leaderConsensusId, m_shardId) ==
          std::tie(header.m_fallbackEpochNo, header.m_fallbackDSEpochNo,
                   header.m_fallbackState, header.m_hashset,
                   header.m_leaderConsensusId, header.m_shardId));
}

bool FallbackBlockHeader::operator<(const FallbackBlockHeader& header) const {
  // To compare, first they must be of identical epochno and state
  if (!(std::tie(m_version, m_fallbackEpochNo, m_fallbackDSEpochNo,
                 m_fallbackState) ==
        std::tie(header.m_version, header.m_fallbackEpochNo,
                 header.m_fallbackDSEpochNo, header.m_fallbackState))) {
    return false;
  }

  if (m_shardId == header.m_shardId) {
    return m_leaderConsensusId < header.m_leaderConsensusId;
  } else {
    return m_shardId < header.m_shardId;
  }
}

bool FallbackBlockHeader::operator>(const FallbackBlockHeader& header) const {
  return header < *this;
}
