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
#include "libMessage/MessengerCommon.h"
#include "libMessage/ZilliqaMessage.pb.h"

using namespace std;
using namespace boost::multiprecision;

namespace {

bool CheckRequiredFieldsProtoBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& /*protoBlockHeaderBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockHeaderBase.has_version() &&
         protoBlockHeaderBase.has_committeehash() &&
         protoBlockHeaderBase.has_prevhash();
#endif
  return true;
}

std::optional<std::tuple<uint32_t, CommitteeHash, BlockHash>>
ProtobufToBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase) {
  if (!CheckRequiredFieldsProtoBlockHeaderBase(protoBlockHeaderBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockHeaderBase failed");
    return std::nullopt;
  }

  // Deserialize the version
  uint32_t version = protoBlockHeaderBase.version();

  // base.SetVersion(version);

  // Deserialize committee hash
  CommitteeHash committeeHash;
  if (!Messenger::CopyWithSizeCheck(protoBlockHeaderBase.committeehash(),
                                    committeeHash.asArray())) {
    return std::nullopt;
  }
  // base.SetCommitteeHash(committeeHash);

  // Deserialize prev hash
  BlockHash prevHash;
  if (!Messenger::CopyWithSizeCheck(protoBlockHeaderBase.prevhash(),
                                    prevHash.asArray())) {
    return std::nullopt;
  }
  // base.SetPrevHash(prevHash);

  return std::make_tuple(version, committeeHash, prevHash);
}

bool CheckRequiredFieldsProtoDSBlockDSBlockHeader(
    const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& /*protoDSBlockHeader*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member dswinners
  // Don't need to enforce check on optional members dsdifficulty, difficulty,
  // and gasprice
  return protoDSBlockHeader.has_leaderpubkey() &&
         protoDSBlockHeader.has_blocknum() &&
         protoDSBlockHeader.has_epochnum() && protoDSBlockHeader.has_swinfo() &&
         protoDSBlockHeader.has_hash() &&
         protoDSBlockHeader.has_blockheaderbase() &&
         CheckRequiredFieldsProtoDSBlockDSBlockHashSet(
             protoDSBlockHeader.hash());
#endif
  return true;
}

bool CheckRequiredFieldsProtoDSBlockPowDSWinner(
    const ZilliqaMessage::ProtoDSBlock::DSBlockHeader::
        PowDSWinners& /*powDSWinner*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return powDSWinner.has_key() && powDSWinner.has_val();
#endif
  return true;
}

bool ProtobufToDSBlockHeader(
    const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
    DSBlockHeader& dsBlockHeader) {
  if (!CheckRequiredFieldsProtoDSBlockDSBlockHeader(protoDSBlockHeader)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlockDSBlockHeader failed");
    return false;
  }

  PubKey leaderPubKey;
  SWInfo swInfo;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoDSBlockHeader.leaderpubkey(),
                                  leaderPubKey);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoDSBlockHeader.swinfo(), swInfo);

  // Deserialize powDSWinners
  map<PubKey, Peer> powDSWinners;
  PubKey tempPubKey;
  Peer tempWinnerNetworkInfo;
  for (const auto& dswinner : protoDSBlockHeader.dswinners()) {
    if (!CheckRequiredFieldsProtoDSBlockPowDSWinner(dswinner)) {
      LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlockPowDSWinner failed");
      return false;
    }
    PROTOBUFBYTEARRAYTOSERIALIZABLE(dswinner.key(), tempPubKey);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(dswinner.val(), tempWinnerNetworkInfo);
    powDSWinners[tempPubKey] = tempWinnerNetworkInfo;
  }

  GovDSShardVotesMap govProposalMap;
  for (const auto& protoProposal : protoDSBlockHeader.proposals()) {
    std::map<uint32_t, uint32_t> dsVotes;
    std::map<uint32_t, uint32_t> shardVotes;
    for (const auto& protovote : protoProposal.dsvotes()) {
      dsVotes[protovote.value()] = protovote.count();
    }
    for (const auto& protovote : protoProposal.minervotes()) {
      shardVotes[protovote.value()] = protovote.count();
    }
    govProposalMap[protoProposal.proposalid()].first = dsVotes;
    govProposalMap[protoProposal.proposalid()].second = shardVotes;
  }

  // Deserialize removeDSNodePubkeys
  std::vector<PubKey> removeDSNodePubKeys;
  PubKey tempRemovePubKey;
  for (const auto& removenode : protoDSBlockHeader.dsremoved()) {
    PROTOBUFBYTEARRAYTOSERIALIZABLE(removenode, tempRemovePubKey);
    removeDSNodePubKeys.emplace_back(tempRemovePubKey);
  }

  // Deserialize DSBlockHashSet
  DSBlockHashSet hash;
  const ZilliqaMessage::ProtoDSBlock::DSBlockHashSet& protoDSBlockHeaderHash =
      protoDSBlockHeader.hash();

  if (!Messenger::CopyWithSizeCheck(protoDSBlockHeaderHash.shardinghash(),
                                    hash.m_shardingHash.asArray())) {
    return false;
  }

  copy(protoDSBlockHeaderHash.reservedfield().begin(),
       protoDSBlockHeaderHash.reservedfield().begin() +
           min((unsigned int)protoDSBlockHeaderHash.reservedfield().size(),
               (unsigned int)hash.m_reservedField.size()),
       hash.m_reservedField.begin());

  // Generate the new DSBlock

  const uint8_t dsdifficulty = protoDSBlockHeader.dsdifficulty();
  const uint8_t difficulty = protoDSBlockHeader.difficulty();
  uint128_t gasprice = 0;

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoDSBlockHeader.gasprice(), gasprice);

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoDSBlockHeader.blockheaderbase();

  auto blockHeaderBaseVars = ProtobufToBlockHeaderBase(protoBlockHeaderBase);
  if (!blockHeaderBaseVars) return false;

  auto [version, committeeHash, prevHash] = *blockHeaderBaseVars;

  dsBlockHeader = DSBlockHeader(
      dsdifficulty, difficulty, leaderPubKey, protoDSBlockHeader.blocknum(),
      protoDSBlockHeader.epochnum(), gasprice, swInfo, powDSWinners,
      removeDSNodePubKeys, hash, govProposalMap, version, committeeHash,
      prevHash);

  return true;
}

template <std::ranges::contiguous_range RangeT>
bool GetDSBlockHeader(RangeT&& src, const unsigned int offset,
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

  return ProtobufToDSBlockHeader(result, dsBlockHeader);
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
  if (!GetDSBlockHeader(src, offset, *this)) {
    LOG_GENERAL(WARNING, "Messenger::GetDSBlockHeader failed.");
    return false;
  }

  return true;
}

bool DSBlockHeader::Deserialize(const string& src, unsigned int offset) {
  if (!GetDSBlockHeader(src, offset, *this)) {
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
