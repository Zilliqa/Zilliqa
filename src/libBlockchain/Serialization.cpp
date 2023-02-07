/*
 * Copyright (C) 2022 Zilliqa
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

#include "Serialization.h"
#include "DSBlock.h"
#include "MicroBlock.h"
#include "TxBlock.h"
#include "VCBlock.h"

namespace io {

namespace {

constexpr bool CheckRequiredFieldsProtoBlockHeaderBase(
    const ZilliqaMessage::ProtoBlockHeaderBase& /*protoBlockHeaderBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockHeaderBase.has_version() &&
         protoBlockHeaderBase.has_committeehash() &&
         protoBlockHeaderBase.has_prevhash();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoDSBlockPowDSWinner(
    const ZilliqaMessage::ProtoDSBlock::DSBlockHeader::
        PowDSWinners& /*powDSWinner*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return powDSWinner.has_key() && powDSWinner.has_val();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoDSBlockDSBlockHeader(
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

constexpr bool CheckRequiredFieldsProtoMicroBlockMicroBlockHeader(
    const ZilliqaMessage::ProtoMicroBlock::
        MicroBlockHeader& /*protoMicroBlockHeader*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoMicroBlockHeader.has_shardid() &&
         protoMicroBlockHeader.has_gaslimit() &&
         protoMicroBlockHeader.has_gasused() &&
         protoMicroBlockHeader.has_rewards() &&
         protoMicroBlockHeader.has_epochnum() &&
         protoMicroBlockHeader.has_txroothash() &&
         protoMicroBlockHeader.has_numtxs() &&
         protoMicroBlockHeader.has_minerpubkey() &&
         protoMicroBlockHeader.has_dsblocknum() &&
         protoMicroBlockHeader.has_statedeltahash() &&
         protoMicroBlockHeader.has_tranreceipthash() &&
         protoMicroBlockHeader.has_blockheaderbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoTxBlockTxBlockHeader(
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

void FaultyLeaderToProtobuf(
    const VectorOfNode& faultyLeaders,
    ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  for (const auto& node : faultyLeaders) {
    ZilliqaMessage::ProtoDSNode* protodsnode =
        protoVCBlockHeader.add_faultyleaders();
    SerializableToProtobufByteArray(node.first, *protodsnode->mutable_pubkey());
    SerializableToProtobufByteArray(node.second, *protodsnode->mutable_peer());
  }
}

constexpr bool CheckRequiredFieldsProtoVCBlockVCBlockHeader(
    const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& /*protoVCBlockHeader*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member faultyleaders
  return protoVCBlockHeader.has_viewchangedsepochno() &&
         protoVCBlockHeader.has_viewchangeepochno() &&
         protoVCBlockHeader.has_viewchangestate() &&
         protoVCBlockHeader.has_candidateleadernetworkinfo() &&
         protoVCBlockHeader.has_candidateleaderpubkey() &&
         protoVCBlockHeader.has_vccounter() &&
         protoVCBlockHeader.has_blockheaderbase();
#endif
  return true;
}

bool ProtobufToFaultyDSMembers(
    const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
    VectorOfNode& faultyDSMembers) {
  for (const auto& dsnode : protoVCBlockHeader.faultyleaders()) {
    PubKey pubkey;
    Peer peer;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(dsnode.pubkey(), pubkey);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(dsnode.peer(), peer);
    faultyDSMembers.emplace_back(pubkey, peer);
  }

  return true;
}

constexpr bool CheckRequiredFieldsProtoBlockBase(
    const ZilliqaMessage::ProtoBlockBase& /*protoBlockBase*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockBase.has_blockhash() && protoBlockBase.has_cosigs() &&
         protoBlockBase.has_timestamp() &&
         CheckRequiredFieldsProtoBlockBaseCoSignatures(protoBlockBase.cosigs());
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoDSBlock(
    const ZilliqaMessage::ProtoDSBlock& /*protoDSBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoDSBlock.has_header() && protoDSBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& /*protoMicroBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member tranhashes
  return protoMicroBlock.has_header() && protoMicroBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoTxBlock(
    const ZilliqaMessage::ProtoTxBlock& /*protoTxBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member mbinfos
  return protoTxBlock.has_header() && protoTxBlock.has_blockbase();
#endif
  return true;
}

constexpr bool CheckRequiredFieldsProtoMbInfo(
    const ZilliqaMessage::ProtoMbInfo& /*protoMbInfo*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoMbInfo.has_mbhash() && protoMbInfo.has_txroot() &&
         protoMbInfo.has_shardid();
#endif
  return true;
}

bool ProtobufToMbInfo(const ZilliqaMessage::ProtoMbInfo& ProtoMbInfo,
                      MicroBlockInfo& mbInfo) {
  if (!CheckRequiredFieldsProtoMbInfo(ProtoMbInfo)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMbInfo failed");
    return false;
  }

  copy(ProtoMbInfo.mbhash().begin(),
       ProtoMbInfo.mbhash().begin() +
           std::min((unsigned int)ProtoMbInfo.mbhash().size(),
                    (unsigned int)mbInfo.m_microBlockHash.size),
       mbInfo.m_microBlockHash.asArray().begin());
  copy(ProtoMbInfo.txroot().begin(),
       ProtoMbInfo.txroot().begin() +
           std::min((unsigned int)ProtoMbInfo.txroot().size(),
                    (unsigned int)mbInfo.m_txnRootHash.size),
       mbInfo.m_txnRootHash.asArray().begin());
  mbInfo.m_shardId = ProtoMbInfo.shardid();

  return true;
}

constexpr bool CheckRequiredFieldsProtoVCBlock(
    const ZilliqaMessage::ProtoVCBlock& /*protoVCBlock*/) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoVCBlock.has_header() && protoVCBlock.has_blockbase();
