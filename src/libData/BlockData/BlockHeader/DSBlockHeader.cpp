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

#include "DSBlockHeader.h"
#include "libCrypto/Sha2.h"
#include "libMessage/Messenger.h"

using namespace std;
using namespace boost::multiprecision;

DSBlockHeader::DSBlockHeader(
    uint8_t dsDifficulty, uint8_t difficulty, const PubKey& leaderPubKey,
    uint64_t blockNum, uint64_t epochNum, const uint128_t& gasPrice,
    const SWInfo& swInfo, const map<PubKey, Peer>& powDSWinners,
    const std::vector<PubKey>& removeDSNodePubkeys,
    const DSBlockHashSet& hashset, const GovDSShardVotesMap& govProposalMap,
    uint32_t version, const CommitteeHash& committeeHash,
    const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_dsDifficulty(dsDifficulty),
      m_difficulty(difficulty),
      m_leaderPubKey(leaderPubKey),
      m_blockNum(blockNum),
      m_epochNum(epochNum),
      m_gasPrice(gasPrice),
      m_swInfo(swInfo),
      m_PoWDSWinners(powDSWinners),
      m_removeDSNodePubkeys(removeDSNodePubkeys),
      m_hashset(hashset),
      m_govProposalMap(govProposalMap) {}

bool DSBlockHeader::Serialize(zbytes& dst, unsigned int offset) const {
  if (!Messenger::SetDSBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return false;
  }

  return true;
}

BlockHash DSBlockHeader::GetHashForRandom() const {
  SHA256Calculator sha2;
  zbytes vec;

  if (!Messenger::SetDSBlockHeader(vec, 0, *this, true)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return BlockHash();
  }

  sha2.Update(vec);
  const zbytes& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

bool DSBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  if (!Messenger::GetDSBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}

bool DSBlockHeader::Deserialize(const string& src, unsigned int offset) {
  if (!Messenger::GetDSBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}

bool DSBlockHeader::operator==(const DSBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_dsDifficulty, m_difficulty, m_leaderPubKey, m_blockNum,
                   m_gasPrice, m_swInfo, m_PoWDSWinners,
                   m_removeDSNodePubkeys) ==
          std::tie(header.m_dsDifficulty, header.m_difficulty,
                   header.m_leaderPubKey, header.m_blockNum, header.m_gasPrice,
                   header.m_swInfo, header.m_PoWDSWinners,
                   header.m_removeDSNodePubkeys));
}

bool DSBlockHeader::operator<(const DSBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool DSBlockHeader::operator>(const DSBlockHeader& header) const {
  return header < *this;
}
