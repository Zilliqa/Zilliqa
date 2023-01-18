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

#include "MicroBlockHeader.h"
#include "Serialization.h"

using namespace std;

namespace {

bool SetMicroBlockHeader(zbytes& dst, const unsigned int offset,
                         const MicroBlockHeader& microBlockHeader) {
  ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader result;

  io::MicroBlockHeaderToProtobuf(microBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoMicroBlock::MicroBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <std::ranges::contiguous_range RangeT>
bool GetMicroBlockHeader(RangeT&& src, unsigned int offset,
                         MicroBlockHeader& microBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoMicroBlock::MicroBlockHeader initialization failed");
    return false;
  }

  return io::ProtobufToMicroBlockHeader(result, microBlockHeader);
}

}  // namespace

MicroBlockHeader::MicroBlockHeader(uint32_t shardId, uint64_t gasLimit,
                                   uint64_t gasUsed, const uint128_t& rewards,
                                   uint64_t epochNum,
                                   const MicroBlockHashSet& hashset,
                                   uint32_t numTxs, const PubKey& minerPubKey,
                                   uint64_t dsBlockNum, uint32_t version,
                                   const CommitteeHash& committeeHash,
                                   const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_shardId(shardId),
      m_gasLimit(gasLimit),
      m_gasUsed(gasUsed),
      m_rewards(rewards),
      m_epochNum(epochNum),
      m_hashset(hashset),
      m_numTxs(numTxs),
      m_minerPubKey(minerPubKey),
      m_dsBlockNum(dsBlockNum) {}

bool MicroBlockHeader::Serialize(zbytes& dst, unsigned int offset) const {
  if (!SetMicroBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetMicroBlockHeader failed.");
    return false;
  }

  return true;
}

bool MicroBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  return GetMicroBlockHeader(src, offset, *this);
}

bool MicroBlockHeader::Deserialize(const string& src, unsigned int offset) {
  return GetMicroBlockHeader(src, offset, *this);
}

bool MicroBlockHeader::operator==(const MicroBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_shardId, m_gasLimit, m_gasUsed, m_rewards, m_epochNum,
                   m_hashset, m_numTxs, m_minerPubKey, m_dsBlockNum) ==
          std::tie(header.m_shardId, header.m_gasLimit, header.m_gasUsed,
                   header.m_rewards, header.m_epochNum, header.m_hashset,
                   header.m_numTxs, header.m_minerPubKey, header.m_dsBlockNum));
}

std::ostream& operator<<(std::ostream& os, const MicroBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<MicroBlockHeader>" << std::endl
     << " ShardId     = " << t.GetShardId() << std::endl
     << " GasLimit    = " << t.GetGasLimit() << std::endl
     << " GasUsed     = " << t.GetGasUsed() << std::endl
     << " Rewards     = " << t.GetRewards() << std::endl
     << " EpochNum    = " << t.GetEpochNum() << std::endl
     << " NumTxs      = " << t.GetNumTxs() << std::endl
     << " MinerPubKey = " << t.GetMinerPubKey() << std::endl
     << " DSBlockNum  = " << t.GetDSBlockNum() << std::endl
     << t.GetHashes();
  return os;
}
