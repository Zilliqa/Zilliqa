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
#include "Serialization.h"
#include "libCrypto/Sha2.h"

using namespace std;

namespace {

bool SetDSBlockHeader(zbytes& dst, unsigned int offset,
                      const DSBlockHeader& dsBlockHeader,
                      bool concreteVarsOnly = false) {
  ZilliqaMessage::ProtoDSBlock::DSBlockHeader result;

  io::DSBlockHeaderToProtobuf(dsBlockHeader, result, concreteVarsOnly);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <std::ranges::contiguous_range RangeT>
bool GetDSBlockHeader(RangeT&& src, unsigned int offset,
                      DSBlockHeader& dsBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoDSBlock::DSBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed");
    return false;
  }

  return io::ProtobufToDSBlockHeader(result, dsBlockHeader);
}

}  // namespace

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
  if (!SetDSBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetDSBlockHeader failed.");
    return false;
  }

  return true;
}

BlockHash DSBlockHeader::GetHashForRandom() const {
  SHA256Calculator sha2;
  zbytes vec;

  if (!SetDSBlockHeader(vec, 0, *this, true)) {
    LOG_GENERAL(WARNING, "SetDSBlockHeader failed.");
    return BlockHash();
  }

  sha2.Update(vec);
  const zbytes& resVec = sha2.Finalize();
  BlockHash blockHash;
  std::copy(resVec.begin(), resVec.end(), blockHash.asArray().begin());
  return blockHash;
}

bool DSBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  return GetDSBlockHeader(src, offset, *this);
}

bool DSBlockHeader::Deserialize(const string& src, unsigned int offset) {
  return GetDSBlockHeader(src, offset, *this);
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

std::ostream& operator<<(std::ostream& os, const DSBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<DSBlockHeader>" << std::endl
     << " DSDifficulty    = " << t.GetDSDifficulty() << std::endl
     << " Difficulty      = " << t.GetDifficulty() << std::endl
     << " TotalDifficulty = " << t.GetTotalDifficulty() << std::endl
     << " LeaderPubKey    = " << t.GetLeaderPubKey() << std::endl
     << " BlockNum        = " << t.GetBlockNum() << std::endl
     << " EpochNum        = " << t.GetEpochNum() << std::endl
     << " GasPrice        = " << t.GetGasPrice() << std::endl
     << t.m_hashset << std::endl
     << t.GetSWInfo() << std::endl;
  for (const auto& node : t.GetDSPoWWinners()) {
    os << " PoWDSWinner     = " << node.first << " " << node.second
       << std::endl;
  }
  for (const auto& pubkey : t.GetDSRemovePubKeys()) {
    os << " DSRemoved       = " << pubkey << std::endl;
  }

  return os;
}
