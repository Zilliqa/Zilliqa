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

#include "TxBlockHeader.h"
#include "Serialization.h"
#include "libMessage/Messenger.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

void TxBlockHeaderToProtobuf(
    const TxBlockHeader& txBlockHeader,
    ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoTxBlockHeader) {
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoTxBlockHeader.mutable_blockheaderbase();
  io::BlockHeaderBaseToProtobuf(txBlockHeader, *protoBlockHeaderBase);

  protoTxBlockHeader.set_gaslimit(txBlockHeader.GetGasLimit());
  protoTxBlockHeader.set_gasused(txBlockHeader.GetGasUsed());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      txBlockHeader.GetRewards(), *protoTxBlockHeader.mutable_rewards());
  protoTxBlockHeader.set_blocknum(txBlockHeader.GetBlockNum());

  ZilliqaMessage::ProtoTxBlock::TxBlockHashSet* protoHeaderHash =
      protoTxBlockHeader.mutable_hash();
  protoHeaderHash->set_stateroothash(txBlockHeader.GetStateRootHash().data(),
                                     txBlockHeader.GetStateRootHash().size);
  protoHeaderHash->set_statedeltahash(txBlockHeader.GetStateDeltaHash().data(),
                                      txBlockHeader.GetStateDeltaHash().size);
  protoHeaderHash->set_mbinfohash(txBlockHeader.GetMbInfoHash().data(),
                                  txBlockHeader.GetMbInfoHash().size);

  protoTxBlockHeader.set_numtxs(txBlockHeader.GetNumTxs());
  SerializableToProtobufByteArray(txBlockHeader.GetMinerPubKey(),
                                  *protoTxBlockHeader.mutable_minerpubkey());
  protoTxBlockHeader.set_dsblocknum(txBlockHeader.GetDSBlockNum());
}

