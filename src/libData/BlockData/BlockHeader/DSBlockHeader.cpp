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

#include "DSBlockHeader.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;

DSBlockHeader::DSBlockHeader() { m_blockNum = (uint64_t)-1; }

DSBlockHeader::DSBlockHeader(const vector<unsigned char>& src,
                             unsigned int offset) {
  if (Deserialize(src, offset) != 0) {
    LOG_GENERAL(WARNING, "We failed to init DSBlockHeader.");
  }
}

DSBlockHeader::DSBlockHeader(const uint8_t dsDifficulty,
                             const uint8_t difficulty,
                             const BlockHash& prevHash,
                             const PubKey& leaderPubKey,
                             const uint64_t& blockNum,
                             const uint256_t& timestamp, const SWInfo& swInfo,
                             const map<PubKey, Peer>& powDSWinners)
    : m_dsDifficulty(dsDifficulty),
      m_difficulty(difficulty),
      m_prevHash(prevHash),
      m_leaderPubKey(leaderPubKey),
      m_blockNum(blockNum),
      m_timestamp(timestamp),
      m_swInfo(swInfo),
      m_PoWDSWinners(powDSWinners) {}

unsigned int DSBlockHeader::Serialize(vector<unsigned char>& dst,
                                      unsigned int offset) const {
  LOG_MARKER();

  unsigned int size_remaining = dst.size() - offset;
  if (size_remaining < GetSize()) {
    dst.resize(GetSize() + offset);
  }

  unsigned int curOffset = offset;

  SetNumber<uint8_t>(dst, curOffset, m_dsDifficulty, sizeof(uint8_t));
  curOffset += sizeof(uint8_t);
  SetNumber<uint8_t>(dst, curOffset, m_difficulty, sizeof(uint8_t));
  curOffset += sizeof(uint8_t);
  copy(m_prevHash.asArray().begin(), m_prevHash.asArray().end(),
       dst.begin() + curOffset);
  curOffset += BLOCK_HASH_SIZE;
  m_leaderPubKey.Serialize(dst, curOffset);
  curOffset += PUB_KEY_SIZE;
  SetNumber<uint64_t>(dst, curOffset, m_blockNum, sizeof(uint64_t));
  curOffset += sizeof(uint64_t);
  SetNumber<uint256_t>(dst, curOffset, m_timestamp, UINT256_SIZE);
  curOffset += UINT256_SIZE;
  curOffset += m_swInfo.Serialize(dst, curOffset);
  SetNumber<uint32_t>(dst, curOffset, m_PoWDSWinners.size(), sizeof(uint32_t));
  curOffset += sizeof(uint32_t);

  for (const auto& DSWinner : m_PoWDSWinners) {
    // Pubkey
    DSWinner.first.Serialize(dst, curOffset);
    curOffset += PUB_KEY_SIZE;
    // IP address
    Serializable::SetNumber<uint128_t>(
        dst, curOffset, DSWinner.second.m_ipAddress, UINT128_SIZE);
    curOffset += UINT128_SIZE;
    // Port
    Serializable::SetNumber<uint32_t>(
        dst, curOffset, DSWinner.second.m_listenPortHost, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
  }
  return GetSize();
}

int DSBlockHeader::Deserialize(const vector<unsigned char>& src,
                               unsigned int offset) {
  LOG_MARKER();
  PubKey deserializedPubKey;
  Peer deserializedPeer;
  uint128_t deserializedIP;
  uint32_t deserializedPort;
  unsigned int curOffset = offset;
  try {
    m_dsDifficulty = GetNumber<uint8_t>(src, curOffset, sizeof(uint8_t));
    curOffset += sizeof(uint8_t);
    m_difficulty = GetNumber<uint8_t>(src, curOffset, sizeof(uint8_t));
    curOffset += sizeof(uint8_t);
    copy(src.begin() + curOffset, src.begin() + curOffset + BLOCK_HASH_SIZE,
         m_prevHash.asArray().begin());
    curOffset += BLOCK_HASH_SIZE;
    if (m_leaderPubKey.Deserialize(src, curOffset) != 0) {
      LOG_GENERAL(WARNING, "We failed to init m_leaderPubKey.");
      return -1;
    }
    curOffset += PUB_KEY_SIZE;
    m_blockNum = GetNumber<uint64_t>(src, curOffset, sizeof(uint64_t));
    curOffset += sizeof(uint64_t);
    m_timestamp = GetNumber<uint256_t>(src, curOffset, UINT256_SIZE);
    curOffset += UINT256_SIZE;
    if (m_swInfo.Deserialize(src, curOffset) != 0) {
      LOG_GENERAL(WARNING, "We failed to init m_swInfo.");
      return -1;
    }
    curOffset += SWInfo::SIZE;
    uint32_t numOfIncomingDSMem =
        GetNumber<uint32_t>(src, curOffset, sizeof(uint32_t));
    curOffset += sizeof(uint32_t);
    uint32_t expectedSizeOffset =
        curOffset + (numOfIncomingDSMem * (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE));
    if (expectedSizeOffset >= src.size()) {
      LOG_GENERAL(WARNING,
                  "payload size is too small to deserialize all incoming "
                  "ds members"
                      << expectedSizeOffset << " " << src.size());
      return -1;
    }

    for (uint32_t i = 0; i < numOfIncomingDSMem; i++) {
      deserializedPubKey.Deserialize(src, curOffset);
      curOffset += PUB_KEY_SIZE;
      deserializedIP = GetNumber<uint128_t>(src, curOffset, IP_SIZE);
      curOffset += IP_SIZE;
      deserializedPort = GetNumber<uint32_t>(src, curOffset, PORT_SIZE);
      curOffset += PORT_SIZE;
      m_PoWDSWinners[deserializedPubKey] =
          Peer(deserializedIP, deserializedPort);
    }
  } catch (const std::exception& e) {
    LOG_GENERAL(WARNING,
                "Error with DSBlockHeader::Deserialize." << ' ' << e.what());
    return -1;
  }
  return 0;
}

const uint8_t& DSBlockHeader::GetDSDifficulty() const { return m_dsDifficulty; }

const uint8_t& DSBlockHeader::GetDifficulty() const { return m_difficulty; }

const BlockHash& DSBlockHeader::GetPrevHash() const { return m_prevHash; }

const PubKey& DSBlockHeader::GetLeaderPubKey() const { return m_leaderPubKey; }

const uint64_t& DSBlockHeader::GetBlockNum() const { return m_blockNum; }

uint32_t DSBlockHeader::GetSize() const {
  uint32_t dsBlockSize1 = sizeof(uint8_t) + sizeof(uint8_t) + BLOCK_HASH_SIZE +
                          PUB_KEY_SIZE + sizeof(uint64_t) + UINT256_SIZE +
                          SWInfo::SIZE + sizeof(uint32_t);

  dsBlockSize1 += m_PoWDSWinners.size() * (PUB_KEY_SIZE + IP_SIZE + PORT_SIZE);
  return dsBlockSize1;
}

const uint256_t& DSBlockHeader::GetTimestamp() const { return m_timestamp; }

const SWInfo& DSBlockHeader::GetSWInfo() const { return m_swInfo; }

const map<PubKey, Peer>& DSBlockHeader::GetDSPoWWinners() const {
  return m_PoWDSWinners;
}

bool DSBlockHeader::operator==(const DSBlockHeader& header) const {
  return tie(m_dsDifficulty, m_difficulty, m_prevHash, m_leaderPubKey,
             m_blockNum, m_timestamp, m_swInfo, m_PoWDSWinners) ==
         tie(header.m_dsDifficulty, header.m_difficulty, header.m_prevHash,
             header.m_leaderPubKey, header.m_blockNum, header.m_timestamp,
             header.m_swInfo, header.m_PoWDSWinners);
}

bool DSBlockHeader::operator<(const DSBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool DSBlockHeader::operator>(const DSBlockHeader& header) const {
  return header < *this;
}
