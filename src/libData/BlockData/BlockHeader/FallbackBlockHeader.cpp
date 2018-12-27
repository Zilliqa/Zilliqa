/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
      m_shardId(0),
      m_prevHash() {}

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
    const PubKey& leaderPubKey, const uint32_t shardId,
    const CommitteeHash& committeeHash, const BlockHash& prevHash)
    : BlockHeaderBase(committeeHash),
      m_fallbackDSEpochNo(fallbackDSEpochNo),
      m_fallbackEpochNo(fallbackEpochNo),
      m_fallbackState(fallbackState),
      m_hashset(hashset),
      m_leaderConsensusId(leaderConsensusId),
      m_leaderNetworkInfo(leaderNetworkInfo),
      m_leaderPubKey(leaderPubKey),
      m_shardId(shardId),
      m_prevHash(prevHash) {}

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
  return std::tie(m_fallbackEpochNo, m_fallbackDSEpochNo, m_fallbackState,
                  m_hashset, m_leaderConsensusId, m_shardId) ==
         std::tie(header.m_fallbackEpochNo, header.m_fallbackDSEpochNo,
                  header.m_fallbackState, header.m_hashset,
                  header.m_leaderConsensusId, header.m_shardId);
}

bool FallbackBlockHeader::operator<(const FallbackBlockHeader& header) const {
  return std::tie(header.m_fallbackEpochNo, header.m_fallbackDSEpochNo,
                  header.m_fallbackState, header.m_hashset,
                  header.m_leaderConsensusId, header.m_shardId) >
         std::tie(m_fallbackEpochNo, m_fallbackDSEpochNo, m_fallbackState,
                  m_hashset, m_leaderConsensusId, m_shardId);
}

bool FallbackBlockHeader::operator>(const FallbackBlockHeader& header) const {
  return header < *this;
}