bool SetTxBlockHeader(zbytes& dst, unsigned int offset,
                      const TxBlockHeader& txBlockHeader) {
  ZilliqaMessage::ProtoTxBlock::TxBlockHeader result;

  TxBlockHeaderToProtobuf(txBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

inline bool CheckRequiredFieldsProtoTxBlockTxBlockHeader(
    const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& /*protoTxBlockHeader*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoTxBlockHeader.has_gaslimit() &&
         protoTxBlockHeader.has_gasused() && protoTxBlockHeader.has_rewards() &&
         protoTxBlockHeader.has_blocknum() && protoTxBlockHeader.has_hash() &&
         protoTxBlockHeader.has_numtxs() &&
         protoTxBlockHeader.has_minerpubkey() &&
         protoTxBlockHeader.has_dsblocknum() &&
         protoTxBlockHeader.has_blockheaderbase() &&
         CheckRequiredFieldsProtoTxBlockTxBlockHashSet(
             protoTxBlockHeader.hash());
#endif
  return true;
}

bool ProtobufToTxBlockHeader(
    const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoTxBlockHeader,
    TxBlockHeader& txBlockHeader) {
  if (!CheckRequiredFieldsProtoTxBlockTxBlockHeader(protoTxBlockHeader)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTxBlockTxBlockHeader failed");
    return false;
  }

  uint64_t gasLimit;
  uint64_t gasUsed;
  uint128_t rewards;
  TxBlockHashSet hash;
  PubKey minerPubKey;

  gasLimit = protoTxBlockHeader.gaslimit();
  gasUsed = protoTxBlockHeader.gasused();
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxBlockHeader.rewards(), rewards);

  const ZilliqaMessage::ProtoTxBlock::TxBlockHashSet& protoTxBlockHeaderHash =
      protoTxBlockHeader.hash();
  copy(protoTxBlockHeaderHash.stateroothash().begin(),
       protoTxBlockHeaderHash.stateroothash().begin() +
           min((unsigned int)protoTxBlockHeaderHash.stateroothash().size(),
               (unsigned int)hash.m_stateRootHash.size),
       hash.m_stateRootHash.asArray().begin());
  copy(protoTxBlockHeaderHash.statedeltahash().begin(),
       protoTxBlockHeaderHash.statedeltahash().begin() +
           min((unsigned int)protoTxBlockHeaderHash.statedeltahash().size(),
               (unsigned int)hash.m_stateDeltaHash.size),
       hash.m_stateDeltaHash.asArray().begin());
  copy(protoTxBlockHeaderHash.mbinfohash().begin(),
       protoTxBlockHeaderHash.mbinfohash().begin() +
           min((unsigned int)protoTxBlockHeaderHash.mbinfohash().size(),
               (unsigned int)hash.m_mbInfoHash.size),
       hash.m_mbInfoHash.asArray().begin());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoTxBlockHeader.minerpubkey(),
                                  minerPubKey);

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoTxBlockHeader.blockheaderbase();

  auto blockHeaderBaseVars =
      io::ProtobufToBlockHeaderBase(protoBlockHeaderBase);
  if (!blockHeaderBaseVars) return false;

  const auto& [version, committeeHash, prevHash] = *blockHeaderBaseVars;

  txBlockHeader = TxBlockHeader(
      gasLimit, gasUsed, rewards, protoTxBlockHeader.blocknum(), hash,
      protoTxBlockHeader.numtxs(), minerPubKey, protoTxBlockHeader.dsblocknum(),
      version, committeeHash, prevHash);

  return true;
}

template <std::ranges::contiguous_range RangeT>
bool GetTxBlockHeader(RangeT&& src, unsigned int offset,
                      TxBlockHeader& txBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ZilliqaMessage::ProtoTxBlock::TxBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed");
    return false;
  }

  return ProtobufToTxBlockHeader(result, txBlockHeader);
}

}  // namespace

TxBlockHeader::TxBlockHeader(uint64_t gasLimit, uint64_t gasUsed,
                             const uint128_t& rewards, uint64_t blockNum,
                             const TxBlockHashSet& blockHashSet,
                             uint32_t numTxs, const PubKey& minerPubKey,
                             uint64_t dsBlockNum, uint32_t version,
                             const CommitteeHash& committeeHash,
                             const BlockHash& prevHash)
    : BlockHeaderBase(version, committeeHash, prevHash),
      m_gasLimit(gasLimit),
      m_gasUsed(gasUsed),
      m_rewards(rewards),
      m_blockNum(blockNum),
      m_hashset(blockHashSet),
      m_numTxs(numTxs),
      m_minerPubKey(minerPubKey),
      m_dsBlockNum(dsBlockNum) {}

bool TxBlockHeader::Serialize(zbytes& dst, unsigned int offset) const {
  if (!SetTxBlockHeader(dst, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::SetTxBlockHeader failed.");
    return false;
  }

  return true;
}

bool TxBlockHeader::Deserialize(const zbytes& src, unsigned int offset) {
  return GetTxBlockHeader(src, offset, *this);
}

bool TxBlockHeader::Deserialize(const string& src, unsigned int offset) {
  return GetTxBlockHeader(src, offset, *this);
}

bool TxBlockHeader::operator==(const TxBlockHeader& header) const {
  return BlockHeaderBase::operator==(header) &&
         (std::tie(m_gasLimit, m_gasUsed, m_rewards, m_blockNum, m_hashset,
                   m_numTxs, m_minerPubKey, m_dsBlockNum) ==
          std::tie(header.m_gasLimit, header.m_gasUsed, header.m_rewards,
                   header.m_blockNum, header.m_hashset, header.m_numTxs,
                   header.m_minerPubKey, header.m_dsBlockNum));
}

#if 0
bool TxBlockHeader::operator<(const TxBlockHeader& header) const {
  return m_blockNum < header.m_blockNum;
}

bool TxBlockHeader::operator>(const TxBlockHeader& header) const {
  return header < *this;
}
#endif

std::ostream& operator<<(std::ostream& os, const TxBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<TxBlockHeader>" << std::endl
     << " GasLimit    = " << t.GetGasLimit() << std::endl
     << " GasUsed     = " << t.GetGasUsed() << std::endl
     << " Rewards     = " << t.GetRewards() << std::endl
     << " BlockNum    = " << t.GetBlockNum() << std::endl
     << " NumTxs      = " << t.GetNumTxs() << std::endl
     << " MinerPubKey = " << t.GetMinerPubKey() << std::endl
     << " DSBlockNum  = " << t.GetDSBlockNum() << std::endl
     << t.m_hashset;
  return os;
}