#endif
  return true;
}

}  // namespace

void BlockHeaderBaseToProtobuf(
    const BlockHeaderBase& base,
    ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase) {
  // version
  protoBlockHeaderBase.set_version(base.GetVersion());
  // committee hash
  protoBlockHeaderBase.set_committeehash(base.GetCommitteeHash().data(),
                                         base.GetCommitteeHash().size);
  protoBlockHeaderBase.set_prevhash(base.GetPrevHash().data(),
                                    base.GetPrevHash().size);
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

  // Deserialize committee hash
  CommitteeHash committeeHash;
  if (!CopyWithSizeCheck(protoBlockHeaderBase.committeehash(),
                         committeeHash.asArray())) {
    return std::nullopt;
  }

  // Deserialize prev hash
  BlockHash prevHash;
  if (!CopyWithSizeCheck(protoBlockHeaderBase.prevhash(), prevHash.asArray())) {
    return std::nullopt;
  }

  return std::make_tuple(version, committeeHash, prevHash);
}

void DSBlockHeaderToProtobuf(
    const DSBlockHeader& dsBlockHeader,
    ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
    bool concreteVarsOnly /*= false*/) {
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoDSBlockHeader.mutable_blockheaderbase();
  io::BlockHeaderBaseToProtobuf(dsBlockHeader, *protoBlockHeaderBase);

  if (!concreteVarsOnly) {
    protoDSBlockHeader.set_dsdifficulty(dsBlockHeader.GetDSDifficulty());
    protoDSBlockHeader.set_difficulty(dsBlockHeader.GetDifficulty());
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        dsBlockHeader.GetGasPrice(), *protoDSBlockHeader.mutable_gasprice());
    ZilliqaMessage::ProtoDSBlock::DSBlockHeader::PowDSWinners* powdswinner;

    for (const auto& winner : dsBlockHeader.GetDSPoWWinners()) {
      powdswinner = protoDSBlockHeader.add_dswinners();
      SerializableToProtobufByteArray(winner.first,
                                      *powdswinner->mutable_key());
      SerializableToProtobufByteArray(winner.second,
                                      *powdswinner->mutable_val());
    }

    ZilliqaMessage::ProtoDSBlock::DSBlockHeader::Proposal* protoproposal;
    for (const auto& govProposal : dsBlockHeader.GetGovProposalMap()) {
      protoproposal = protoDSBlockHeader.add_proposals();
      protoproposal->set_proposalid(govProposal.first);
      for (const auto& vote : govProposal.second.first) {
        ZilliqaMessage::ProtoDSBlock::DSBlockHeader::Vote* protoVote;
        protoVote = protoproposal->add_dsvotes();
        protoVote->set_value(vote.first);
        protoVote->set_count(vote.second);
      }
      for (const auto& vote : govProposal.second.second) {
        ZilliqaMessage::ProtoDSBlock::DSBlockHeader::Vote* protoVote;
        protoVote = protoproposal->add_minervotes();
        protoVote->set_value(vote.first);
        protoVote->set_count(vote.second);
      }
    }

    ZilliqaMessage::ByteArray* dsremoved;
    for (const auto& removedPubKey : dsBlockHeader.GetDSRemovePubKeys()) {
      dsremoved = protoDSBlockHeader.add_dsremoved();
      SerializableToProtobufByteArray(removedPubKey, *dsremoved);
    }
  }

  SerializableToProtobufByteArray(dsBlockHeader.GetLeaderPubKey(),
                                  *protoDSBlockHeader.mutable_leaderpubkey());

  protoDSBlockHeader.set_blocknum(dsBlockHeader.GetBlockNum());
  protoDSBlockHeader.set_epochnum(dsBlockHeader.GetEpochNum());
  SerializableToProtobufByteArray(dsBlockHeader.GetSWInfo(),
                                  *protoDSBlockHeader.mutable_swinfo());

  ZilliqaMessage::ProtoDSBlock::DSBlockHashSet* protoHeaderHash =
      protoDSBlockHeader.mutable_hash();
  protoHeaderHash->set_shardinghash(dsBlockHeader.GetShardingHash().data(),
                                    dsBlockHeader.GetShardingHash().size);
  protoHeaderHash->set_reservedfield(
      dsBlockHeader.GetHashSetReservedField().data(),
      dsBlockHeader.GetHashSetReservedField().size());
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
  std::map<PubKey, Peer> powDSWinners;
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

  if (!CopyWithSizeCheck(protoDSBlockHeaderHash.shardinghash(),
                         hash.m_shardingHash.asArray())) {
    return false;
  }

  copy(protoDSBlockHeaderHash.reservedfield().begin(),
       protoDSBlockHeaderHash.reservedfield().begin() +
           std::min((unsigned int)protoDSBlockHeaderHash.reservedfield().size(),
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

  auto blockHeaderBaseVars =
      io::ProtobufToBlockHeaderBase(protoBlockHeaderBase);
  if (!blockHeaderBaseVars) return false;

  const auto& [version, committeeHash, prevHash] = *blockHeaderBaseVars;

  dsBlockHeader = DSBlockHeader(
      dsdifficulty, difficulty, leaderPubKey, protoDSBlockHeader.blocknum(),
      protoDSBlockHeader.epochnum(), gasprice, swInfo, powDSWinners,
      removeDSNodePubKeys, hash, govProposalMap, version, committeeHash,
      prevHash);

  return true;
}

void MicroBlockHeaderToProtobuf(
    const MicroBlockHeader& microBlockHeader,
    ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader) {
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoMicroBlockHeader.mutable_blockheaderbase();
  io::BlockHeaderBaseToProtobuf(microBlockHeader, *protoBlockHeaderBase);

  protoMicroBlockHeader.set_shardid(microBlockHeader.GetShardId());
  protoMicroBlockHeader.set_gaslimit(microBlockHeader.GetGasLimit());
  protoMicroBlockHeader.set_gasused(microBlockHeader.GetGasUsed());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      microBlockHeader.GetRewards(), *protoMicroBlockHeader.mutable_rewards());
  protoMicroBlockHeader.set_epochnum(microBlockHeader.GetEpochNum());
  protoMicroBlockHeader.set_txroothash(microBlockHeader.GetTxRootHash().data(),
                                       microBlockHeader.GetTxRootHash().size);
  protoMicroBlockHeader.set_numtxs(microBlockHeader.GetNumTxs());
  SerializableToProtobufByteArray(microBlockHeader.GetMinerPubKey(),
                                  *protoMicroBlockHeader.mutable_minerpubkey());
  protoMicroBlockHeader.set_dsblocknum(microBlockHeader.GetDSBlockNum());
  protoMicroBlockHeader.set_statedeltahash(
      microBlockHeader.GetStateDeltaHash().data(),
      microBlockHeader.GetStateDeltaHash().size);
  protoMicroBlockHeader.set_tranreceipthash(
      microBlockHeader.GetTranReceiptHash().data(),
      microBlockHeader.GetTranReceiptHash().size);
}

