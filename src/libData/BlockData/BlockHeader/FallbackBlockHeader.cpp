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
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

FallbackBlockHeader::FallbackBlockHeader() : m_leaderConsensusId(0) {}

FallbackBlockHeader::FallbackBlockHeader(const vector<unsigned char>& src,
                                         unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(INFO, "Error. We failed to initialize FallbackBlockHeader.");
  }
}

FallbackBlockHeader::FallbackBlockHeader(
    const uint64_t& fallbackDSEpochNo, const uint64_t& fallbackEpochNo,
    const unsigned char fallbackState, const StateHash& stateRootHash,
    const uint32_t leaderConsensusId, const Peer& leaderNetworkInfo,
    const PubKey& leaderPubKey, const uint32_t shardId,
    const boost::multiprecision::uint256_t& timestamp)
    : m_fallbackDSEpochNo(fallbackDSEpochNo),
      m_fallbackEpochNo(fallbackEpochNo),
      m_fallbackState(fallbackState),
      m_hash{stateRootHash},
      m_leaderConsensusId(leaderConsensusId),
      m_leaderNetworkInfo(leaderNetworkInfo),
      m_leaderPubKey(leaderPubKey),
      m_shardId(shardId),
      m_timestamp(timestamp) {}

unsigned int FallbackBlockHeader::Serialize(vector<unsigned char>& dst,
                                            unsigned int offset) const {
  unsigned int size_needed = FallbackBlockHeader::SIZE;
  unsigned int size_remaining = dst.size() - offset;

  if (size_remaining < size_needed) {
    dst.resize(size_needed + offset);
  }

  unsigned int curOffset = offset;
  SetNumber<uint64_t>(dst, curOffset, m_fallbackDSEpochNo, sizeof(uint64_t));
  curOffset += sizeof(uint64_t);
  SetNumber<uint64_t>(dst, curOffset, m_fallbackEpochNo, sizeof(uint64_t));
  curOffset += sizeof(uint64_t);
  SetNumber<unsigned char>(dst, curOffset, m_fallbackState,
                           sizeof(unsigned char));
  curOffset += sizeof(unsigned char);
  curOffset = m_hash.Serialize(dst, curOffset);
  SetNumber<uint32_t>(dst, curOffset, m_leaderConsensusId, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  curOffset += m_leaderNetworkInfo.Serialize(dst, curOffset);
  curOffset += m_leaderPubKey.Serialize(dst, curOffset);
  SetNumber<uint32_t>(dst, curOffset, m_shardId, sizeof(uint32_t));
  curOffset += sizeof(uint32_t);
  SetNumber<uint256_t>(dst, curOffset, m_timestamp, UINT256_SIZE);
  curOffset += UINT256_SIZE;
  return size_needed;
}

int FallbackBlockHeader::Deserialize(const vector<unsigned char>& src,
                                     unsigned int offset) {
  unsigned int curOffset = offset;
  try {
    m_fallbackDSEpochNo = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    m_fallbackEpochNo = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    m_fallbackState =
        GetNumber<unsigned char>(src, curOffset, sizeof(unsigned char));
    curOffset += sizeof(unsigned char);
    if (m_hash.Deserialize(src, curOffset)) {
      LOG_GENERAL(WARNING, "We failed to extract TxBlockHeader::m_hash.");
      return -1;
    }
    curOffset += m_hash.size();
    m_leaderConsensusId = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    if (m_leaderNetworkInfo.Deserialize(src, curOffset) != 0) {
      LOG_GENERAL(WARNING,
                  "Error. We failed to deserialize m_leaderNetworkInfo.");
      return -1;
    }
    curOffset += IP_SIZE + PORT_SIZE;
    if (m_leaderPubKey.Deserialize(src, curOffset) != 0) {
      LOG_GENERAL(WARNING, "Error. We failed to deserialize m_leaderPubKey.");
      return -1;
    }
    curOffset += PUB_KEY_SIZE;

    m_shardId = GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    m_timestamp = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
    curOffset += UINT256_SIZE;
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING, "ERROR: Error with VCBlockHeader::Deserialize."
                             << ' ' << e.what());
    return -1;
  }

  return 0;
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
  return m_hash.m_stateRootHash;
}

uint32_t FallbackBlockHeader::GetLeaderConsensusId() const {
  return m_leaderConsensusId;
}

const Peer& FallbackBlockHeader::GetLeaderNetworkInfo() const {
  return m_leaderNetworkInfo;
}

const PubKey& FallbackBlockHeader::GetLeaderPubKey() const {
  return m_leaderPubKey;
}

uint32_t FallbackBlockHeader::GetShardId() const { return m_shardId; }

const boost::multiprecision::uint256_t& FallbackBlockHeader::GetTimeStamp()
    const {
  return m_timestamp;
}

bool FallbackBlockHeader::operator==(const FallbackBlockHeader& header) const {
  return std::tie(m_fallbackEpochNo, m_fallbackDSEpochNo, m_fallbackState,
                  m_hash, m_leaderConsensusId, m_shardId, m_timestamp) ==
         std::tie(header.m_fallbackEpochNo, header.m_fallbackDSEpochNo,
                  header.m_fallbackState, header.m_hash,
                  header.m_leaderConsensusId, header.m_shardId,
                  header.m_timestamp);
}

bool FallbackBlockHeader::operator<(const FallbackBlockHeader& header) const {
  return std::tie(header.m_fallbackEpochNo, header.m_fallbackDSEpochNo,
                  header.m_fallbackState, header.m_hash,
                  header.m_leaderConsensusId, header.m_shardId,
                  header.m_timestamp) >
         std::tie(m_fallbackEpochNo, m_fallbackDSEpochNo, m_fallbackState,
                  m_hash, m_leaderConsensusId, m_shardId, m_timestamp);
}

bool FallbackBlockHeader::operator>(const FallbackBlockHeader& header) const {
  return header < *this;
}