bool ProtobufToMicroBlockHeader(
    const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader&
        protoMicroBlockHeader,
    MicroBlockHeader& microBlockHeader) {
  if (!CheckRequiredFieldsProtoMicroBlockMicroBlockHeader(
          protoMicroBlockHeader)) {
    LOG_GENERAL(WARNING,
                "CheckRequiredFieldsProtoMicroBlockMicroBlockHeader failed");
    return false;
  }

  uint64_t gasLimit;
  uint64_t gasUsed;
  uint128_t rewards;
  TxnHash txRootHash;
  PubKey minerPubKey;
  BlockHash dsBlockHash;
  StateHash stateDeltaHash;
  TxnHash tranReceiptHash;

  gasLimit = protoMicroBlockHeader.gaslimit();
  gasUsed = protoMicroBlockHeader.gasused();
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoMicroBlockHeader.rewards(), rewards);

  if (!CopyWithSizeCheck(protoMicroBlockHeader.txroothash(),
                         txRootHash.asArray())) {
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoMicroBlockHeader.minerpubkey(),
                                  minerPubKey);

  if (!CopyWithSizeCheck(protoMicroBlockHeader.statedeltahash(),
                         stateDeltaHash.asArray())) {
    return false;
  }

  if (!CopyWithSizeCheck(protoMicroBlockHeader.tranreceipthash(),
                         tranReceiptHash.asArray())) {
    return false;
  }

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoMicroBlockHeader.blockheaderbase();

  auto blockHeaderBaseVars =
      io::ProtobufToBlockHeaderBase(protoBlockHeaderBase);
  if (!blockHeaderBaseVars) return false;

  const auto& [version, committeeHash, prevHash] = *blockHeaderBaseVars;

  microBlockHeader = MicroBlockHeader(
      protoMicroBlockHeader.shardid(), gasLimit, gasUsed, rewards,
      protoMicroBlockHeader.epochnum(),
      {txRootHash, stateDeltaHash, tranReceiptHash},
      protoMicroBlockHeader.numtxs(), minerPubKey,
      protoMicroBlockHeader.dsblocknum(), version, committeeHash, prevHash);

  return true;
}

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
           std::min((unsigned int)protoTxBlockHeaderHash.stateroothash().size(),
                    (unsigned int)hash.m_stateRootHash.size),
       hash.m_stateRootHash.asArray().begin());
  copy(
      protoTxBlockHeaderHash.statedeltahash().begin(),
      protoTxBlockHeaderHash.statedeltahash().begin() +
          std::min((unsigned int)protoTxBlockHeaderHash.statedeltahash().size(),
                   (unsigned int)hash.m_stateDeltaHash.size),
      hash.m_stateDeltaHash.asArray().begin());
  copy(protoTxBlockHeaderHash.mbinfohash().begin(),
       protoTxBlockHeaderHash.mbinfohash().begin() +
           std::min((unsigned int)protoTxBlockHeaderHash.mbinfohash().size(),
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

void VCBlockHeaderToProtobuf(
    const VCBlockHeader& vcBlockHeader,
    ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoVCBlockHeader.mutable_blockheaderbase();
  io::BlockHeaderBaseToProtobuf(vcBlockHeader, *protoBlockHeaderBase);

  protoVCBlockHeader.set_viewchangedsepochno(
      vcBlockHeader.GetViewChangeDSEpochNo());
  protoVCBlockHeader.set_viewchangeepochno(
      vcBlockHeader.GetViewChangeEpochNo());
  protoVCBlockHeader.set_viewchangestate(vcBlockHeader.GetViewChangeState());
  SerializableToProtobufByteArray(
      vcBlockHeader.GetCandidateLeaderNetworkInfo(),
      *protoVCBlockHeader.mutable_candidateleadernetworkinfo());
  SerializableToProtobufByteArray(
      vcBlockHeader.GetCandidateLeaderPubKey(),
      *protoVCBlockHeader.mutable_candidateleaderpubkey());
  protoVCBlockHeader.set_vccounter(vcBlockHeader.GetViewChangeCounter());
  FaultyLeaderToProtobuf(vcBlockHeader.GetFaultyLeaders(), protoVCBlockHeader);
}

bool ProtobufToVCBlockHeader(
    const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
    VCBlockHeader& vcBlockHeader) {
  if (!CheckRequiredFieldsProtoVCBlockVCBlockHeader(protoVCBlockHeader)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlockVCBlockHeader failed");
    return false;
  }

  Peer candidateLeaderNetworkInfo;
  PubKey candidateLeaderPubKey;
  VectorOfNode faultyLeaders;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(
      protoVCBlockHeader.candidateleadernetworkinfo(),
      candidateLeaderNetworkInfo);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoVCBlockHeader.candidateleaderpubkey(),
                                  candidateLeaderPubKey);

  if (!ProtobufToFaultyDSMembers(protoVCBlockHeader, faultyLeaders)) {
    LOG_GENERAL(WARNING, "ProtobufToFaultyDSMembers failed");
    return false;
  }

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoVCBlockHeader.blockheaderbase();

  auto blockHeaderBaseVars =
      io::ProtobufToBlockHeaderBase(protoBlockHeaderBase);
  if (!blockHeaderBaseVars) return false;

  const auto& [version, committeeHash, prevHash] = *blockHeaderBaseVars;

  vcBlockHeader = VCBlockHeader(
      protoVCBlockHeader.viewchangedsepochno(),
      protoVCBlockHeader.viewchangeepochno(),
      protoVCBlockHeader.viewchangestate(), candidateLeaderNetworkInfo,
      candidateLeaderPubKey, protoVCBlockHeader.vccounter(), faultyLeaders,
      version, committeeHash, prevHash);

  return true;
}

void BlockBaseToProtobuf(const BlockBase& base,
                         ZilliqaMessage::ProtoBlockBase& protoBlockBase) {
  // Block hash

  protoBlockBase.set_blockhash(base.GetBlockHash().data(),
                               base.GetBlockHash().size);

  // Timestampo
  protoBlockBase.set_timestamp(base.GetTimestamp());

  // Serialize cosigs

  ZilliqaMessage::ProtoBlockBase::CoSignatures* cosigs =
      protoBlockBase.mutable_cosigs();

  SerializableToProtobufByteArray(base.GetCS1(), *cosigs->mutable_cs1());
  for (const auto& i : base.GetB1()) {
    cosigs->add_b1(i);
  }
  SerializableToProtobufByteArray(base.GetCS2(), *cosigs->mutable_cs2());
  for (const auto& i : base.GetB2()) {
    cosigs->add_b2(i);
  }
}

std::optional<std::tuple<BlockHash, CoSignatures, uint64_t>>
ProtobufToBlockBase(const ZilliqaMessage::ProtoBlockBase& protoBlockBase) {
  if (!CheckRequiredFieldsProtoBlockBase(protoBlockBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockBase failed");
    return std::nullopt;
  }

  // Deserialize cosigs
  CoSignatures cosigs;
  cosigs.m_B1.resize(protoBlockBase.cosigs().b1().size());
  cosigs.m_B2.resize(protoBlockBase.cosigs().b2().size());

  PROTOBUFBYTEARRAYTOSERIALIZABLEOPT(protoBlockBase.cosigs().cs1(),
                                     cosigs.m_CS1);
  copy(protoBlockBase.cosigs().b1().begin(), protoBlockBase.cosigs().b1().end(),
       cosigs.m_B1.begin());
  PROTOBUFBYTEARRAYTOSERIALIZABLEOPT(protoBlockBase.cosigs().cs2(),
                                     cosigs.m_CS2);
  copy(protoBlockBase.cosigs().b2().begin(), protoBlockBase.cosigs().b2().end(),
       cosigs.m_B2.begin());

  // Deserialize the block hash
  BlockHash blockHash;
  if (!CopyWithSizeCheck(protoBlockBase.blockhash(), blockHash.asArray())) {
    return std::nullopt;
  }

  // Deserialize timestamp
  uint64_t timestamp;
  timestamp = protoBlockBase.timestamp();

  return std::make_tuple(std::move(blockHash), std::move(cosigs), timestamp);
}

void DSBlockToProtobuf(const DSBlock& dsBlock,
                       ZilliqaMessage::ProtoDSBlock& protoDSBlock) {
  // Serialize header

  ZilliqaMessage::ProtoDSBlock::DSBlockHeader* protoHeader =
      protoDSBlock.mutable_header();

  const DSBlockHeader& header = dsBlock.GetHeader();

  io::DSBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoDSBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(dsBlock, *protoBlockBase);
}

bool ProtobufToDSBlock(const ZilliqaMessage::ProtoDSBlock& protoDSBlock,
                       DSBlock& dsBlock) {
  // Deserialize header

  if (!CheckRequiredFieldsProtoDSBlock(protoDSBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlock failed");
    return false;
  }

  const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoHeader =
      protoDSBlock.header();

  DSBlockHeader header;
  if (!io::ProtobufToDSBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToDSBlockHeader failed");
    return false;
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoDSBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  dsBlock = DSBlock{header, std::move(coSigs), timestamp};
  return true;
}

void MicroBlockToProtobuf(const MicroBlock& microBlock,
                          ZilliqaMessage::ProtoMicroBlock& protoMicroBlock) {
  // Serialize header

  ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader* protoHeader =
      protoMicroBlock.mutable_header();

  const MicroBlockHeader& header = microBlock.GetHeader();

  io::MicroBlockHeaderToProtobuf(header, *protoHeader);

  // Serialize body

  for (const auto& hash : microBlock.GetTranHashes()) {
    protoMicroBlock.add_tranhashes(hash.data(), hash.size);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoMicroBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(microBlock, *protoBlockBase);
}

bool ProtobufToMicroBlock(
    const ZilliqaMessage::ProtoMicroBlock& protoMicroBlock,
    MicroBlock& microBlock) {
  if (!CheckRequiredFieldsProtoMicroBlock(protoMicroBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMicroBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoHeader =
      protoMicroBlock.header();

  MicroBlockHeader header;

  if (!io::ProtobufToMicroBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToMicroBlockHeader failed");
    return false;
  }

  // Deserialize body

  std::vector<TxnHash> tranHashes;
  for (const auto& hash : protoMicroBlock.tranhashes()) {
    tranHashes.emplace_back();
    unsigned int size = std::min((unsigned int)hash.size(),
                                 (unsigned int)tranHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         tranHashes.back().asArray().begin());
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoMicroBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  microBlock = MicroBlock{header, tranHashes, std::move(coSigs), timestamp};
  return true;
}

void TxBlockToProtobuf(const TxBlock& txBlock,
                       ZilliqaMessage::ProtoTxBlock& protoTxBlock) {
  // Serialize header

  ZilliqaMessage::ProtoTxBlock::TxBlockHeader* protoHeader =
      protoTxBlock.mutable_header();

  const TxBlockHeader& header = txBlock.GetHeader();

  io::TxBlockHeaderToProtobuf(header, *protoHeader);

  for (const auto& mbInfo : txBlock.GetMicroBlockInfos()) {
    auto protoMbInfo = protoTxBlock.add_mbinfos();
    MbInfoToProtobuf(mbInfo, *protoMbInfo);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoTxBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(txBlock, *protoBlockBase);
}

bool ProtobufToTxBlock(const ZilliqaMessage::ProtoTxBlock& protoTxBlock,
                       TxBlock& txBlock) {
  if (!CheckRequiredFieldsProtoTxBlock(protoTxBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTxBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoHeader =
      protoTxBlock.header();

  TxBlockHeader header;

  if (!io::ProtobufToTxBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToTxBlockHeader failed");
    return false;
  }

  // Deserialize body
  std::vector<MicroBlockInfo> mbInfos;

  for (const auto& protoMbInfo : protoTxBlock.mbinfos()) {
    MicroBlockInfo mbInfo;
    if (!ProtobufToMbInfo(protoMbInfo, mbInfo)) {
      return false;
    }
    mbInfos.emplace_back(mbInfo);
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoTxBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  txBlock = TxBlock(header, mbInfos, std::move(coSigs), timestamp);
  return true;
}

void VCBlockToProtobuf(const VCBlock& vcBlock,
                       ZilliqaMessage::ProtoVCBlock& protoVCBlock) {
  // Serialize header

  ZilliqaMessage::ProtoVCBlock::VCBlockHeader* protoHeader =
      protoVCBlock.mutable_header();

  const VCBlockHeader& header = vcBlock.GetHeader();

  io::VCBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoVCBlock.mutable_blockbase();

  io::BlockBaseToProtobuf(vcBlock, *protoBlockBase);
}

bool ProtobufToVCBlock(const ZilliqaMessage::ProtoVCBlock& protoVCBlock,
                       VCBlock& vcBlock) {
  if (!CheckRequiredFieldsProtoVCBlock(protoVCBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoHeader =
      protoVCBlock.header();

  VCBlockHeader header;

  if (!io::ProtobufToVCBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToVCBlockHeader failed");
    return false;
  }

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoVCBlock.blockbase();

  auto blockBaseVars = io::ProtobufToBlockBase(protoBlockBase);
  if (!blockBaseVars) return false;

  const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
  vcBlock = VCBlock(header, std::move(coSigs), timestamp);
  return true;
}

void MbInfoToProtobuf(const MicroBlockInfo& mbInfo,
                      ZilliqaMessage::ProtoMbInfo& ProtoMbInfo) {
  ProtoMbInfo.set_mbhash(mbInfo.m_microBlockHash.data(),
                         mbInfo.m_microBlockHash.size);
  ProtoMbInfo.set_txroot(mbInfo.m_txnRootHash.data(),
                         mbInfo.m_txnRootHash.size);
  ProtoMbInfo.set_shardid(mbInfo.m_shardId);
}

}  // namespace io
