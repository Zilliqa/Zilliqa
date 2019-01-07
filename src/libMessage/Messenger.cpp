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

#include "Messenger.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libDirectoryService/DirectoryService.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"

#include <algorithm>
#include <map>
#include <random>
#include <unordered_set>

using namespace boost::multiprecision;
using namespace std;
using namespace ZilliqaMessage;

// ============================================================================
// Utility conversion functions
// ============================================================================

void SerializableToProtobufByteArray(const Serializable& serializable,
                                     ByteArray& byteArray) {
  bytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

void ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                     Serializable& serializable) {
  bytes tmp;
  copy(byteArray.data().begin(), byteArray.data().end(), back_inserter(tmp));
  serializable.Deserialize(tmp, 0);
}

// Temporary function for use by data blocks
void SerializableToProtobufByteArray(const SerializableDataBlock& serializable,
                                     ByteArray& byteArray) {
  bytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

// Temporary function for use by data blocks
void ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                     SerializableDataBlock& serializable) {
  bytes tmp;
  copy(byteArray.data().begin(), byteArray.data().end(), back_inserter(tmp));
  serializable.Deserialize(tmp, 0);
}

template <class T, size_t S>
void NumberToProtobufByteArray(const T& number, ByteArray& byteArray) {
  bytes tmp;
  Serializable::SetNumber<T>(tmp, 0, number, S);
  byteArray.set_data(tmp.data(), tmp.size());
}

template <class T, size_t S>
void ProtobufByteArrayToNumber(const ByteArray& byteArray, T& number) {
  bytes tmp;
  copy(byteArray.data().begin(), byteArray.data().end(), back_inserter(tmp));
  number = Serializable::GetNumber<T>(tmp, 0, S);
}

template <class T>
bool SerializeToArray(const T& protoMessage, bytes& dst,
                      const unsigned int offset) {
  if ((offset + protoMessage.ByteSize()) > dst.size()) {
    dst.resize(offset + protoMessage.ByteSize());
  }

  return protoMessage.SerializeToArray(dst.data() + offset,
                                       protoMessage.ByteSize());
}

template bool SerializeToArray<ProtoAccountStore>(
    const ProtoAccountStore& protoMessage, bytes& dst,
    const unsigned int offset);

template <class T>
bool RepeatableToArray(const T& repeatable, bytes& dst,
                       const unsigned int offset) {
  int tempOffset = offset;
  for (const auto& element : repeatable) {
    if (!SerializeToArray(element, dst, tempOffset)) {
      LOG_GENERAL(WARNING, "SerializeToArray failed, offset: " << tempOffset);
      return false;
    }
    tempOffset += element.ByteSize();
  }
  return true;
}

template <class T, size_t S>
void NumberToArray(const T& number, bytes& dst, const unsigned int offset) {
  Serializable::SetNumber<T>(dst, offset, number, S);
}

// ============================================================================
// Functions to check for fields in primitives that are used for persistent
// storage. Remove fields from the checks once they are deprecated.
// ============================================================================

inline bool CheckRequiredFieldsProtoBlockLink(
    const ProtoBlockLink& protoBlockLink) {
  return protoBlockLink.has_index() && protoBlockLink.has_dsindex() &&
         protoBlockLink.has_blocktype() && protoBlockLink.has_blockhash();
}

inline bool CheckRequiredFieldsProtoDSBlockPowDSWinner(
    const ProtoDSBlock::DSBlockHeader::PowDSWinners& powDSWinner) {
  return powDSWinner.has_key() && powDSWinner.has_val();
}

inline bool CheckRequiredFieldsProtoDSBlockDSBlockHashSet(
    const ProtoDSBlock::DSBlockHashSet& dsBlockHashSet) {
  return dsBlockHashSet.has_shardinghash() &&
         dsBlockHashSet.has_reservedfield();
}

inline bool CheckRequiredFieldsProtoDSBlockDSBlockHeader(
    const ProtoDSBlock::DSBlockHeader& protoDSBlockHeader) {
  // Don't need to enforce check on repeated member dswinners
  // Don't need to enforce check on optional members dsdifficulty, difficulty,
  // and gasprice
  return protoDSBlockHeader.has_prevhash() &&
         protoDSBlockHeader.has_leaderpubkey() &&
         protoDSBlockHeader.has_blocknum() &&
         protoDSBlockHeader.has_epochnum() && protoDSBlockHeader.has_swinfo() &&
         protoDSBlockHeader.has_hash() &&
         protoDSBlockHeader.has_committeehash() &&
         CheckRequiredFieldsProtoDSBlockDSBlockHashSet(
             protoDSBlockHeader.hash());
}

inline bool CheckRequiredFieldsProtoDSBlock(const ProtoDSBlock& protoDSBlock) {
  return protoDSBlock.has_header() && protoDSBlock.has_blockbase();
}

inline bool CheckRequiredFieldsProtoDSNode(const ProtoDSNode& protoDSNode) {
  return protoDSNode.has_pubkey() && protoDSNode.has_peer();
}

inline bool CheckRequiredFieldsProtoDSCommittee(
    const ProtoDSCommittee& protoDSCommittee) {
  // Don't need to enforce check on repeated member dsnodes
  return true;
}

inline bool CheckRequiredFieldsProtoMicroBlockMicroBlockHeader(
    const ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader) {
  return protoMicroBlockHeader.has_type() &&
         protoMicroBlockHeader.has_version() &&
         protoMicroBlockHeader.has_shardid() &&
         protoMicroBlockHeader.has_gaslimit() &&
         protoMicroBlockHeader.has_gasused() &&
         protoMicroBlockHeader.has_rewards() &&
         protoMicroBlockHeader.has_prevhash() &&
         protoMicroBlockHeader.has_epochnum() &&
         protoMicroBlockHeader.has_txroothash() &&
         protoMicroBlockHeader.has_numtxs() &&
         protoMicroBlockHeader.has_minerpubkey() &&
         protoMicroBlockHeader.has_dsblocknum() &&
         protoMicroBlockHeader.has_statedeltahash() &&
         protoMicroBlockHeader.has_tranreceipthash() &&
         protoMicroBlockHeader.has_committeehash();
}

inline bool CheckRequiredFieldsProtoMicroBlock(
    const ProtoMicroBlock& protoMicroBlock) {
  // Don't need to enforce check on repeated member tranhashes
  return protoMicroBlock.has_header() && protoMicroBlock.has_blockbase();
}

inline bool CheckRequiredFieldsProtoShardingStructureMember(
    const ProtoShardingStructure::Member& protoMember) {
  return protoMember.has_pubkey() && protoMember.has_peerinfo() &&
         protoMember.has_reputation();
}

inline bool CheckRequiredFieldsProtoShardingStructureShard(
    const ProtoShardingStructure::Shard& protoShard) {
  // Don't need to enforce check on repeated member members
  return true;
}

inline bool CheckRequiredFieldsProtoShardingStructure(
    const ProtoShardingStructure& protoShardingStructure) {
  // Don't need to enforce check on repeated member shards
  return true;
}

inline bool CheckRequiredFieldsProtoTxBlockTxBlockHashSet(
    const ProtoTxBlock::TxBlockHashSet& protoTxBlockHashSet) {
  return protoTxBlockHashSet.has_stateroothash() &&
         protoTxBlockHashSet.has_statedeltahash() &&
         protoTxBlockHashSet.has_mbinfohash();
}

inline bool CheckRequiredFieldsProtoMbInfo(const ProtoMbInfo& protoMbInfo) {
  return protoMbInfo.has_mbhash() && protoMbInfo.has_txroot() &&
         protoMbInfo.has_shardid();
}

inline bool CheckRequiredFieldsProtoTxBlockTxBlockHeader(
    const ProtoTxBlock::TxBlockHeader& protoTxBlockHeader) {
  return protoTxBlockHeader.has_type() && protoTxBlockHeader.has_version() &&
         protoTxBlockHeader.has_gaslimit() &&
         protoTxBlockHeader.has_gasused() && protoTxBlockHeader.has_rewards() &&
         protoTxBlockHeader.has_prevhash() &&
         protoTxBlockHeader.has_blocknum() && protoTxBlockHeader.has_hash() &&
         protoTxBlockHeader.has_numtxs() &&
         protoTxBlockHeader.has_minerpubkey() &&
         protoTxBlockHeader.has_dsblocknum() &&
         protoTxBlockHeader.has_committeehash() &&
         CheckRequiredFieldsProtoTxBlockTxBlockHashSet(
             protoTxBlockHeader.hash());
}

inline bool CheckRequiredFieldsProtoTxBlock(const ProtoTxBlock& protoTxBlock) {
  // Don't need to enforce check on repeated member mbinfos
  return protoTxBlock.has_header() && protoTxBlock.has_blockbase();
}

inline bool CheckRequiredFieldsProtoVCBlockVCBlockHeader(
    const ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  // Don't need to enforce check on repeated member faultyleaders
  return protoVCBlockHeader.has_viewchangedsepochno() &&
         protoVCBlockHeader.has_viewchangeepochno() &&
         protoVCBlockHeader.has_viewchangestate() &&
         protoVCBlockHeader.has_candidateleadernetworkinfo() &&
         protoVCBlockHeader.has_candidateleaderpubkey() &&
         protoVCBlockHeader.has_vccounter() &&
         protoVCBlockHeader.has_committeehash() &&
         protoVCBlockHeader.has_prevhash();
}

inline bool CheckRequiredFieldsProtoVCBlock(const ProtoVCBlock& protoVCBlock) {
  return protoVCBlock.has_header() && protoVCBlock.has_blockbase();
}

inline bool CheckRequiredFieldsProtoBlockBaseCoSignatures(
    const ProtoBlockBase::CoSignatures& protoCoSignatures) {
  // Don't need to enforce check on repeated members b1 and b2
  return protoCoSignatures.has_cs1() && protoCoSignatures.has_cs2();
}

inline bool CheckRequiredFieldsProtoBlockBase(
    const ProtoBlockBase& protoBlockBase) {
  return protoBlockBase.has_blockhash() && protoBlockBase.has_cosigs() &&
         protoBlockBase.has_timestamp() &&
         CheckRequiredFieldsProtoBlockBaseCoSignatures(protoBlockBase.cosigs());
}

// ============================================================================
// Protobuf <-> Primitives conversion functions
// ============================================================================

void AccountToProtobuf(const Account& account, ProtoAccount& protoAccount) {
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      account.GetBalance(), *protoAccount.mutable_balance());
  protoAccount.set_nonce(account.GetNonce());
  protoAccount.set_storageroot(account.GetStorageRoot().data(),
                               account.GetStorageRoot().size);

  if (account.GetCode().size() > 0) {
    protoAccount.set_codehash(account.GetCodeHash().data(),
                              account.GetCodeHash().size);
    protoAccount.set_createblocknum(account.GetCreateBlockNum());
    protoAccount.set_initdata(account.GetInitData().data(),
                              account.GetInitData().size());
    protoAccount.set_code(account.GetCode().data(), account.GetCode().size());

    for (const auto& keyHash : account.GetStorageKeyHashes()) {
      ProtoAccount::StorageData* entry = protoAccount.add_storage();
      entry->set_keyhash(keyHash.data(), keyHash.size);
      entry->set_data(account.GetRawStorage(keyHash));
    }
  }
}

bool ProtobufToAccount(const ProtoAccount& protoAccount, Account& account,
                       const Address& addr) {
  uint128_t tmpNumber;

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(protoAccount.balance(),
                                                     tmpNumber);
  account.SetBalance(tmpNumber);
  account.SetNonce(protoAccount.nonce());

  dev::h256 tmpStorageRoot;
  copy(protoAccount.storageroot().begin(),
       protoAccount.storageroot().begin() +
           min((unsigned int)protoAccount.storageroot().size(),
               (unsigned int)tmpStorageRoot.size),
       tmpStorageRoot.asArray().begin());

  if (protoAccount.code().size() > 0) {
    bytes tmpVec;
    tmpVec.resize(protoAccount.code().size());
    copy(protoAccount.code().begin(), protoAccount.code().end(),
         tmpVec.begin());
    account.SetCode(tmpVec);

    dev::h256 tmpHash;
    if (!Messenger::CopyWithSizeCheck(protoAccount.codehash(),
                                      tmpHash.asArray())) {
      return false;
    }

    if (account.GetCodeHash() != tmpHash) {
      LOG_GENERAL(WARNING,
                  "Code hash mismatch. Expected: "
                      << DataConversion::charArrToHexStr(
                             account.GetCodeHash().asArray())
                      << " Actual: "
                      << DataConversion::charArrToHexStr(tmpHash.asArray()));
      return false;
    }

    account.SetCreateBlockNum(protoAccount.createblocknum());

    if (protoAccount.initdata().size() > 0) {
      tmpVec.resize(protoAccount.initdata().size());
      copy(protoAccount.initdata().begin(), protoAccount.initdata().end(),
           tmpVec.begin());
      account.InitContract(tmpVec, addr);
    }

    for (const auto& entry : protoAccount.storage()) {
      if (!Messenger::CopyWithSizeCheck(entry.keyhash(), tmpHash.asArray())) {
        return false;
      }

      account.SetStorage(tmpHash, entry.data());
    }

    if (account.GetStorageRoot() != tmpStorageRoot) {
      LOG_GENERAL(WARNING, "Storage root mismatch. Expected: "
                               << DataConversion::charArrToHexStr(
                                      account.GetStorageRoot().asArray())
                               << " Actual: "
                               << DataConversion::charArrToHexStr(
                                      tmpStorageRoot.asArray()));
      return false;
    }
  }

  return true;
}

void AccountDeltaToProtobuf(const Account* oldAccount,
                            const Account& newAccount,
                            ProtoAccount& protoAccount) {
  Account acc(0, 0);

  bool fullCopy = false;

  if (oldAccount == nullptr) {
    oldAccount = &acc;
    fullCopy = true;
  }

  int256_t balanceDelta =
      int256_t(newAccount.GetBalance()) - int256_t(oldAccount->GetBalance());
  protoAccount.set_numbersign(balanceDelta > 0);

  uint128_t balanceDeltaNum(abs(balanceDelta));
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      balanceDeltaNum, *protoAccount.mutable_balance());

  uint64_t nonceDelta = 0;
  if (!SafeMath<uint64_t>::sub(newAccount.GetNonce(), oldAccount->GetNonce(),
                               nonceDelta)) {
    return;
  }

  protoAccount.set_nonce(nonceDelta);

  if (!newAccount.GetCode().empty()) {
    if (fullCopy) {
      protoAccount.set_code(newAccount.GetCode().data(),
                            newAccount.GetCode().size());
      protoAccount.set_initdata(newAccount.GetInitData().data(),
                                newAccount.GetInitData().size());
      protoAccount.set_createblocknum(newAccount.GetCreateBlockNum());
    }

    if (newAccount.GetStorageRoot() != oldAccount->GetStorageRoot()) {
      protoAccount.set_storageroot(newAccount.GetStorageRoot().data(),
                                   newAccount.GetStorageRoot().size);

      for (const auto& keyHash : newAccount.GetStorageKeyHashes()) {
        string rlpStr = newAccount.GetRawStorage(keyHash);
        if (rlpStr != oldAccount->GetRawStorage(keyHash)) {
          ProtoAccount::StorageData* entry = protoAccount.add_storage();
          entry->set_keyhash(keyHash.data(), keyHash.size);
          entry->set_data(rlpStr);
        }
      }
    }
  }
}

bool ProtobufToAccountDelta(const ProtoAccount& protoAccount, Account& account,
                            const Address& addr, const bool fullCopy) {
  uint128_t tmpNumber;

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(protoAccount.balance(),
                                                     tmpNumber);

  int256_t balanceDelta = protoAccount.numbersign()
                              ? tmpNumber.convert_to<int256_t>()
                              : 0 - tmpNumber.convert_to<int256_t>();
  account.ChangeBalance(balanceDelta);

  account.IncreaseNonceBy(protoAccount.nonce());

  if (protoAccount.code().size() > 0 || account.isContract()) {
    bool doInitContract = false;

    if (fullCopy) {
      bytes tmpVec;
      if (protoAccount.code().size() > MAX_CODE_SIZE_IN_BYTES) {
        LOG_GENERAL(WARNING, "Code size "
                                 << protoAccount.code().size()
                                 << " greater than MAX_CODE_SIZE_IN_BYTES "
                                 << MAX_CODE_SIZE_IN_BYTES);
        return false;
      }
      tmpVec.resize(protoAccount.code().size());
      copy(protoAccount.code().begin(), protoAccount.code().end(),
           tmpVec.begin());
      if (tmpVec != account.GetCode()) {
        account.SetCode(tmpVec);
      }

      if (!protoAccount.initdata().empty() && account.GetInitData().empty()) {
        tmpVec.resize(protoAccount.initdata().size());
        copy(protoAccount.initdata().begin(), protoAccount.initdata().end(),
             tmpVec.begin());
        account.SetInitData(tmpVec);
        doInitContract = true;
      }

      account.SetCreateBlockNum(protoAccount.createblocknum());
    }

    dev::h256 tmpStorageRoot;
    copy(protoAccount.storageroot().begin(),
         protoAccount.storageroot().begin() +
             min((unsigned int)protoAccount.storageroot().size(),
                 (unsigned int)tmpStorageRoot.size),
         tmpStorageRoot.asArray().begin());

    if (tmpStorageRoot != account.GetStorageRoot()) {
      if (doInitContract) {
        account.InitContract(addr);
      }

      dev::h256 tmpHash;

      for (const auto& entry : protoAccount.storage()) {
        if (!Messenger::CopyWithSizeCheck(entry.keyhash(), tmpHash.asArray())) {
          return false;
        }

        account.SetStorage(tmpHash, entry.data());
      }

      if (tmpStorageRoot != account.GetStorageRoot()) {
        LOG_GENERAL(WARNING, "Storage root mismatch. Expected: "
                                 << DataConversion::charArrToHexStr(
                                        account.GetStorageRoot().asArray())
                                 << " Actual: "
                                 << DataConversion::charArrToHexStr(
                                        tmpStorageRoot.asArray()));
        return false;
      }
    }
  }

  return true;
}

void DSCommitteeToProtobuf(const deque<pair<PubKey, Peer>>& dsCommittee,
                           ProtoDSCommittee& protoDSCommittee) {
  for (const auto& node : dsCommittee) {
    ProtoDSNode* protodsnode = protoDSCommittee.add_dsnodes();
    SerializableToProtobufByteArray(node.first, *protodsnode->mutable_pubkey());
    SerializableToProtobufByteArray(node.second, *protodsnode->mutable_peer());
  }
}

bool ProtobufToDSCommittee(const ProtoDSCommittee& protoDSCommittee,
                           deque<pair<PubKey, Peer>>& dsCommittee) {
  if (!CheckRequiredFieldsProtoDSCommittee(protoDSCommittee)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSCommittee failed.");
    return false;
  }

  for (const auto& dsnode : protoDSCommittee.dsnodes()) {
    if (!CheckRequiredFieldsProtoDSNode(dsnode)) {
      LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSNode failed.");
      continue;
    }

    PubKey pubkey;
    Peer peer;

    ProtobufByteArrayToSerializable(dsnode.pubkey(), pubkey);
    ProtobufByteArrayToSerializable(dsnode.peer(), peer);
    dsCommittee.emplace_back(pubkey, peer);
  }

  return true;
}

void FaultyLeaderToProtobuf(const vector<pair<PubKey, Peer>>& faultyLeaders,
                            ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  for (const auto& node : faultyLeaders) {
    ProtoDSNode* protodsnode = protoVCBlockHeader.add_faultyleaders();
    SerializableToProtobufByteArray(node.first, *protodsnode->mutable_pubkey());
    SerializableToProtobufByteArray(node.second, *protodsnode->mutable_peer());
  }
}

void ProtobufToFaultyDSMembers(
    const ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
    vector<pair<PubKey, Peer>>& faultyDSMembers) {
  for (const auto& dsnode : protoVCBlockHeader.faultyleaders()) {
    PubKey pubkey;
    Peer peer;

    ProtobufByteArrayToSerializable(dsnode.pubkey(), pubkey);
    ProtobufByteArrayToSerializable(dsnode.peer(), peer);
    faultyDSMembers.emplace_back(pubkey, peer);
  }
}

void DSCommitteeToProtoCommittee(const deque<pair<PubKey, Peer>>& dsCommittee,
                                 ProtoCommittee& protoCommittee) {
  for (const auto& node : dsCommittee) {
    SerializableToProtobufByteArray(node.first, *protoCommittee.add_members());
  }
}

void ShardToProtoCommittee(const Shard& shard, ProtoCommittee& protoCommittee) {
  for (const auto& node : shard) {
    SerializableToProtobufByteArray(std::get<SHARD_NODE_PUBKEY>(node),
                                    *protoCommittee.add_members());
  }
}

void BlockBaseToProtobuf(const BlockBase& base,
                         ProtoBlockBase& protoBlockBase) {
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

bool ProtobufToBlockBase(const ProtoBlockBase& protoBlockBase,
                         BlockBase& base) {
  if (!CheckRequiredFieldsProtoBlockBase(protoBlockBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockBase failed.");
    return false;
  }

  // Deserialize cosigs
  CoSignatures cosigs;
  cosigs.m_B1.resize(protoBlockBase.cosigs().b1().size());
  cosigs.m_B2.resize(protoBlockBase.cosigs().b2().size());

  ProtobufByteArrayToSerializable(protoBlockBase.cosigs().cs1(), cosigs.m_CS1);
  copy(protoBlockBase.cosigs().b1().begin(), protoBlockBase.cosigs().b1().end(),
       cosigs.m_B1.begin());
  ProtobufByteArrayToSerializable(protoBlockBase.cosigs().cs2(), cosigs.m_CS2);
  copy(protoBlockBase.cosigs().b2().begin(), protoBlockBase.cosigs().b2().end(),
       cosigs.m_B2.begin());

  base.SetCoSignatures(cosigs);

  // Deserialize the block hash
  BlockHash blockHash;
  if (!Messenger::CopyWithSizeCheck(protoBlockBase.blockhash(),
                                    blockHash.asArray())) {
    return false;
  }
  base.SetBlockHash(blockHash);

  // Deserialize timestamp
  uint64_t timestamp;
  timestamp = protoBlockBase.timestamp();

  base.SetTimestamp(timestamp);

  return true;
}

void ShardingStructureToProtobuf(
    const DequeOfShard& shards,
    ProtoShardingStructure& protoShardingStructure) {
  for (const auto& shard : shards) {
    ProtoShardingStructure::Shard* proto_shard =
        protoShardingStructure.add_shards();

    for (const auto& node : shard) {
      ProtoShardingStructure::Member* proto_member = proto_shard->add_members();

      SerializableToProtobufByteArray(std::get<SHARD_NODE_PUBKEY>(node),
                                      *proto_member->mutable_pubkey());
      SerializableToProtobufByteArray(std::get<SHARD_NODE_PEER>(node),
                                      *proto_member->mutable_peerinfo());
      proto_member->set_reputation(std::get<SHARD_NODE_REP>(node));
    }
  }
}

bool ProtobufToShardingStructure(
    const ProtoShardingStructure& protoShardingStructure,
    DequeOfShard& shards) {
  if (!CheckRequiredFieldsProtoShardingStructure(protoShardingStructure)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoShardingStructure failed.");
    return false;
  }

  for (const auto& proto_shard : protoShardingStructure.shards()) {
    if (!CheckRequiredFieldsProtoShardingStructureShard(proto_shard)) {
      LOG_GENERAL(WARNING,
                  "CheckRequiredFieldsProtoShardingStructureShard failed.");
      continue;
    }

    shards.emplace_back();

    for (const auto& proto_member : proto_shard.members()) {
      if (!CheckRequiredFieldsProtoShardingStructureMember(proto_member)) {
        LOG_GENERAL(WARNING,
                    "CheckRequiredFieldsProtoShardingStructureMember failed.");
        continue;
      }

      PubKey key;
      Peer peer;

      ProtobufByteArrayToSerializable(proto_member.pubkey(), key);
      ProtobufByteArrayToSerializable(proto_member.peerinfo(), peer);

      shards.back().emplace_back(key, peer, proto_member.reputation());
    }
  }

  return true;
}

void AnnouncementShardingStructureToProtobuf(
    const DequeOfShard& shards, const MapOfPubKeyPoW& allPoWs,
    ProtoShardingStructureWithPoWSolns& protoShardingStructure) {
  for (const auto& shard : shards) {
    ProtoShardingStructureWithPoWSolns::Shard* proto_shard =
        protoShardingStructure.add_shards();

    for (const auto& node : shard) {
      ProtoShardingStructureWithPoWSolns::Member* proto_member =
          proto_shard->add_members();

      const PubKey& key = std::get<SHARD_NODE_PUBKEY>(node);

      SerializableToProtobufByteArray(key, *proto_member->mutable_pubkey());
      SerializableToProtobufByteArray(std::get<SHARD_NODE_PEER>(node),
                                      *proto_member->mutable_peerinfo());
      proto_member->set_reputation(std::get<SHARD_NODE_REP>(node));

      ProtoPoWSolution* proto_soln = proto_member->mutable_powsoln();
      const auto soln = allPoWs.find(key);
      proto_soln->set_nonce(soln->second.nonce);
      proto_soln->set_result(soln->second.result.data(),
                             soln->second.result.size());
      proto_soln->set_mixhash(soln->second.mixhash.data(),
                              soln->second.mixhash.size());
      proto_soln->set_lookupid(soln->second.lookupId);
      NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
          soln->second.gasPrice, *proto_soln->mutable_gasprice());
    }
  }
}

void ProtobufToShardingStructureAnnouncement(
    const ProtoShardingStructureWithPoWSolns& protoShardingStructure,
    DequeOfShard& shards, MapOfPubKeyPoW& allPoWs) {
  std::array<unsigned char, 32> result;
  std::array<unsigned char, 32> mixhash;
  uint128_t gasPrice;

  for (const auto& proto_shard : protoShardingStructure.shards()) {
    shards.emplace_back();

    for (const auto& proto_member : proto_shard.members()) {
      PubKey key;
      Peer peer;

      ProtobufByteArrayToSerializable(proto_member.pubkey(), key);
      ProtobufByteArrayToSerializable(proto_member.peerinfo(), peer);

      shards.back().emplace_back(key, peer, proto_member.reputation());

      copy(proto_member.powsoln().result().begin(),
           proto_member.powsoln().result().begin() +
               min((unsigned int)proto_member.powsoln().result().size(),
                   (unsigned int)result.size()),
           result.begin());
      copy(proto_member.powsoln().mixhash().begin(),
           proto_member.powsoln().mixhash().begin() +
               min((unsigned int)proto_member.powsoln().mixhash().size(),
                   (unsigned int)mixhash.size()),
           mixhash.begin());
      ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
          proto_member.powsoln().gasprice(), gasPrice);
      allPoWs.emplace(
          key, PoWSolution(proto_member.powsoln().nonce(), result, mixhash,
                           proto_member.powsoln().lookupid(), gasPrice));
    }
  }
}

void TransactionCoreInfoToProtobuf(const TransactionCoreInfo& txnCoreInfo,
                                   ProtoTransactionCoreInfo& protoTxnCoreInfo) {
  protoTxnCoreInfo.set_version(txnCoreInfo.version);
  protoTxnCoreInfo.set_nonce(txnCoreInfo.nonce);
  protoTxnCoreInfo.set_toaddr(txnCoreInfo.toAddr.data(),
                              txnCoreInfo.toAddr.size);
  SerializableToProtobufByteArray(txnCoreInfo.senderPubKey,
                                  *protoTxnCoreInfo.mutable_senderpubkey());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      txnCoreInfo.amount, *protoTxnCoreInfo.mutable_amount());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      txnCoreInfo.gasPrice, *protoTxnCoreInfo.mutable_gasprice());
  protoTxnCoreInfo.set_gaslimit(txnCoreInfo.gasLimit);
  protoTxnCoreInfo.set_code(txnCoreInfo.code.data(), txnCoreInfo.code.size());
  protoTxnCoreInfo.set_data(txnCoreInfo.data.data(), txnCoreInfo.data.size());
}

void ProtobufToTransactionCoreInfo(
    const ProtoTransactionCoreInfo& protoTxnCoreInfo,
    TransactionCoreInfo& txnCoreInfo) {
  txnCoreInfo.version = protoTxnCoreInfo.version();
  txnCoreInfo.nonce = protoTxnCoreInfo.nonce();
  copy(protoTxnCoreInfo.toaddr().begin(),
       protoTxnCoreInfo.toaddr().begin() +
           min((unsigned int)protoTxnCoreInfo.toaddr().size(),
               (unsigned int)txnCoreInfo.toAddr.size),
       txnCoreInfo.toAddr.asArray().begin());
  ProtobufByteArrayToSerializable(protoTxnCoreInfo.senderpubkey(),
                                  txnCoreInfo.senderPubKey);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(protoTxnCoreInfo.amount(),
                                                     txnCoreInfo.amount);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxnCoreInfo.gasprice(), txnCoreInfo.gasPrice);
  txnCoreInfo.gasLimit = protoTxnCoreInfo.gaslimit();
  txnCoreInfo.code.resize(protoTxnCoreInfo.code().size());
  copy(protoTxnCoreInfo.code().begin(), protoTxnCoreInfo.code().end(),
       txnCoreInfo.code.begin());
  txnCoreInfo.data.resize(protoTxnCoreInfo.data().size());
  copy(protoTxnCoreInfo.data().begin(), protoTxnCoreInfo.data().end(),
       txnCoreInfo.data.begin());
}

void TransactionToProtobuf(const Transaction& transaction,
                           ProtoTransaction& protoTransaction) {
  protoTransaction.set_tranid(transaction.GetTranID().data(),
                              transaction.GetTranID().size);
  TransactionCoreInfoToProtobuf(transaction.GetCoreInfo(),
                                *protoTransaction.mutable_info());

  SerializableToProtobufByteArray(transaction.GetSignature(),
                                  *protoTransaction.mutable_signature());
}

void ProtobufToTransaction(const ProtoTransaction& protoTransaction,
                           Transaction& transaction) {
  TxnHash tranID;
  TransactionCoreInfo txnCoreInfo;
  Signature signature;

  copy(protoTransaction.tranid().begin(),
       protoTransaction.tranid().begin() +
           min((unsigned int)protoTransaction.tranid().size(),
               (unsigned int)tranID.size),
       tranID.asArray().begin());

  ProtobufToTransactionCoreInfo(protoTransaction.info(), txnCoreInfo);

  ProtobufByteArrayToSerializable(protoTransaction.signature(), signature);

  bytes txnData;
  if (!SerializeToArray(protoTransaction.info(), txnData, 0)) {
    LOG_GENERAL(WARNING, "Serialize Proto transaction core info failed.");
    return;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& hash = sha2.Finalize();

  if (!std::equal(hash.begin(), hash.end(), tranID.begin(), tranID.end())) {
    TxnHash expected;
    copy(hash.begin(), hash.end(), expected.asArray().begin());
    LOG_GENERAL(WARNING, "TranID verification failed. Expected: "
                             << expected << " Actual: " << tranID);
    return;
  }

  // Verify signature
  if (!Schnorr::GetInstance().Verify(txnData, signature,
                                     txnCoreInfo.senderPubKey)) {
    LOG_GENERAL(WARNING, "Signature verification failed.");
    return;
  }

  transaction = Transaction(
      tranID, txnCoreInfo.version, txnCoreInfo.nonce, txnCoreInfo.toAddr,
      txnCoreInfo.senderPubKey, txnCoreInfo.amount, txnCoreInfo.gasPrice,
      txnCoreInfo.gasLimit, txnCoreInfo.code, txnCoreInfo.data, signature);
}

void TransactionOffsetToProtobuf(const std::vector<uint32_t>& txnOffsets,
                                 ProtoTxnFileOffset& protoTxnFileOffset) {
  for (const auto& offset : txnOffsets) {
    protoTxnFileOffset.add_offsetinfile(offset);
  }
}

void ProtobufToTransactionOffset(const ProtoTxnFileOffset& protoTxnFileOffset,
                                 std::vector<uint32_t>& txnOffsets) {
  txnOffsets.clear();
  for (const auto& offset : protoTxnFileOffset.offsetinfile()) {
    txnOffsets.push_back(offset);
  }
}

void TransactionArrayToProtobuf(const std::vector<Transaction>& txns,
                                ProtoTransactionArray& protoTransactionArray) {
  for (const auto& txn : txns) {
    TransactionToProtobuf(txn, *protoTransactionArray.add_transactions());
  }
}

void ProtobufToTransactionArray(
    const ProtoTransactionArray& protoTransactionArray,
    std::vector<Transaction>& txns) {
  for (const auto& protoTransaction : protoTransactionArray.transactions()) {
    Transaction txn;
    ProtobufToTransaction(protoTransaction, txn);
    txns.push_back(txn);
  }
}

void TransactionReceiptToProtobuf(const TransactionReceipt& transReceipt,
                                  ProtoTransactionReceipt& protoTransReceipt) {
  protoTransReceipt.set_receipt(transReceipt.GetString());
  // protoTransReceipt.set_cumgas(transReceipt.GetCumGas());
  protoTransReceipt.set_cumgas(transReceipt.GetCumGas());
}

void ProtobufToTransactionReceipt(
    const ProtoTransactionReceipt& protoTransactionReceipt,
    TransactionReceipt& transactionReceipt) {
  std::string tranReceiptStr;
  tranReceiptStr.resize(protoTransactionReceipt.receipt().size());
  copy(protoTransactionReceipt.receipt().begin(),
       protoTransactionReceipt.receipt().end(), tranReceiptStr.begin());
  transactionReceipt.SetString(tranReceiptStr);
  transactionReceipt.SetCumGas(protoTransactionReceipt.cumgas());
}

void TransactionWithReceiptToProtobuf(
    const TransactionWithReceipt& transWithReceipt,
    ProtoTransactionWithReceipt& protoTransWithReceipt) {
  auto* protoTransaction = protoTransWithReceipt.mutable_transaction();
  TransactionToProtobuf(transWithReceipt.GetTransaction(), *protoTransaction);

  auto* protoTranReceipt = protoTransWithReceipt.mutable_receipt();
  TransactionReceiptToProtobuf(transWithReceipt.GetTransactionReceipt(),
                               *protoTranReceipt);
}

void ProtobufToTransactionWithReceipt(
    const ProtoTransactionWithReceipt& protoWithTransaction,
    TransactionWithReceipt& transactionWithReceipt) {
  Transaction transaction;
  ProtobufToTransaction(protoWithTransaction.transaction(), transaction);

  TransactionReceipt receipt;
  ProtobufToTransactionReceipt(protoWithTransaction.receipt(), receipt);

  transactionWithReceipt = TransactionWithReceipt(transaction, receipt);
}

void PeerToProtobuf(const Peer& peer, ProtoPeer& protoPeer) {
  NumberToProtobufByteArray<boost::multiprecision::uint128_t,
                            sizeof(boost::multiprecision::uint128_t)>(
      peer.GetIpAddress(), *protoPeer.mutable_ipaddress());

  protoPeer.set_listenporthost(peer.GetListenPortHost());
}

void ProtobufToPeer(const ProtoPeer& protoPeer, Peer& peer) {
  boost::multiprecision::uint128_t ipAddress;
  ProtobufByteArrayToNumber<boost::multiprecision::uint128_t,
                            sizeof(boost::multiprecision::uint128_t)>(
      protoPeer.ipaddress(), ipAddress);

  peer = Peer(ipAddress, protoPeer.listenporthost());
}

void DSBlockHeaderToProtobuf(const DSBlockHeader& dsBlockHeader,
                             ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
                             bool concreteVarsOnly = false) {
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
  }

  protoDSBlockHeader.set_prevhash(dsBlockHeader.GetPrevHash().data(),
                                  dsBlockHeader.GetPrevHash().size);
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

  protoDSBlockHeader.set_committeehash(dsBlockHeader.GetCommitteeHash().data(),
                                       dsBlockHeader.GetCommitteeHash().size);
}

void DSBlockToProtobuf(const DSBlock& dsBlock, ProtoDSBlock& protoDSBlock) {
  // Serialize header

  ZilliqaMessage::ProtoDSBlock::DSBlockHeader* protoHeader =
      protoDSBlock.mutable_header();

  const DSBlockHeader& header = dsBlock.GetHeader();

  DSBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoDSBlock.mutable_blockbase();

  BlockBaseToProtobuf(dsBlock, *protoBlockBase);
}

bool ProtobufToDSBlockHeader(
    const ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
    DSBlockHeader& dsBlockHeader) {
  if (!CheckRequiredFieldsProtoDSBlockDSBlockHeader(protoDSBlockHeader)) {
    LOG_GENERAL(WARNING,
                "CheckRequiredFieldsProtoDSBlockDSBlockHeader failed.");
    return false;
  }

  BlockHash prevHash;
  PubKey leaderPubKey;
  SWInfo swInfo;
  CommitteeHash committeeHash;

  if (!Messenger::CopyWithSizeCheck(protoDSBlockHeader.prevhash(),
                                    prevHash.asArray())) {
    return false;
  }

  ProtobufByteArrayToSerializable(protoDSBlockHeader.leaderpubkey(),
                                  leaderPubKey);
  ProtobufByteArrayToSerializable(protoDSBlockHeader.swinfo(), swInfo);

  // Deserialize powDSWinners
  map<PubKey, Peer> powDSWinners;
  PubKey tempPubKey;
  Peer tempWinnerNetworkInfo;
  for (const auto& dswinner : protoDSBlockHeader.dswinners()) {
    if (!CheckRequiredFieldsProtoDSBlockPowDSWinner(dswinner)) {
      LOG_GENERAL(WARNING,
                  "CheckRequiredFieldsProtoDSBlockPowDSWinner failed.");
      continue;
    }
    ProtobufByteArrayToSerializable(dswinner.key(), tempPubKey);
    ProtobufByteArrayToSerializable(dswinner.val(), tempWinnerNetworkInfo);
    powDSWinners[tempPubKey] = tempWinnerNetworkInfo;
  }

  // Deserialize DSBlockHashSet
  DSBlockHashSet hash;
  const ZilliqaMessage::ProtoDSBlock::DSBlockHashSet& protoDSBlockHeaderHash =
      protoDSBlockHeader.hash();

  if (!Messenger::CopyWithSizeCheck(protoDSBlockHeaderHash.shardinghash(),
                                    hash.m_shardingHash.asArray())) {
    return false;
  }

  if (!Messenger::CopyWithSizeCheck(protoDSBlockHeader.committeehash(),
                                    committeeHash.asArray())) {
    return false;
  }

  copy(protoDSBlockHeaderHash.reservedfield().begin(),
       protoDSBlockHeaderHash.reservedfield().begin() +
           min((unsigned int)protoDSBlockHeaderHash.reservedfield().size(),
               (unsigned int)hash.m_reservedField.size()),
       hash.m_reservedField.begin());

  // Generate the new DSBlock

  const uint8_t dsdifficulty = protoDSBlockHeader.has_dsdifficulty()
                                   ? protoDSBlockHeader.dsdifficulty()
                                   : 0;
  const uint8_t difficulty =
      protoDSBlockHeader.has_difficulty() ? protoDSBlockHeader.difficulty() : 0;
  uint128_t gasprice = 0;

  if (protoDSBlockHeader.has_gasprice()) {
    ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
        protoDSBlockHeader.gasprice(), gasprice);
  }

  dsBlockHeader = DSBlockHeader(dsdifficulty, difficulty, prevHash,
                                leaderPubKey, protoDSBlockHeader.blocknum(),
                                protoDSBlockHeader.epochnum(), gasprice, swInfo,
                                powDSWinners, hash, committeeHash);

  return true;
}

bool ProtobufToDSBlock(const ProtoDSBlock& protoDSBlock, DSBlock& dsBlock) {
  // Deserialize header

  if (!CheckRequiredFieldsProtoDSBlock(protoDSBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlock failed.");
    return false;
  }

  const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoHeader =
      protoDSBlock.header();

  DSBlockHeader header;

  if (!ProtobufToDSBlockHeader(protoHeader, header)) {
    return false;
  }

  dsBlock = DSBlock(header, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoDSBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, dsBlock);
}

void MicroBlockHeaderToProtobuf(
    const MicroBlockHeader& microBlockHeader,
    ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader) {
  protoMicroBlockHeader.set_type(microBlockHeader.GetType());
  protoMicroBlockHeader.set_version(microBlockHeader.GetVersion());
  protoMicroBlockHeader.set_shardid(microBlockHeader.GetShardId());
  protoMicroBlockHeader.set_gaslimit(microBlockHeader.GetGasLimit());
  protoMicroBlockHeader.set_gasused(microBlockHeader.GetGasUsed());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      microBlockHeader.GetRewards(), *protoMicroBlockHeader.mutable_rewards());
  protoMicroBlockHeader.set_prevhash(microBlockHeader.GetPrevHash().data(),
                                     microBlockHeader.GetPrevHash().size);
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

  protoMicroBlockHeader.set_committeehash(
      microBlockHeader.GetCommitteeHash().data(),
      microBlockHeader.GetCommitteeHash().size);
}

void DSPowSolutionToProtobuf(const DSPowSolution& powSolution,
                             DSPoWSubmission& dsPowSubmission) {
  dsPowSubmission.mutable_data()->set_blocknumber(powSolution.GetBlockNumber());
  dsPowSubmission.mutable_data()->set_difficultylevel(
      powSolution.GetDifficultyLevel());

  SerializableToProtobufByteArray(
      powSolution.GetSubmitterPeer(),
      *dsPowSubmission.mutable_data()->mutable_submitterpeer());

  SerializableToProtobufByteArray(
      powSolution.GetSubmitterKey(),
      *dsPowSubmission.mutable_data()->mutable_submitterpubkey());

  dsPowSubmission.mutable_data()->set_nonce(powSolution.GetNonce());
  dsPowSubmission.mutable_data()->set_resultinghash(
      powSolution.GetResultingHash());
  dsPowSubmission.mutable_data()->set_mixhash(powSolution.GetMixHash());
  dsPowSubmission.mutable_data()->set_lookupid(powSolution.GetLookupId());

  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      powSolution.GetGasPrice(),
      *dsPowSubmission.mutable_data()->mutable_gasprice());

  SerializableToProtobufByteArray(powSolution.GetSignature(),
                                  *dsPowSubmission.mutable_signature());
}

void ProtobufToDSPowSolution(const DSPoWSubmission& dsPowSubmission,
                             DSPowSolution& powSolution) {
  const uint64_t& blockNumber = dsPowSubmission.data().blocknumber();
  const uint8_t& difficultyLevel = dsPowSubmission.data().difficultylevel();
  Peer submitterPeer;
  ProtobufByteArrayToSerializable(dsPowSubmission.data().submitterpeer(),
                                  submitterPeer);
  PubKey submitterKey;
  ProtobufByteArrayToSerializable(dsPowSubmission.data().submitterpubkey(),
                                  submitterKey);
  const uint64_t& nonce = dsPowSubmission.data().nonce();
  const std::string& resultingHash = dsPowSubmission.data().resultinghash();
  const std::string& mixHash = dsPowSubmission.data().mixhash();
  const uint32_t& lookupId = dsPowSubmission.data().lookupid();
  boost::multiprecision::uint128_t gasPrice;
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      dsPowSubmission.data().gasprice(), gasPrice);
  Signature signature;
  ProtobufByteArrayToSerializable(dsPowSubmission.signature(), signature);

  DSPowSolution result(blockNumber, difficultyLevel, submitterPeer,
                       submitterKey, nonce, resultingHash, mixHash, lookupId,
                       gasPrice, signature);
  powSolution = result;
}

void MicroBlockToProtobuf(const MicroBlock& microBlock,
                          ProtoMicroBlock& protoMicroBlock) {
  // Serialize header

  ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader* protoHeader =
      protoMicroBlock.mutable_header();

  const MicroBlockHeader& header = microBlock.GetHeader();

  MicroBlockHeaderToProtobuf(header, *protoHeader);

  // Serialize body

  for (const auto& hash : microBlock.GetTranHashes()) {
    protoMicroBlock.add_tranhashes(hash.data(), hash.size);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoMicroBlock.mutable_blockbase();

  BlockBaseToProtobuf(microBlock, *protoBlockBase);
}

bool ProtobufToMicroBlockHeader(
    const ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader,
    MicroBlockHeader& microBlockHeader) {
  if (!CheckRequiredFieldsProtoMicroBlockMicroBlockHeader(
          protoMicroBlockHeader)) {
    LOG_GENERAL(WARNING,
                "CheckRequiredFieldsProtoMicroBlockMicroBlockHeader failed.");
    return false;
  }

  uint64_t gasLimit;
  uint64_t gasUsed;
  uint128_t rewards;
  BlockHash prevHash;
  TxnHash txRootHash;
  PubKey minerPubKey;
  BlockHash dsBlockHash;
  StateHash stateDeltaHash;
  TxnHash tranReceiptHash;
  CommitteeHash committeeHash;

  gasLimit = protoMicroBlockHeader.gaslimit();
  gasUsed = protoMicroBlockHeader.gasused();
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoMicroBlockHeader.rewards(), rewards);

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.prevhash(),
                                    prevHash.asArray())) {
    return false;
  }

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.txroothash(),
                                    txRootHash.asArray())) {
    return false;
  }

  ProtobufByteArrayToSerializable(protoMicroBlockHeader.minerpubkey(),
                                  minerPubKey);

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.statedeltahash(),
                                    stateDeltaHash.asArray())) {
    return false;
  }

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.tranreceipthash(),
                                    tranReceiptHash.asArray())) {
    return false;
  }

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.committeehash(),
                                    committeeHash.asArray())) {
    return false;
  }

  microBlockHeader = MicroBlockHeader(
      protoMicroBlockHeader.type(), protoMicroBlockHeader.version(),
      protoMicroBlockHeader.shardid(), gasLimit, gasUsed, rewards, prevHash,
      protoMicroBlockHeader.epochnum(),
      {txRootHash, stateDeltaHash, tranReceiptHash},
      protoMicroBlockHeader.numtxs(), minerPubKey,
      protoMicroBlockHeader.dsblocknum(), committeeHash);

  return true;
}

bool ProtobufToMicroBlock(const ProtoMicroBlock& protoMicroBlock,
                          MicroBlock& microBlock) {
  if (!CheckRequiredFieldsProtoMicroBlock(protoMicroBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMicroBlock failed.");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoHeader =
      protoMicroBlock.header();

  MicroBlockHeader header;

  if (!ProtobufToMicroBlockHeader(protoHeader, header)) {
    return false;
  }

  // Deserialize body

  vector<TxnHash> tranHashes;
  for (const auto& hash : protoMicroBlock.tranhashes()) {
    tranHashes.emplace_back();
    unsigned int size =
        min((unsigned int)hash.size(), (unsigned int)tranHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         tranHashes.back().asArray().begin());
  }

  microBlock = MicroBlock(header, tranHashes, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoMicroBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, microBlock);
}

void MbInfoToProtobuf(const MicroBlockInfo& mbInfo, ProtoMbInfo& ProtoMbInfo) {
  ProtoMbInfo.set_mbhash(mbInfo.m_microBlockHash.data(),
                         mbInfo.m_microBlockHash.size);
  ProtoMbInfo.set_txroot(mbInfo.m_txnRootHash.data(),
                         mbInfo.m_txnRootHash.size);
  ProtoMbInfo.set_shardid(mbInfo.m_shardId);
}

bool ProtobufToMbInfo(const ProtoMbInfo& ProtoMbInfo, MicroBlockInfo& mbInfo) {
  if (!CheckRequiredFieldsProtoMbInfo(ProtoMbInfo)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMbInfo failed.");
    return false;
  }

  copy(ProtoMbInfo.mbhash().begin(),
       ProtoMbInfo.mbhash().begin() +
           min((unsigned int)ProtoMbInfo.mbhash().size(),
               (unsigned int)mbInfo.m_microBlockHash.size),
       mbInfo.m_microBlockHash.asArray().begin());
  copy(ProtoMbInfo.txroot().begin(),
       ProtoMbInfo.txroot().begin() +
           min((unsigned int)ProtoMbInfo.txroot().size(),
               (unsigned int)mbInfo.m_txnRootHash.size),
       mbInfo.m_txnRootHash.asArray().begin());
  mbInfo.m_shardId = ProtoMbInfo.shardid();

  return true;
}

void TxBlockHeaderToProtobuf(const TxBlockHeader& txBlockHeader,
                             ProtoTxBlock::TxBlockHeader& protoTxBlockHeader) {
  protoTxBlockHeader.set_type(txBlockHeader.GetType());
  protoTxBlockHeader.set_version(txBlockHeader.GetVersion());
  protoTxBlockHeader.set_gaslimit(txBlockHeader.GetGasLimit());
  protoTxBlockHeader.set_gasused(txBlockHeader.GetGasUsed());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      txBlockHeader.GetRewards(), *protoTxBlockHeader.mutable_rewards());
  protoTxBlockHeader.set_prevhash(txBlockHeader.GetPrevHash().data(),
                                  txBlockHeader.GetPrevHash().size);
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

  protoTxBlockHeader.set_committeehash(txBlockHeader.GetCommitteeHash().data(),
                                       txBlockHeader.GetCommitteeHash().size);
}

void TxBlockToProtobuf(const TxBlock& txBlock, ProtoTxBlock& protoTxBlock) {
  // Serialize header

  ZilliqaMessage::ProtoTxBlock::TxBlockHeader* protoHeader =
      protoTxBlock.mutable_header();

  const TxBlockHeader& header = txBlock.GetHeader();

  TxBlockHeaderToProtobuf(header, *protoHeader);

  for (const auto& mbInfo : txBlock.GetMicroBlockInfos()) {
    auto protoMbInfo = protoTxBlock.add_mbinfos();
    MbInfoToProtobuf(mbInfo, *protoMbInfo);
  }

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoTxBlock.mutable_blockbase();

  BlockBaseToProtobuf(txBlock, *protoBlockBase);
}

bool ProtobufToTxBlockHeader(
    const ProtoTxBlock::TxBlockHeader& protoTxBlockHeader,
    TxBlockHeader& txBlockHeader) {
  if (!CheckRequiredFieldsProtoTxBlockTxBlockHeader(protoTxBlockHeader)) {
    LOG_GENERAL(WARNING,
                "CheckRequiredFieldsProtoTxBlockTxBlockHeader failed.");
    return false;
  }

  uint64_t gasLimit;
  uint64_t gasUsed;
  uint128_t rewards;
  BlockHash prevHash;
  TxBlockHashSet hash;
  PubKey minerPubKey;
  CommitteeHash committeeHash;

  gasLimit = protoTxBlockHeader.gaslimit();
  gasUsed = protoTxBlockHeader.gasused();
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxBlockHeader.rewards(), rewards);
  copy(protoTxBlockHeader.prevhash().begin(),
       protoTxBlockHeader.prevhash().begin() +
           min((unsigned int)protoTxBlockHeader.prevhash().size(),
               (unsigned int)prevHash.size),
       prevHash.asArray().begin());

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

  ProtobufByteArrayToSerializable(protoTxBlockHeader.minerpubkey(),
                                  minerPubKey);

  copy(protoTxBlockHeader.committeehash().begin(),
       protoTxBlockHeader.committeehash().begin() +
           min((unsigned int)protoTxBlockHeader.committeehash().size(),
               (unsigned int)committeeHash.size),
       committeeHash.asArray().begin());

  txBlockHeader = TxBlockHeader(
      protoTxBlockHeader.type(), protoTxBlockHeader.version(), gasLimit,
      gasUsed, rewards, prevHash, protoTxBlockHeader.blocknum(), hash,
      protoTxBlockHeader.numtxs(), minerPubKey, protoTxBlockHeader.dsblocknum(),
      committeeHash);

  return true;
}

bool ProtobufToTxBlock(const ProtoTxBlock& protoTxBlock, TxBlock& txBlock) {
  if (!CheckRequiredFieldsProtoTxBlock(protoTxBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTxBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoTxBlock::TxBlockHeader& protoHeader =
      protoTxBlock.header();

  TxBlockHeader header;

  if (!ProtobufToTxBlockHeader(protoHeader, header)) {
    return false;
  }

  // Deserialize body
  vector<MicroBlockInfo> mbInfos;

  for (const auto& protoMbInfo : protoTxBlock.mbinfos()) {
    MicroBlockInfo mbInfo;
    if (!ProtobufToMbInfo(protoMbInfo, mbInfo)) {
      continue;
    }
    mbInfos.emplace_back(mbInfo);
  }

  txBlock = TxBlock(header, mbInfos, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoTxBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, txBlock);
}

void VCBlockHeaderToProtobuf(const VCBlockHeader& vcBlockHeader,
                             ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  protoVCBlockHeader.set_prevhash(vcBlockHeader.GetPrevHash().data(),
                                  vcBlockHeader.GetPrevHash().size);
  protoVCBlockHeader.set_viewchangedsepochno(
      vcBlockHeader.GetVieWChangeDSEpochNo());
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
  protoVCBlockHeader.set_committeehash(vcBlockHeader.GetCommitteeHash().data(),
                                       vcBlockHeader.GetCommitteeHash().size);
}

void VCBlockToProtobuf(const VCBlock& vcBlock, ProtoVCBlock& protoVCBlock) {
  // Serialize header

  ZilliqaMessage::ProtoVCBlock::VCBlockHeader* protoHeader =
      protoVCBlock.mutable_header();

  const VCBlockHeader& header = vcBlock.GetHeader();

  VCBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoVCBlock.mutable_blockbase();

  BlockBaseToProtobuf(vcBlock, *protoBlockBase);
}

bool ProtobufToVCBlockHeader(
    const ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
    VCBlockHeader& vcBlockHeader) {
  if (!CheckRequiredFieldsProtoVCBlockVCBlockHeader(protoVCBlockHeader)) {
    LOG_GENERAL(WARNING,
                "CheckRequiredFieldsProtoVCBlockVCBlockHeader failed.");
    return false;
  }

  Peer candidateLeaderNetworkInfo;
  PubKey candidateLeaderPubKey;
  CommitteeHash committeeHash;
  BlockHash prevHash;
  vector<pair<PubKey, Peer>> faultyLeaders;

  ProtobufByteArrayToSerializable(
      protoVCBlockHeader.candidateleadernetworkinfo(),
      candidateLeaderNetworkInfo);
  ProtobufByteArrayToSerializable(protoVCBlockHeader.candidateleaderpubkey(),
                                  candidateLeaderPubKey);

  ProtobufToFaultyDSMembers(protoVCBlockHeader, faultyLeaders);

  copy(protoVCBlockHeader.prevhash().begin(),
       protoVCBlockHeader.prevhash().begin() +
           min((unsigned int)protoVCBlockHeader.prevhash().size(),
               (unsigned int)prevHash.size),
       prevHash.asArray().begin());
  copy(protoVCBlockHeader.committeehash().begin(),
       protoVCBlockHeader.committeehash().begin() +
           min((unsigned int)protoVCBlockHeader.committeehash().size(),
               (unsigned int)committeeHash.size),
       committeeHash.asArray().begin());

  vcBlockHeader = VCBlockHeader(
      protoVCBlockHeader.viewchangedsepochno(),
      protoVCBlockHeader.viewchangeepochno(),
      protoVCBlockHeader.viewchangestate(), candidateLeaderNetworkInfo,
      candidateLeaderPubKey, protoVCBlockHeader.vccounter(), faultyLeaders,
      committeeHash, prevHash);

  return true;
}

bool ProtobufToVCBlock(const ProtoVCBlock& protoVCBlock, VCBlock& vcBlock) {
  if (!CheckRequiredFieldsProtoVCBlock(protoVCBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlock failed.");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoHeader =
      protoVCBlock.header();

  VCBlockHeader header;

  if (!ProtobufToVCBlockHeader(protoHeader, header)) {
    return false;
  }

  vcBlock = VCBlock(header, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoVCBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, vcBlock);
}

void FallbackBlockHeaderToProtobuf(
    const FallbackBlockHeader& fallbackBlockHeader,
    ProtoFallbackBlock::FallbackBlockHeader& protoFallbackBlockHeader) {
  protoFallbackBlockHeader.set_prevhash(
      fallbackBlockHeader.GetPrevHash().data(),
      fallbackBlockHeader.GetPrevHash().size);
  protoFallbackBlockHeader.set_fallbackdsepochno(
      fallbackBlockHeader.GetFallbackDSEpochNo());
  protoFallbackBlockHeader.set_fallbackepochno(
      fallbackBlockHeader.GetFallbackEpochNo());
  protoFallbackBlockHeader.set_fallbackstate(
      fallbackBlockHeader.GetFallbackState());
  protoFallbackBlockHeader.set_stateroothash(
      fallbackBlockHeader.GetStateRootHash().data(),
      fallbackBlockHeader.GetStateRootHash().size);
  protoFallbackBlockHeader.set_leaderconsensusid(
      fallbackBlockHeader.GetLeaderConsensusId());
  SerializableToProtobufByteArray(
      fallbackBlockHeader.GetLeaderNetworkInfo(),
      *protoFallbackBlockHeader.mutable_leadernetworkinfo());
  SerializableToProtobufByteArray(
      fallbackBlockHeader.GetLeaderPubKey(),
      *protoFallbackBlockHeader.mutable_leaderpubkey());
  protoFallbackBlockHeader.set_shardid(fallbackBlockHeader.GetShardId());

  protoFallbackBlockHeader.set_committeehash(
      fallbackBlockHeader.GetCommitteeHash().data(),
      fallbackBlockHeader.GetCommitteeHash().size);
}

void FallbackBlockToProtobuf(const FallbackBlock& fallbackBlock,
                             ProtoFallbackBlock& protoFallbackBlock) {
  // Serialize header

  ZilliqaMessage::ProtoFallbackBlock::FallbackBlockHeader* protoHeader =
      protoFallbackBlock.mutable_header();

  const FallbackBlockHeader& header = fallbackBlock.GetHeader();

  FallbackBlockHeaderToProtobuf(header, *protoHeader);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      protoFallbackBlock.mutable_blockbase();

  BlockBaseToProtobuf(fallbackBlock, *protoBlockBase);
}

void ProtobufToFallbackBlockHeader(
    const ProtoFallbackBlock::FallbackBlockHeader& protoFallbackBlockHeader,
    FallbackBlockHeader& fallbackBlockHeader) {
  Peer leaderNetworkInfo;
  PubKey leaderPubKey;
  StateHash stateRootHash;
  CommitteeHash committeeHash;
  BlockHash prevHash;

  ProtobufByteArrayToSerializable(protoFallbackBlockHeader.leadernetworkinfo(),
                                  leaderNetworkInfo);
  ProtobufByteArrayToSerializable(protoFallbackBlockHeader.leaderpubkey(),
                                  leaderPubKey);

  copy(protoFallbackBlockHeader.prevhash().begin(),
       protoFallbackBlockHeader.prevhash().begin() +
           min((unsigned int)protoFallbackBlockHeader.prevhash().size(),
               (unsigned int)prevHash.size),
       prevHash.asArray().begin());

  copy(protoFallbackBlockHeader.stateroothash().begin(),
       protoFallbackBlockHeader.stateroothash().begin() +
           min((unsigned int)protoFallbackBlockHeader.stateroothash().size(),
               (unsigned int)stateRootHash.size),
       stateRootHash.asArray().begin());

  copy(protoFallbackBlockHeader.committeehash().begin(),
       protoFallbackBlockHeader.committeehash().begin() +
           min((unsigned int)protoFallbackBlockHeader.committeehash().size(),
               (unsigned int)committeeHash.size),
       committeeHash.asArray().begin());

  fallbackBlockHeader = FallbackBlockHeader(
      protoFallbackBlockHeader.fallbackdsepochno(),
      protoFallbackBlockHeader.fallbackepochno(),
      protoFallbackBlockHeader.fallbackstate(), {stateRootHash},
      protoFallbackBlockHeader.leaderconsensusid(), leaderNetworkInfo,
      leaderPubKey, protoFallbackBlockHeader.shardid(), committeeHash,
      prevHash);
}

bool ProtobufToFallbackBlock(const ProtoFallbackBlock& protoFallbackBlock,
                             FallbackBlock& fallbackBlock) {
  // Deserialize header
  const ZilliqaMessage::ProtoFallbackBlock::FallbackBlockHeader& protoHeader =
      protoFallbackBlock.header();

  FallbackBlockHeader header;

  ProtobufToFallbackBlockHeader(protoHeader, header);

  fallbackBlock = FallbackBlock(header, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoFallbackBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, fallbackBlock);
}

bool SetConsensusAnnouncementCore(
    ZilliqaMessage::ConsensusAnnouncement& announcement,
    const uint32_t consensusID, uint64_t blockNumber, const bytes& blockHash,
    const uint16_t leaderID, const pair<PrivKey, PubKey>& leaderKey) {
  LOG_MARKER();

  // Set the consensus parameters

  announcement.mutable_consensusinfo()->set_consensusid(consensusID);
  announcement.mutable_consensusinfo()->set_blocknumber(blockNumber);
  announcement.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                      blockHash.size());
  announcement.mutable_consensusinfo()->set_leaderid(leaderID);

  if (!announcement.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ConsensusAnnouncement.ConsensusInfo initialization failed.");
    return false;
  }

  // Sign the announcement

  bytes inputToSigning;

  switch (announcement.announcement_case()) {
    case ConsensusAnnouncement::AnnouncementCase::kDsblock:
      if (!announcement.dsblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement dsblock content not initialized.");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSize() +
                            announcement.dsblock().ByteSize());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSize());
      announcement.dsblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSize(),
          announcement.dsblock().ByteSize());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kMicroblock:
      if (!announcement.microblock().IsInitialized()) {
        LOG_GENERAL(WARNING,
                    "Announcement microblock content not initialized.");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSize() +
                            announcement.microblock().ByteSize());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSize());
      announcement.microblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSize(),
          announcement.microblock().ByteSize());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kFinalblock:
      if (!announcement.finalblock().IsInitialized()) {
        LOG_GENERAL(WARNING,
                    "Announcement finalblock content not initialized.");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSize() +
                            announcement.finalblock().ByteSize());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSize());
      announcement.finalblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSize(),
          announcement.finalblock().ByteSize());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kVcblock:
      if (!announcement.vcblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement vcblock content not initialized.");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSize() +
                            announcement.vcblock().ByteSize());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSize());
      announcement.vcblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSize(),
          announcement.vcblock().ByteSize());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kFallbackblock:
      if (!announcement.fallbackblock().IsInitialized()) {
        LOG_GENERAL(WARNING,
                    "Announcement fallbackblock content not initialized.");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSize() +
                            announcement.fallbackblock().ByteSize());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSize());
      announcement.fallbackblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSize(),
          announcement.fallbackblock().ByteSize());
      break;
    case ConsensusAnnouncement::AnnouncementCase::ANNOUNCEMENT_NOT_SET:
    default:
      LOG_GENERAL(WARNING, "Announcement content not set.");
      return false;
  }

  Signature signature;
  if (!Schnorr::GetInstance().Sign(inputToSigning, leaderKey.first,
                                   leaderKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign announcement.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *announcement.mutable_signature());

  return announcement.IsInitialized();
}

bool GetConsensusAnnouncementCore(
    const ZilliqaMessage::ConsensusAnnouncement& announcement,
    const uint32_t consensusID, const uint64_t blockNumber,
    const bytes& blockHash, const uint16_t leaderID, const PubKey& leaderKey) {
  LOG_MARKER();

  // Check the consensus parameters

  if (announcement.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << announcement.consensusinfo().consensusid());
    return false;
  }

  if (announcement.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << announcement.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = announcement.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  if (announcement.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << announcement.consensusinfo().leaderid());
    return false;
  }

  // Verify the signature
  bytes tmp;

  if (announcement.has_dsblock() && announcement.dsblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSize() +
               announcement.dsblock().ByteSize());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSize());
    announcement.dsblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSize(),
        announcement.dsblock().ByteSize());
  } else if (announcement.has_microblock() &&
             announcement.microblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSize() +
               announcement.microblock().ByteSize());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSize());
    announcement.microblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSize(),
        announcement.microblock().ByteSize());
  } else if (announcement.has_finalblock() &&
             announcement.finalblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSize() +
               announcement.finalblock().ByteSize());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSize());
    announcement.finalblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSize(),
        announcement.finalblock().ByteSize());
  } else if (announcement.has_vcblock() &&
             announcement.vcblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSize() +
               announcement.vcblock().ByteSize());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSize());
    announcement.vcblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSize(),
        announcement.vcblock().ByteSize());
  } else if (announcement.has_fallbackblock() &&
             announcement.fallbackblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSize() +
               announcement.fallbackblock().ByteSize());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSize());
    announcement.fallbackblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSize(),
        announcement.fallbackblock().ByteSize());
  } else {
    LOG_GENERAL(WARNING, "Announcement content not set.");
    return false;
  }

  Signature signature;

  ProtobufByteArrayToSerializable(announcement.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in announcement. leaderID = "
                             << leaderID << " leaderKey = " << leaderKey);
    return false;
  }

  return true;
}

// ============================================================================
// Primitives
// ============================================================================

bool Messenger::GetDSCommitteeHash(const deque<pair<PubKey, Peer>>& dsCommittee,
                                   CommitteeHash& dst) {
  ProtoCommittee protoCommittee;

  DSCommitteeToProtoCommittee(dsCommittee, protoCommittee);

  if (!protoCommittee.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoCommittee initialization failed.");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed.");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::GetShardHash(const Shard& shard, CommitteeHash& dst) {
  ProtoCommittee protoCommittee;

  ShardToProtoCommittee(shard, protoCommittee);

  if (!protoCommittee.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoCommittee initialization failed.");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed.");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::GetShardingStructureHash(const DequeOfShard& shards,
                                         ShardingHash& dst) {
  ProtoShardingStructure protoShardingStructure;

  ShardingStructureToProtobuf(shards, protoShardingStructure);

  if (!protoShardingStructure.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure initialization failed.");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoShardingStructure, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure serialization failed.");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::SetAccount(bytes& dst, const unsigned int offset,
                           const Account& account) {
  ProtoAccount result;

  AccountToProtobuf(account, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetAccount(const bytes& src, const unsigned int offset,
                           Account& account) {
  ProtoAccount result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
    return false;
  }

  Address address;

  if (!ProtobufToAccount(result, account, address)) {
    LOG_GENERAL(WARNING, "ProtobufToAccount failed.");
    return false;
  }

  return true;
}

bool Messenger::SetAccountDelta(bytes& dst, const unsigned int offset,
                                Account* oldAccount,
                                const Account& newAccount) {
  ProtoAccount result;

  AccountDeltaToProtobuf(oldAccount, newAccount, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

// bool Messenger::GetAccountDelta(const bytes& src, const unsigned int offset,
//                                 Account& account, const bool fullCopy) {
//   ProtoAccount result;

//   result.ParseFromArray(src.data() + offset, src.size() - offset);

//   if (!result.IsInitialized()) {
//     LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
//     return false;
//   }

//   if (!ProtobufToAccountDelta(result, account, addr, fullCopy)) {
//     LOG_GENERAL(WARNING, "ProtobufToAccountDelta failed.");
//     return false;
//   }

//   return true;
// }

template <class MAP>
bool Messenger::SetAccountStore(bytes& dst, const unsigned int offset,
                                const MAP& addressToAccount) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Debug: Total number of accounts to serialize: "
                        << addressToAccount.size());

  for (const auto& entry : addressToAccount) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    AccountToProtobuf(entry.second, *protoEntryAccount);
    if (!protoEntryAccount->IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
      return false;
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <class MAP>
bool Messenger::GetAccountStore(const bytes& src, const unsigned int offset,
                                MAP& addressToAccount) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Debug: Total number of accounts deserialized: "
                        << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());
    if (!ProtobufToAccount(entry.account(), account, address)) {
      LOG_GENERAL(WARNING, "ProtobufToAccount failed for account at address "
                               << entry.address());
      return false;
    }

    addressToAccount[address] = account;
  }

  return true;
}

bool Messenger::GetAccountStore(const bytes& src, const unsigned int offset,
                                AccountStore& accountStore) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Debug: Total number of accounts deserialized: "
                        << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());
    if (!ProtobufToAccount(entry.account(), account, address)) {
      LOG_GENERAL(WARNING, "ProtobufToAccount failed for account at address "
                               << entry.address());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account);
  }

  return true;
}

bool Messenger::SetAccountStoreDelta(bytes& dst, const unsigned int offset,
                                     AccountStoreTemp& accountStoreTemp,
                                     AccountStore& accountStore) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Debug: Total number of account deltas to serialize: "
                        << accountStoreTemp.GetNumOfAccounts());

  for (const auto& entry : *accountStoreTemp.GetAddressToAccount()) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    AccountDeltaToProtobuf(accountStore.GetAccount(entry.first), entry.second,
                           *protoEntryAccount);
    if (!protoEntryAccount->IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoAccount initialization failed.");
      return false;
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::StateDeltaToAddressMap(
    const bytes& src, const unsigned int offset,
    unordered_map<Address, int256_t>& accountMap) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());

    uint128_t tmpNumber;

    ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
        entry.account().balance(), tmpNumber);

    int256_t balanceDelta = entry.account().numbersign()
                                ? tmpNumber.convert_to<int256_t>()
                                : 0 - tmpNumber.convert_to<int256_t>();

    accountMap.insert(make_pair(address, balanceDelta));
  }

  return true;
}

bool Messenger::GetAccountStoreDelta(const bytes& src,
                                     const unsigned int offset,
                                     AccountStore& accountStore,
                                     const bool reversible) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO,
              "Total Number of Accounts Delta: " << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());

    const Account* oriAccount = accountStore.GetAccount(address);
    bool fullCopy = false;
    if (oriAccount == nullptr) {
      Account acc(0, 0);
      accountStore.AddAccount(address, acc);
      oriAccount = accountStore.GetAccount(address);
      fullCopy = true;

      if (oriAccount == nullptr) {
        LOG_GENERAL(WARNING, "Failed to create account for " << address);
        return false;
      }
    }

    account = *oriAccount;
    if (!ProtobufToAccountDelta(entry.account(), account, address, fullCopy)) {
      LOG_GENERAL(WARNING,
                  "ProtobufToAccountDelta failed for account at address "
                      << entry.address());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account, fullCopy,
                                                 reversible);
  }

  return true;
}

bool Messenger::GetAccountStoreDelta(const bytes& src,
                                     const unsigned int offset,
                                     AccountStoreTemp& accountStoreTemp) {
  ProtoAccountStore result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO,
              "Total Number of Accounts Delta: " << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account;

    copy(entry.address().begin(),
         entry.address().begin() + min((unsigned int)entry.address().size(),
                                       (unsigned int)address.size),
         address.asArray().begin());

    const Account* oriAccount = accountStoreTemp.GetAccount(address);
    bool fullCopy = false;
    if (oriAccount == nullptr) {
      Account acc(0, 0);
      LOG_GENERAL(INFO, "Creating new account: " << address);
      accountStoreTemp.AddAccount(address, acc);
      fullCopy = true;
    }

    oriAccount = accountStoreTemp.GetAccount(address);

    if (oriAccount == nullptr) {
      LOG_GENERAL(WARNING, "Failed to create account for " << address);
      return false;
    }

    account = *oriAccount;

    if (!ProtobufToAccountDelta(entry.account(), account, address, fullCopy)) {
      LOG_GENERAL(WARNING,
                  "ProtobufToAccountDelta failed for account at address "
                      << entry.address());
      return false;
    }

    accountStoreTemp.AddAccountDuringDeserialization(address, account);
  }

  return true;
}

bool Messenger::GetMbInfoHash(const std::vector<MicroBlockInfo>& mbInfos,
                              MBInfoHash& dst) {
  bytes tmp;

  for (const auto& mbInfo : mbInfos) {
    ProtoMbInfo ProtoMbInfo;

    MbInfoToProtobuf(mbInfo, ProtoMbInfo);

    if (!ProtoMbInfo.IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoMbInfo initialization failed.");
      continue;
    }

    SerializeToArray(ProtoMbInfo, tmp, tmp.size());
  }

  // Fix software crash because of tmp is empty triggered assertion in
  // sha2.update.git
  if (tmp.empty()) {
    LOG_GENERAL(WARNING, "ProtoMbInfo is empty, proceed without it.");
    return true;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::SetDSBlockHeader(bytes& dst, const unsigned int offset,
                                 const DSBlockHeader& dsBlockHeader,
                                 bool concreteVarsOnly) {
  ProtoDSBlock::DSBlockHeader result;

  DSBlockHeaderToProtobuf(dsBlockHeader, result, concreteVarsOnly);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSBlockHeader(const bytes& src, const unsigned int offset,
                                 DSBlockHeader& dsBlockHeader) {
  ProtoDSBlock::DSBlockHeader result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed.");
    return false;
  }

  return ProtobufToDSBlockHeader(result, dsBlockHeader);
}

bool Messenger::SetDSBlock(bytes& dst, const unsigned int offset,
                           const DSBlock& dsBlock) {
  ProtoDSBlock result;

  DSBlockToProtobuf(dsBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSBlock(const bytes& src, const unsigned int offset,
                           DSBlock& dsBlock) {
  ProtoDSBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed.");
    return false;
  }

  return ProtobufToDSBlock(result, dsBlock);
}

bool Messenger::SetMicroBlockHeader(bytes& dst, const unsigned int offset,
                                    const MicroBlockHeader& microBlockHeader) {
  ProtoMicroBlock::MicroBlockHeader result;

  MicroBlockHeaderToProtobuf(microBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoMicroBlock::MicroBlockHeader initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMicroBlockHeader(const bytes& src, const unsigned int offset,
                                    MicroBlockHeader& microBlockHeader) {
  ProtoMicroBlock::MicroBlockHeader result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoMicroBlock::MicroBlockHeader initialization failed.");
    return false;
  }

  return ProtobufToMicroBlockHeader(result, microBlockHeader);
}

bool Messenger::SetMicroBlock(bytes& dst, const unsigned int offset,
                              const MicroBlock& microBlock) {
  ProtoMicroBlock result;

  MicroBlockToProtobuf(microBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMicroBlock(const bytes& src, const unsigned int offset,
                              MicroBlock& microBlock) {
  ProtoMicroBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed.");
    return false;
  }

  return ProtobufToMicroBlock(result, microBlock);
}

bool Messenger::SetTxBlockHeader(bytes& dst, const unsigned int offset,
                                 const TxBlockHeader& txBlockHeader) {
  ProtoTxBlock::TxBlockHeader result;

  TxBlockHeaderToProtobuf(txBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTxBlockHeader(const bytes& src, const unsigned int offset,
                                 TxBlockHeader& txBlockHeader) {
  ProtoTxBlock::TxBlockHeader result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed.");
    return false;
  }

  return ProtobufToTxBlockHeader(result, txBlockHeader);
}

bool Messenger::SetTxBlock(bytes& dst, const unsigned int offset,
                           const TxBlock& txBlock) {
  ProtoTxBlock result;

  TxBlockToProtobuf(txBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTxBlock(const bytes& src, const unsigned int offset,
                           TxBlock& txBlock) {
  ProtoTxBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed.");
    return false;
  }

  return ProtobufToTxBlock(result, txBlock);
}

bool Messenger::SetVCBlockHeader(bytes& dst, const unsigned int offset,
                                 const VCBlockHeader& vcBlockHeader) {
  ProtoVCBlock::VCBlockHeader result;

  VCBlockHeaderToProtobuf(vcBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock::VCBlockHeader initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetVCBlockHeader(const bytes& src, const unsigned int offset,
                                 VCBlockHeader& vcBlockHeader) {
  ProtoVCBlock::VCBlockHeader result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock::VCBlockHeader initialization failed.");
    return false;
  }

  return ProtobufToVCBlockHeader(result, vcBlockHeader);
}

bool Messenger::SetVCBlock(bytes& dst, const unsigned int offset,
                           const VCBlock& vcBlock) {
  ProtoVCBlock result;

  VCBlockToProtobuf(vcBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetVCBlock(const bytes& src, const unsigned int offset,
                           VCBlock& vcBlock) {
  ProtoVCBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed.");
    return false;
  }

  return ProtobufToVCBlock(result, vcBlock);
}

bool Messenger::SetFallbackBlockHeader(
    bytes& dst, const unsigned int offset,
    const FallbackBlockHeader& fallbackBlockHeader) {
  ProtoFallbackBlock::FallbackBlockHeader result;

  FallbackBlockHeaderToProtobuf(fallbackBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "ProtoFallbackBlock::FallbackBlockHeader initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlockHeader(
    const bytes& src, const unsigned int offset,
    FallbackBlockHeader& fallbackBlockHeader) {
  ProtoFallbackBlock::FallbackBlockHeader result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "ProtoFallbackBlock::FallbackBlockHeader initialization failed.");
    return false;
  }

  ProtobufToFallbackBlockHeader(result, fallbackBlockHeader);

  return true;
}

bool Messenger::SetFallbackBlock(bytes& dst, const unsigned int offset,
                                 const FallbackBlock& fallbackBlock) {
  ProtoFallbackBlock result;

  FallbackBlockToProtobuf(fallbackBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoFallbackBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlock(const bytes& src, const unsigned int offset,
                                 FallbackBlock& fallbackBlock) {
  ProtoFallbackBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoFallbackBlock initialization failed.");
    return false;
  }

  ProtobufToFallbackBlock(result, fallbackBlock);

  return true;
}

bool Messenger::SetTransactionCoreInfo(bytes& dst, const unsigned int offset,
                                       const TransactionCoreInfo& transaction) {
  ProtoTransactionCoreInfo result;

  TransactionCoreInfoToProtobuf(transaction, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction core info initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionCoreInfo(const bytes& src,
                                       const unsigned int offset,
                                       TransactionCoreInfo& transaction) {
  ProtoTransactionCoreInfo result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction core info initialization failed.");
    return false;
  }

  ProtobufToTransactionCoreInfo(result, transaction);

  return true;
}

bool Messenger::SetTransaction(bytes& dst, const unsigned int offset,
                               const Transaction& transaction) {
  ProtoTransaction result;

  TransactionToProtobuf(transaction, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransaction(const bytes& src, const unsigned int offset,
                               Transaction& transaction) {
  ProtoTransaction result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction initialization failed.");
    return false;
  }

  ProtobufToTransaction(result, transaction);

  return true;
}

bool Messenger::SetTransactionFileOffset(
    bytes& dst, const unsigned int offset,
    const std::vector<uint32_t>& txnOffsets) {
  ProtoTxnFileOffset result;
  TransactionOffsetToProtobuf(txnOffsets, result);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction file offset initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionFileOffset(const bytes& src,
                                         const unsigned int offset,
                                         std::vector<uint32_t>& txnOffsets) {
  ProtoTxnFileOffset result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction file offset initialization failed.");
    return false;
  }

  ProtobufToTransactionOffset(result, txnOffsets);
  return true;
}

bool Messenger::SetTransactionArray(bytes& dst, const unsigned int offset,
                                    const std::vector<Transaction>& txns) {
  ProtoTransactionArray result;
  TransactionArrayToProtobuf(txns, result);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction array initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionArray(const bytes& src, const unsigned int offset,
                                    std::vector<Transaction>& txns) {
  ProtoTransactionArray result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Transaction array initialization failed.");
    return false;
  }

  ProtobufToTransactionArray(result, txns);
  return true;
}

bool Messenger::SetTransactionReceipt(
    bytes& dst, const unsigned int offset,
    const TransactionReceipt& transactionReceipt) {
  ProtoTransactionReceipt result;

  TransactionReceiptToProtobuf(transactionReceipt, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "TransactionReceipt initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionReceipt(const bytes& src,
                                      const unsigned int offset,
                                      TransactionReceipt& transactionReceipt) {
  ProtoTransactionReceipt result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "TransactionReceipt initialization failed.");
    return false;
  }

  ProtobufToTransactionReceipt(result, transactionReceipt);

  return true;
}

bool Messenger::SetTransactionWithReceipt(
    bytes& dst, const unsigned int offset,
    const TransactionWithReceipt& transactionWithReceipt) {
  ProtoTransactionWithReceipt result;

  TransactionWithReceiptToProtobuf(transactionWithReceipt, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "TransactionWithReceipt initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionWithReceipt(
    const bytes& src, const unsigned int offset,
    TransactionWithReceipt& transactionWithReceipt) {
  ProtoTransactionWithReceipt result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "TransactionWithReceipt initialization failed.");
    return false;
  }

  ProtobufToTransactionWithReceipt(result, transactionWithReceipt);

  return true;
}

bool Messenger::SetPeer(bytes& dst, const unsigned int offset,
                        const Peer& peer) {
  ProtoPeer result;

  PeerToProtobuf(peer, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Peer initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetPeer(const bytes& src, const unsigned int offset,
                        Peer& peer) {
  ProtoPeer result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Peer initialization failed.");
    return false;
  }

  ProtobufToPeer(result, peer);

  return true;
}

bool Messenger::SetBlockLink(
    bytes& dst, const unsigned int offset,
    const std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink) {
  ProtoBlockLink result;

  result.set_index(get<BlockLinkIndex::INDEX>(blocklink));
  result.set_dsindex(get<BlockLinkIndex::DSINDEX>(blocklink));
  result.set_blocktype(get<BlockLinkIndex::BLOCKTYPE>(blocklink));
  BlockHash blkhash = get<BlockLinkIndex::BLOCKHASH>(blocklink);
  result.set_blockhash(blkhash.data(), blkhash.size);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "Failed to intialize ProtoBlockLink");
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetBlockLink(
    const bytes& src, const unsigned int offset,
    std::tuple<uint64_t, uint64_t, BlockType, BlockHash>& blocklink) {
  ProtoBlockLink result;
  BlockHash blkhash;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoBlockLink initialization failed");
    return false;
  }

  if (!CheckRequiredFieldsProtoBlockLink(result)) {
    LOG_GENERAL(WARNING, "ProtoBlockLink is missing some required fields");
    return false;
  }

  get<BlockLinkIndex::INDEX>(blocklink) = result.index();
  get<BlockLinkIndex::DSINDEX>(blocklink) = result.dsindex();

  if (!CopyWithSizeCheck(result.blockhash(), blkhash.asArray())) {
    return false;
  }

  get<BlockLinkIndex::BLOCKTYPE>(blocklink) = (BlockType)result.blocktype();
  get<BlockLinkIndex::BLOCKHASH>(blocklink) = blkhash;

  return true;
}

bool Messenger::SetFallbackBlockWShardingStructure(
    bytes& dst, const unsigned int offset, const FallbackBlock& fallbackblock,
    const DequeOfShard& shards) {
  ProtoFallbackBlockWShardingStructure result;

  FallbackBlockToProtobuf(fallbackblock, *result.mutable_fallbackblock());
  ShardingStructureToProtobuf(shards, *result.mutable_sharding());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoFallbackBlockWShardingStructure initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlockWShardingStructure(const bytes& src,
                                                   const unsigned int offset,
                                                   FallbackBlock& fallbackblock,
                                                   DequeOfShard& shards) {
  ProtoFallbackBlockWShardingStructure result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoFallbackBlockWShardingStructure initialization failed");
    return false;
  }

  ProtobufToFallbackBlock(result.fallbackblock(), fallbackblock);

  return ProtobufToShardingStructure(result.sharding(), shards);
}

bool Messenger::SetDiagnosticData(bytes& dst, const unsigned int offset,
                                  const DequeOfShard& shards,
                                  const DequeOfDSNode& dsCommittee) {
  ProtoDiagnosticData result;

  ShardingStructureToProtobuf(shards, *result.mutable_shards());
  DSCommitteeToProtobuf(dsCommittee, *result.mutable_dscommittee());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticData initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDiagnosticData(const bytes& src, const unsigned int offset,
                                  DequeOfShard& shards,
                                  DequeOfDSNode& dsCommittee) {
  ProtoDiagnosticData result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticData initialization failed");
    return false;
  }

  ProtobufToShardingStructure(result.shards(), shards);

  return ProtobufToDSCommittee(result.dscommittee(), dsCommittee);
}

// ============================================================================
// Peer Manager messages
// ============================================================================

bool Messenger::SetPMHello(bytes& dst, const unsigned int offset,
                           const pair<PrivKey, PubKey>& key,
                           const uint32_t listenPort) {
  LOG_MARKER();

  PMHello result;

  SerializableToProtobufByteArray(key.second,
                                  *result.mutable_data()->mutable_pubkey());
  result.mutable_data()->set_listenport(listenPort);

  if (result.data().IsInitialized()) {
    bytes tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!Schnorr::GetInstance().Sign(tmp, key.first, key.second, signature)) {
      LOG_GENERAL(WARNING, "Failed to sign PMHello.data.");
      return false;
    }

    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(WARNING, "PMHello.Data initialization failed.");
    return false;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetPMHello(const bytes& src, const unsigned int offset,
                           PubKey& pubKey, uint32_t& listenPort) {
  LOG_MARKER();

  PMHello result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello initialization failed.");
    return false;
  }

  ProtobufByteArrayToSerializable(result.data().pubkey(), pubKey);
  listenPort = result.data().listenport();

  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "PMHello signature wrong.");
    return false;
  }

  return true;
}

// ============================================================================
// Directory Service messages
// ============================================================================

bool Messenger::SetDSPoWSubmission(
    bytes& dst, const unsigned int offset, const uint64_t blockNumber,
    const uint8_t difficultyLevel, const Peer& submitterPeer,
    const pair<PrivKey, PubKey>& submitterKey, const uint64_t nonce,
    const string& resultingHash, const string& mixHash,
    const uint32_t& lookupId, const uint128_t& gasPrice) {
  LOG_MARKER();

  DSPoWSubmission result;

  result.mutable_data()->set_blocknumber(blockNumber);
  result.mutable_data()->set_difficultylevel(difficultyLevel);

  SerializableToProtobufByteArray(
      submitterPeer, *result.mutable_data()->mutable_submitterpeer());
  SerializableToProtobufByteArray(
      submitterKey.second, *result.mutable_data()->mutable_submitterpubkey());

  result.mutable_data()->set_nonce(nonce);
  result.mutable_data()->set_resultinghash(resultingHash);
  result.mutable_data()->set_mixhash(mixHash);
  result.mutable_data()->set_lookupid(lookupId);

  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      gasPrice, *result.mutable_data()->mutable_gasprice());

  if (result.data().IsInitialized()) {
    bytes tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!MultiSig::GetInstance().SignKey(tmp, submitterKey, signature)) {
      LOG_GENERAL(WARNING, "Failed to sign PoW.");
      return false;
    }

    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(WARNING, "DSPoWSubmission.Data initialization failed.");
    return false;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSPoWSubmission(const bytes& src, const unsigned int offset,
                                   uint64_t& blockNumber,
                                   uint8_t& difficultyLevel,
                                   Peer& submitterPeer, PubKey& submitterPubKey,
                                   uint64_t& nonce, string& resultingHash,
                                   string& mixHash, Signature& signature,
                                   uint32_t& lookupId, uint128_t& gasPrice) {
  LOG_MARKER();

  DSPoWSubmission result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed.");
    return false;
  }

  blockNumber = result.data().blocknumber();
  difficultyLevel = result.data().difficultylevel();
  ProtobufByteArrayToSerializable(result.data().submitterpeer(), submitterPeer);
  ProtobufByteArrayToSerializable(result.data().submitterpubkey(),
                                  submitterPubKey);
  nonce = result.data().nonce();
  resultingHash = result.data().resultinghash();
  mixHash = result.data().mixhash();
  lookupId = result.data().lookupid();
  ProtobufByteArrayToSerializable(result.signature(), signature);

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.data().gasprice(),
                                                     gasPrice);

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!MultiSig::GetInstance().VerifyKey(tmp, signature, submitterPubKey)) {
    LOG_GENERAL(WARNING, "PoW submission signature wrong.");
    return false;
  }

  return true;
}

bool Messenger::SetDSMicroBlockSubmission(bytes& dst, const unsigned int offset,
                                          const unsigned char microBlockType,
                                          const uint64_t epochNumber,
                                          const vector<MicroBlock>& microBlocks,
                                          const vector<bytes>& stateDeltas) {
  LOG_MARKER();

  DSMicroBlockSubmission result;

  result.set_microblocktype(microBlockType);
  result.set_epochnumber(epochNumber);
  for (const auto& microBlock : microBlocks) {
    MicroBlockToProtobuf(microBlock, *result.add_microblocks());
  }
  for (const auto& stateDelta : stateDeltas) {
    result.add_statedeltas(stateDelta.data(), stateDelta.size());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetDSPoWPacketSubmission(
    bytes& dst, const unsigned int offset,
    const vector<DSPowSolution>& dsPowSolutions,
    const pair<PrivKey, PubKey>& keys) {
  LOG_MARKER();

  DSPoWPacketSubmission result;

  for (const auto& sol : dsPowSolutions) {
    DSPowSolutionToProtobuf(sol,
                            *result.mutable_data()->add_dspowsubmissions());
  }

  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, keys.first, keys.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DSPoWPacketSubmission");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSPowPacketSubmission(const bytes& src,
                                         const unsigned int offset,
                                         vector<DSPowSolution>& dsPowSolutions,
                                         PubKey& pubKey) {
  LOG_MARKER();

  DSPoWPacketSubmission result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission initialization failed.");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), pubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission signature wrong.");
    return false;
  }

  for (const auto& powSubmission : result.data().dspowsubmissions()) {
    DSPowSolution sol;
    ProtobufToDSPowSolution(powSubmission, sol);
    dsPowSolutions.emplace_back(move(sol));
  }

  return true;
}

bool Messenger::GetDSMicroBlockSubmission(const bytes& src,
                                          const unsigned int offset,
                                          unsigned char& microBlockType,
                                          uint64_t& epochNumber,
                                          vector<MicroBlock>& microBlocks,
                                          vector<bytes>& stateDeltas) {
  LOG_MARKER();

  DSMicroBlockSubmission result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission initialization failed.");
    return false;
  }

  microBlockType = result.microblocktype();
  epochNumber = result.epochnumber();
  for (const auto& proto_mb : result.microblocks()) {
    MicroBlock microBlock;
    ProtobufToMicroBlock(proto_mb, microBlock);
    microBlocks.emplace_back(move(microBlock));
  }
  for (const auto& proto_delta : result.statedeltas()) {
    stateDeltas.emplace_back();
    copy(proto_delta.begin(), proto_delta.end(),
         std::back_inserter(stateDeltas.back()));
  }

  return true;
}

bool Messenger::SetDSDSBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey, const DSBlock& dsBlock,
    const DequeOfShard& shards, const MapOfPubKeyPoW& allPoWs,
    const MapOfPubKeyPoW& dsWinnerPoWs, bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the DSBlock announcement parameters

  DSDSBlockAnnouncement* dsblock = announcement.mutable_dsblock();

  DSBlockToProtobuf(dsBlock, *dsblock->mutable_dsblock());

  AnnouncementShardingStructureToProtobuf(shards, allPoWs,
                                          *dsblock->mutable_sharding());

  for (const auto& kv : dsWinnerPoWs) {
    auto protoDSWinnerPoW = dsblock->add_dswinnerpows();
    SerializableToProtobufByteArray(kv.first,
                                    *protoDSWinnerPoW->mutable_pubkey());
    ProtoPoWSolution* proto_soln = protoDSWinnerPoW->mutable_powsoln();
    const auto soln = kv.second;
    proto_soln->set_nonce(soln.nonce);
    proto_soln->set_result(soln.result.data(), soln.result.size());
    proto_soln->set_mixhash(soln.mixhash.data(), soln.mixhash.size());
    proto_soln->set_lookupid(soln.lookupId);
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        soln.gasPrice, *proto_soln->mutable_gasprice());
  }

  if (!dsblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "DSDSBlockAnnouncement initialization failed. Debug: "
                             << announcement.DebugString());
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed. Debug: "
                             << announcement.DebugString());
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!dsBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "DSBlockHeader serialization failed.");
    return false;
  }

  // Serialize the announcement

  return SerializeToArray(announcement, dst, offset);
}

bool Messenger::GetDSDSBlockAnnouncement(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, DSBlock& dsBlock, DequeOfShard& shards,
    MapOfPubKeyPoW& allPoWs, MapOfPubKeyPoW& dsWinnerPoWs,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed. Debug: "
                             << announcement.DebugString());
    return false;
  }

  if (!announcement.has_dsblock()) {
    LOG_GENERAL(
        WARNING,
        "DSDSBlockAnnouncement initialization failed (no ds block). Debug: "
            << announcement.DebugString());
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed.");
    return false;
  }

  // Get the DSBlock announcement parameters

  const DSDSBlockAnnouncement& dsblock = announcement.dsblock();

  if (!ProtobufToDSBlock(dsblock.dsblock(), dsBlock)) {
    return false;
  }

  ProtobufToShardingStructureAnnouncement(dsblock.sharding(), shards, allPoWs);

  dsWinnerPoWs.clear();
  for (const auto& protoDSWinnerPoW : dsblock.dswinnerpows()) {
    PubKey key;
    std::array<unsigned char, 32> result;
    std::array<unsigned char, 32> mixhash;
    uint128_t gasPrice;

    ProtobufByteArrayToSerializable(protoDSWinnerPoW.pubkey(), key);

    copy(protoDSWinnerPoW.powsoln().result().begin(),
         protoDSWinnerPoW.powsoln().result().begin() +
             min((unsigned int)protoDSWinnerPoW.powsoln().result().size(),
                 (unsigned int)result.size()),
         result.begin());
    copy(protoDSWinnerPoW.powsoln().mixhash().begin(),
         protoDSWinnerPoW.powsoln().mixhash().begin() +
             min((unsigned int)protoDSWinnerPoW.powsoln().mixhash().size(),
                 (unsigned int)mixhash.size()),
         mixhash.begin());
    ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
        protoDSWinnerPoW.powsoln().gasprice(), gasPrice);
    dsWinnerPoWs.emplace(
        key, PoWSolution(protoDSWinnerPoW.powsoln().nonce(), result, mixhash,
                         protoDSWinnerPoW.powsoln().lookupid(), gasPrice));
  }

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!dsBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "DSBlockHeader serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::SetDSFinalBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey, const TxBlock& txBlock,
    const shared_ptr<MicroBlock>& microBlock, bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the FinalBlock announcement parameters

  DSFinalBlockAnnouncement* finalblock = announcement.mutable_finalblock();
  TxBlockToProtobuf(txBlock, *finalblock->mutable_txblock());
  if (microBlock != nullptr) {
    MicroBlockToProtobuf(*microBlock, *finalblock->mutable_microblock());
  } else {
    LOG_GENERAL(WARNING, "microblock is nullptr");
  }

  if (!finalblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "DSFinalBlockAnnouncement initialization failed.");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed.");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!txBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed.");
    return false;
  }

  // Serialize the announcement

  return SerializeToArray(announcement, dst, offset);
}

bool Messenger::GetDSFinalBlockAnnouncement(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, TxBlock& txBlock,
    shared_ptr<MicroBlock>& microBlock, bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed.");
    return false;
  }

  if (!announcement.has_finalblock()) {
    LOG_GENERAL(WARNING, "DSFinalBlockAnnouncement initialization failed.");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed.");
    return false;
  }

  // Get the FinalBlock announcement parameters

  const DSFinalBlockAnnouncement& finalblock = announcement.finalblock();
  if (!ProtobufToTxBlock(finalblock.txblock(), txBlock)) {
    return false;
  }

  if (finalblock.has_microblock()) {
    ProtobufToMicroBlock(finalblock.microblock(), *microBlock);
  } else {
    LOG_GENERAL(WARNING, "Announcement doesn't include ds microblock");
    microBlock = nullptr;
  }

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!txBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::SetDSVCBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey, const VCBlock& vcBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the VCBlock announcement parameters

  DSVCBlockAnnouncement* vcblock = announcement.mutable_vcblock();
  SerializableToProtobufByteArray(vcBlock, *vcblock->mutable_vcblock());

  if (!vcblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "DSVCBlockAnnouncement initialization failed.");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed.");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!vcBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "VCBlockHeader serialization failed.");
    return false;
  }

  // Serialize the announcement

  return SerializeToArray(announcement, dst, offset);
}

bool Messenger::GetDSVCBlockAnnouncement(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, VCBlock& vcBlock, bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed.");
    return false;
  }

  if (!announcement.has_vcblock()) {
    LOG_GENERAL(WARNING, "DSVCBlockAnnouncement initialization failed.");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed.");
    return false;
  }

  // Get the VCBlock announcement parameters

  const DSVCBlockAnnouncement& vcblock = announcement.vcblock();
  ProtobufByteArrayToSerializable(vcblock.vcblock(), vcBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!vcBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "VCBlockHeader serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::SetDSMissingMicroBlocksErrorMsg(
    bytes& dst, const unsigned int offset,
    const vector<BlockHash>& missingMicroBlockHashes, const uint64_t epochNum,
    const uint32_t listenPort) {
  LOG_MARKER();

  DSMissingMicroBlocksErrorMsg result;

  for (const auto& hash : missingMicroBlockHashes) {
    result.add_mbhashes(hash.data(), hash.size);
  }

  result.set_epochnum(epochNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMissingMicroBlocksErrorMsg initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSMissingMicroBlocksErrorMsg(
    const bytes& src, const unsigned int offset,
    vector<BlockHash>& missingMicroBlockHashes, uint64_t& epochNum,
    uint32_t& listenPort) {
  LOG_MARKER();

  DSMissingMicroBlocksErrorMsg result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMissingMicroBlocksErrorMsg initialization failed.");
    return false;
  }

  for (const auto& hash : result.mbhashes()) {
    missingMicroBlockHashes.emplace_back();
    unsigned int size = min((unsigned int)hash.size(),
                            (unsigned int)missingMicroBlockHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         missingMicroBlockHashes.back().asArray().begin());
  }

  epochNum = result.epochnum();
  listenPort = result.listenport();

  return true;
}

// ============================================================================
// Node messages
// ============================================================================

bool Messenger::SetNodeVCDSBlocksMessage(bytes& dst, const unsigned int offset,
                                         const uint32_t shardId,
                                         const DSBlock& dsBlock,
                                         const std::vector<VCBlock>& vcBlocks,
                                         const DequeOfShard& shards) {
  LOG_MARKER();

  NodeDSBlock result;

  result.set_shardid(shardId);
  DSBlockToProtobuf(dsBlock, *result.mutable_dsblock());

  for (const auto& vcblock : vcBlocks) {
    VCBlockToProtobuf(vcblock, *result.add_vcblocks());
  }
  ShardingStructureToProtobuf(shards, *result.mutable_sharding());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeDSBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCDSBlocksMessage(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& shardId, DSBlock& dsBlock,
                                         std::vector<VCBlock>& vcBlocks,
                                         DequeOfShard& shards) {
  LOG_MARKER();

  NodeDSBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeDSBlock initialization failed.");
    return false;
  }

  shardId = result.shardid();
  if (!ProtobufToDSBlock(result.dsblock(), dsBlock)) {
    return false;
  }

  for (const auto& proto_vcblock : result.vcblocks()) {
    VCBlock vcblock;
    if (!ProtobufToVCBlock(proto_vcblock, vcblock)) {
      continue;
    }
    vcBlocks.emplace_back(move(vcblock));
  }

  return ProtobufToShardingStructure(result.sharding(), shards);
}

bool Messenger::SetNodeFinalBlock(bytes& dst, const unsigned int offset,
                                  const uint64_t dsBlockNumber,
                                  const uint32_t consensusID,
                                  const TxBlock& txBlock,
                                  const bytes& stateDelta) {
  LOG_MARKER();

  NodeFinalBlock result;

  result.set_dsblocknumber(dsBlockNumber);
  result.set_consensusid(consensusID);
  TxBlockToProtobuf(txBlock, *result.mutable_txblock());
  result.set_statedelta(stateDelta.data(), stateDelta.size());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFinalBlock(const bytes& src, const unsigned int offset,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  bytes& stateDelta) {
  LOG_MARKER();

  NodeFinalBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed.");
    return false;
  }

  dsBlockNumber = result.dsblocknumber();
  consensusID = result.consensusid();
  if (!ProtobufToTxBlock(result.txblock(), txBlock)) {
    return false;
  }
  stateDelta.resize(result.statedelta().size());
  copy(result.statedelta().begin(), result.statedelta().end(),
       stateDelta.begin());

  return true;
}

bool Messenger::SetNodeMBnForwardTransaction(
    bytes& dst, const unsigned int offset, const MicroBlock& microBlock,
    const vector<TransactionWithReceipt>& txns) {
  LOG_MARKER();

  NodeMBnForwardTransaction result;

  MicroBlockToProtobuf(microBlock, *result.mutable_microblock());

  unsigned int txnsCount = 0;

  for (const auto& txn : txns) {
    SerializableToProtobufByteArray(txn, *result.add_txnswithreceipt());
    txnsCount++;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SetNodeMBnForwardTransaction initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "EpochNum: " << microBlock.GetHeader().GetEpochNum()
                                 << " MBHash: " << microBlock.GetBlockHash()
                                 << " Txns: " << txnsCount);

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeMBnForwardTransaction(const bytes& src,
                                             const unsigned int offset,
                                             MBnForwardedTxnEntry& entry) {
  LOG_MARKER();

  NodeMBnForwardTransaction result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTransaction initialization failed.");
    return false;
  }

  ProtobufToMicroBlock(result.microblock(), entry.m_microBlock);

  unsigned int txnsCount = 0;

  for (const auto& txn : result.txnswithreceipt()) {
    TransactionWithReceipt txr;
    ProtobufByteArrayToSerializable(txn, txr);
    entry.m_transactions.emplace_back(txr);
    txnsCount++;
  }

  LOG_GENERAL(INFO, entry << endl << " Txns: " << txnsCount);

  return true;
}

bool Messenger::SetNodeVCBlock(bytes& dst, const unsigned int offset,
                               const VCBlock& vcBlock) {
  LOG_MARKER();

  NodeVCBlock result;

  VCBlockToProtobuf(vcBlock, *result.mutable_vcblock());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeVCBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCBlock(const bytes& src, const unsigned int offset,
                               VCBlock& vcBlock) {
  LOG_MARKER();

  NodeVCBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeVCBlock initialization failed.");
    return false;
  }

  return ProtobufToVCBlock(result.vcblock(), vcBlock);
}

bool Messenger::SetNodeForwardTxnBlock(
    bytes& dst, const unsigned int offset, const uint64_t& epochNumber,
    const uint64_t& dsBlockNum, const uint32_t& shardId,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const std::vector<Transaction>& txnsCurrent,
    const std::vector<Transaction>& txnsGenerated) {
  LOG_MARKER();

  NodeForwardTxnBlock result;

  result.set_epochnumber(epochNumber);
  result.set_dsblocknum(dsBlockNum);
  result.set_shardid(shardId);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  unsigned int txnsCurrentCount = 0;
  unsigned int txnsGeneratedCount = 0;

  for (const auto& txn : txnsCurrent) {
    TransactionToProtobuf(txn, *result.add_transactions());
    txnsCurrentCount++;
  }

  for (const auto& txn : txnsGenerated) {
    TransactionToProtobuf(txn, *result.add_transactions());
    txnsGeneratedCount++;
  }

  Signature signature;
  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions.");
      return false;
    }
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign transactions.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed.");
    return false;
  }

  LOG_GENERAL(INFO, "Epoch: " << epochNumber << " shardId: " << shardId
                              << " Current txns: " << txnsCurrentCount
                              << " Generated txns: " << txnsGeneratedCount);

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeForwardTxnBlock(const bytes& src,
                                       const unsigned int offset,
                                       uint64_t& epochNumber,
                                       uint64_t& dsBlockNum, uint32_t& shardId,
                                       PubKey& lookupPubKey,
                                       std::vector<Transaction>& txns) {
  LOG_MARKER();

  NodeForwardTxnBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed.");
    return false;
  }

  epochNumber = result.epochnumber();
  dsBlockNum = result.dsblocknum();
  shardId = result.shardid();
  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);

  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions.");
      return false;
    }
    Signature signature;
    ProtobufByteArrayToSerializable(result.signature(), signature);

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions.");
      return false;
    }

    for (const auto& txn : result.transactions()) {
      Transaction t;
      ProtobufToTransaction(txn, t);
      txns.emplace_back(t);
    }
  }

  LOG_GENERAL(INFO, "Epoch: " << epochNumber << " Shard: " << shardId
                              << " Received txns: " << txns.size());

  return true;
}

bool Messenger::SetNodeMicroBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey, const MicroBlock& microBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the MicroBlock announcement parameters

  NodeMicroBlockAnnouncement* microblock = announcement.mutable_microblock();
  MicroBlockToProtobuf(microBlock, *microblock->mutable_microblock());

  if (!microblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMicroBlockAnnouncement initialization failed.");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed.");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!microBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed.");
    return false;
  }

  // Serialize the announcement

  return SerializeToArray(announcement, dst, offset);
}

bool Messenger::GetNodeMicroBlockAnnouncement(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, MicroBlock& microBlock, bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed.");
    return false;
  }

  if (!announcement.has_microblock()) {
    LOG_GENERAL(WARNING, "NodeMicroBlockAnnouncement initialization failed.");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed.");
    return false;
  }

  // Get the MicroBlock announcement parameters

  const NodeMicroBlockAnnouncement& microblock = announcement.microblock();
  ProtobufToMicroBlock(microblock.microblock(), microBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!microBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::SetNodeFallbackBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey, const FallbackBlock& fallbackBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the FallbackBlock announcement parameters

  NodeFallbackBlockAnnouncement* fallbackblock =
      announcement.mutable_fallbackblock();
  SerializableToProtobufByteArray(fallbackBlock,
                                  *fallbackblock->mutable_fallbackblock());

  if (!fallbackblock->IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeFallbackBlockAnnouncement initialization failed.");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed.");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!fallbackBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "FallbackBlockHeader serialization failed.");
    return false;
  }

  // Serialize the announcement

  return SerializeToArray(announcement, dst, offset);
}

bool Messenger::GetNodeFallbackBlockAnnouncement(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PubKey& leaderKey, FallbackBlock& fallbackBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed.");
    return false;
  }

  if (!announcement.has_fallbackblock()) {
    LOG_GENERAL(WARNING,
                "NodeFallbackBlockAnnouncement initialization failed.");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed.");
    return false;
  }

  // Get the FallbackBlock announcement parameters

  const NodeFallbackBlockAnnouncement& fallbackblock =
      announcement.fallbackblock();
  ProtobufByteArrayToSerializable(fallbackblock.fallbackblock(), fallbackBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!fallbackBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "FallbackBlockHeader serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::SetNodeFallbackBlock(bytes& dst, const unsigned int offset,
                                     const FallbackBlock& fallbackBlock) {
  LOG_MARKER();

  NodeFallbackBlock result;

  FallbackBlockToProtobuf(fallbackBlock, *result.mutable_fallbackblock());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFallbackBlock initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFallbackBlock(const bytes& src,
                                     const unsigned int offset,
                                     FallbackBlock& fallbackBlock) {
  LOG_MARKER();

  NodeFallbackBlock result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFallbackBlock initialization failed.");
    return false;
  }

  ProtobufToFallbackBlock(result.fallbackblock(), fallbackBlock);

  return true;
}

bool Messenger::ShardStructureToArray(bytes& dst, const unsigned int offset,
                                      const DequeOfShard& shards) {
  ProtoShardingStructure protoShardingStructure;
  ShardingStructureToProtobuf(shards, protoShardingStructure);

  if (!protoShardingStructure.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure initialization failed.");
    return false;
  }

  if (!SerializeToArray(protoShardingStructure, dst, offset)) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure serialization failed.");
    return false;
  }

  return true;
}

bool Messenger::ArrayToShardStructure(const bytes& src,
                                      const unsigned int offset,
                                      DequeOfShard& shards) {
  ProtoShardingStructure protoShardingStructure;
  protoShardingStructure.ParseFromArray(src.data() + offset,
                                        src.size() - offset);
  return ProtobufToShardingStructure(protoShardingStructure, shards);
}

bool Messenger::SetNodeMissingTxnsErrorMsg(
    bytes& dst, const unsigned int offset,
    const vector<TxnHash>& missingTxnHashes, const uint64_t epochNum,
    const uint32_t listenPort) {
  LOG_MARKER();

  NodeMissingTxnsErrorMsg result;

  for (const auto& hash : missingTxnHashes) {
    LOG_EPOCH(INFO, to_string(epochNum).c_str(), "Missing txn: " << hash);
    result.add_txnhashes(hash.data(), hash.size);
  }

  result.set_epochnum(epochNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMissingTxnsErrorMsg initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeMissingTxnsErrorMsg(const bytes& src,
                                           const unsigned int offset,
                                           vector<TxnHash>& missingTxnHashes,
                                           uint64_t& epochNum,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  NodeMissingTxnsErrorMsg result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMissingTxnsErrorMsg initialization failed.");
    return false;
  }

  for (const auto& hash : result.txnhashes()) {
    missingTxnHashes.emplace_back();
    unsigned int size = min((unsigned int)hash.size(),
                            (unsigned int)missingTxnHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         missingTxnHashes.back().asArray().begin());
  }

  epochNum = result.epochnum();
  listenPort = result.listenport();

  return true;
}

// ============================================================================
// Lookup messages
// ============================================================================

bool Messenger::SetLookupGetSeedPeers(bytes& dst, const unsigned int offset,
                                      const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetSeedPeers result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetSeedPeers(const bytes& src,
                                      const unsigned int offset,
                                      uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetSeedPeers result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetSeedPeers(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<Peer>& candidateSeeds) {
  LOG_MARKER();

  LookupSetSeedPeers result;

  unordered_set<uint32_t> indicesAlreadyAdded;

  random_device rd;
  mt19937 gen(rd());
  uniform_int_distribution<> dis(0, candidateSeeds.size() - 1);

  for (unsigned int i = 0; i < candidateSeeds.size(); i++) {
    uint32_t index = dis(gen);
    while (indicesAlreadyAdded.find(index) != indicesAlreadyAdded.end()) {
      index = dis(gen);
    }
    indicesAlreadyAdded.insert(index);

    SerializableToProtobufByteArray(candidateSeeds.at(index),
                                    *result.add_candidateseeds());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (result.candidateseeds().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.candidateseeds(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds.");
      return false;
    }
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign candidate seeds.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetSeedPeers(const bytes& src,
                                      const unsigned int offset,
                                      PubKey& lookupPubKey,
                                      vector<Peer>& candidateSeeds) {
  LOG_MARKER();

  LookupSetSeedPeers result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed.");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);

  for (const auto& peer : result.candidateseeds()) {
    Peer seedPeer;
    ProtobufByteArrayToSerializable(peer, seedPeer);
    candidateSeeds.emplace_back(seedPeer);
  }

  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.candidateseeds().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.candidateseeds(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in candidate seeds.");
      return false;
    }
  }

  return true;
}

bool Messenger::SetLookupGetDSInfoFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort,
                                           const bool initialDS) {
  LOG_MARKER();

  LookupGetDSInfoFromSeed result;

  result.set_listenport(listenPort);
  result.set_initialds(initialDS);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSInfoFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort,
                                           bool& initialDS) {
  LOG_MARKER();

  LookupGetDSInfoFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed.");
    return false;
  }

  listenPort = result.listenport();
  initialDS = result.initialds();

  return true;
}

bool Messenger::SetLookupSetDSInfoFromSeed(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& senderKey,
    const deque<pair<PubKey, Peer>>& dsNodes, const bool initialDS) {
  LOG_MARKER();

  LookupSetDSInfoFromSeed result;

  DSCommitteeToProtobuf(dsNodes, *result.mutable_dscommittee());

  SerializableToProtobufByteArray(senderKey.second, *result.mutable_pubkey());

  bytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee.");
    return false;
  }

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, senderKey.first, senderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DS committee.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  result.set_initialds(initialDS);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSInfoFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& senderPubKey,
                                           deque<pair<PubKey, Peer>>& dsNodes,
                                           bool& initialDS) {
  LOG_MARKER();

  LookupSetDSInfoFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);
  ProtobufByteArrayToSerializable(result.pubkey(), senderPubKey);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed.");
    return false;
  }

  if (!ProtobufToDSCommittee(result.dscommittee(), dsNodes)) {
    return false;
  }

  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  bytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee.");
    return false;
  }

  initialDS = result.initialds();

  if (!Schnorr::GetInstance().Verify(tmp, signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in DS nodes info.");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetDSBlockFromSeed(bytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetDSBlockFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSBlockFromSeed(const bytes& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetDSBlockFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed.");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetDSBlockFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
    const uint64_t highBlockNum, const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<DSBlock>& dsBlocks) {
  LOG_MARKER();

  LookupSetDSBlockFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  for (const auto& dsblock : dsBlocks) {
    DSBlockToProtobuf(dsblock, *result.add_dsblocks());
  }

  Signature signature;
  if (result.dsblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.dsblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize DS blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign DS blocks.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<DSBlock>& dsBlocks) {
  LOG_MARKER();

  LookupSetDSBlockFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed.");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);

  for (const auto& proto_dsblock : result.dsblocks()) {
    DSBlock dsblock;
    if (!ProtobufToDSBlock(proto_dsblock, dsblock)) {
      continue;
    }
    dsBlocks.emplace_back(dsblock);
  }

  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.dsblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.dsblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize DS blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in DS blocks.");
      return false;
    }
  }

  return true;
}

bool Messenger::SetLookupGetTxBlockFromSeed(bytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetTxBlockFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetTxBlockFromSeed(const bytes& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetTxBlockFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetTxBlockFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
    const uint64_t highBlockNum, const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  LookupSetTxBlockFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);

  for (const auto& txblock : txBlocks) {
    TxBlockToProtobuf(txblock, *result.add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (result.txblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.txblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize tx blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign tx blocks.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  LookupSetTxBlockFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed.");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();

  for (const auto& txblock : result.txblocks()) {
    TxBlock block;
    if (!ProtobufToTxBlock(txblock, block)) {
      continue;
    }
    txBlocks.emplace_back(block);
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.txblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.txblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize tx blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in tx blocks.");
      return false;
    }
  }

  return true;
}

bool Messenger::SetLookupGetStateDeltaFromSeed(bytes& dst,
                                               const unsigned int offset,
                                               const uint64_t blockNum,
                                               const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetStateDeltaFromSeed result;

  result.set_blocknum(blockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStateDeltaFromSeed(const bytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetStateDeltaFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed.");
    return false;
  }

  blockNum = result.blocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetStateDeltaFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t blockNum,
    const std::pair<PrivKey, PubKey>& lookupKey, const bytes& stateDelta) {
  LOG_MARKER();

  LookupSetStateDeltaFromSeed result;

  result.set_blocknum(blockNum);

  result.set_statedelta(stateDelta.data(), stateDelta.size());

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(stateDelta, lookupKey.first,
                                   lookupKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign state delta.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltaFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetStateDeltaFromSeed(const bytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               PubKey& lookupPubKey,
                                               bytes& stateDelta) {
  LOG_MARKER();

  LookupSetStateDeltaFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltaFromSeed initialization failed.");
    return false;
  }

  blockNum = result.blocknum();

  stateDelta.resize(result.statedelta().size());
  std::copy(result.statedelta().begin(), result.statedelta().end(),
            stateDelta.begin());

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(stateDelta, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in state delta.");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetTxBodyFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const bytes& txHash,
                                           const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetTxBodyFromSeed result;

  result.set_txhash(txHash.data(), txHash.size());
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBodyFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetTxBodyFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           TxnHash& txHash,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetTxBodyFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBodyFromSeed initialization failed.");
    return false;
  }

  if (!CopyWithSizeCheck(result.txhash(), txHash.asArray())) {
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetTxBodyFromSeed(
    bytes& dst, const unsigned int offset, const TxnHash& txHash,
    const TransactionWithReceipt& txBody) {
  LOG_MARKER();

  LookupSetTxBodyFromSeed result;

  result.set_txhash(txHash.data(), txHash.size);
  SerializableToProtobufByteArray(txBody, *result.mutable_txbody());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBodyFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxBodyFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           TxnHash& txHash,
                                           TransactionWithReceipt& txBody) {
  LOG_MARKER();

  LookupSetTxBodyFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBodyFromSeed initialization failed.");
    return false;
  }

  if (!CopyWithSizeCheck(result.txhash(), txHash.asArray())) {
    return false;
  }

  ProtobufByteArrayToSerializable(result.txbody(), txBody);

  return true;
}

bool Messenger::SetLookupSetNetworkIDFromSeed(bytes& dst,
                                              const unsigned int offset,
                                              const string& networkID) {
  LOG_MARKER();

  LookupSetNetworkIDFromSeed result;

  result.set_networkid(networkID);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetNetworkIDFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetNetworkIDFromSeed(const bytes& src,
                                              const unsigned int offset,
                                              string& networkID) {
  LOG_MARKER();

  LookupSetNetworkIDFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetNetworkIDFromSeed initialization failed.");
    return false;
  }

  networkID = result.networkid();

  return true;
}

bool Messenger::SetLookupGetStateFromSeed(bytes& dst, const unsigned int offset,
                                          const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetStateFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStateFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetStateFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetStateFromSeed(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const AccountStore& accountStore) {
  LOG_MARKER();

  LookupSetStateFromSeed result;

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;

  bytes tmp;

  if (!accountStore.Serialize(tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize AccountStore.");
    return false;
  }
  result.mutable_accountstore()->set_data(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign accounts.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetStateFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          PubKey& lookupPubKey,
                                          bytes& accountStoreBytes) {
  LOG_MARKER();

  LookupSetStateFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed.");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  copy(result.accountstore().data().begin(), result.accountstore().data().end(),
       back_inserter(accountStoreBytes));

  if (!Schnorr::GetInstance().Verify(accountStoreBytes, signature,
                                     lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in accounts.");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetLookupOffline(bytes& dst, const unsigned int offset,
                                          const uint32_t listenPort) {
  LOG_MARKER();

  LookupSetLookupOffline result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOffline(const bytes& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort) {
  LOG_MARKER();

  LookupSetLookupOffline result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetLookupOnline(bytes& dst, const unsigned int offset,
                                         const uint32_t listenPort,
                                         const PubKey& pubKey) {
  LOG_MARKER();

  LookupSetLookupOnline result;

  result.set_listenport(listenPort);
  SerializableToProtobufByteArray(pubKey, *result.mutable_pubkey());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOnline(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& listenPort, PubKey& pubKey) {
  LOG_MARKER();

  LookupSetLookupOnline result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  ProtobufByteArrayToSerializable(result.pubkey(), pubKey);

  return true;
}

bool Messenger::SetLookupGetOfflineLookups(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetOfflineLookups result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetOfflineLookups(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetOfflineLookups result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetOfflineLookups(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey, const vector<Peer>& nodes) {
  LOG_MARKER();

  LookupSetOfflineLookups result;

  for (const auto& node : nodes) {
    SerializableToProtobufByteArray(node, *result.add_nodes());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.nodes().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.nodes(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes.");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign offline lookup nodes.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetOfflineLookups(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           vector<Peer>& nodes) {
  LOG_MARKER();

  LookupSetOfflineLookups result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed.");
    return false;
  }

  for (const auto& lookup : result.nodes()) {
    Peer node;
    ProtobufByteArrayToSerializable(lookup, node);
    nodes.emplace_back(node);
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.nodes().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.nodes(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in offline lookup nodes.");
      return false;
    }
  }

  return true;
}

bool Messenger::SetLookupGetStartPoWFromSeed(bytes& dst,
                                             const unsigned int offset,
                                             const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetStartPoWFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStartPoWFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStartPoWFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetStartPoWFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStartPoWFromSeed initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetStartPoWFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t blockNumber,
    const std::pair<PrivKey, PubKey>& lookupKey) {
  LOG_MARKER();

  LookupSetStartPoWFromSeed result;

  result.set_blocknumber(blockNumber);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  bytes tmp;
  NumberToArray<uint64_t, sizeof(uint64_t)>(blockNumber, tmp, 0);

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign start PoW message.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStartPoWFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetStartPoWFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             PubKey& lookupPubKey) {
  LOG_MARKER();

  LookupSetStartPoWFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStartPoWFromSeed initialization failed.");
    return false;
  }

  bytes tmp;
  NumberToArray<uint64_t, sizeof(uint64_t)>(result.blocknumber(), tmp, 0);

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in start PoW message.");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetShardsFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetShardsFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetShardsFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetShardsFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetShardsFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetShardsFromSeed initialization failed.");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetShardsFromSeed(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey, const DequeOfShard& shards) {
  LOG_MARKER();

  LookupSetShardsFromSeed result;

  ShardingStructureToProtobuf(shards, *result.mutable_sharding());

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  bytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure.");
    return false;
  }

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign sharding structure.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetShardsFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetShardsFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           DequeOfShard& shards) {
  LOG_MARKER();

  LookupSetShardsFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetShardsFromSeed initialization failed.");
    return false;
  }

  if (!ProtobufToShardingStructure(result.sharding(), shards)) {
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  bytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure.");
    return false;
  }

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in sharding structure.");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetMicroBlockFromLookup(
    bytes& dst, const unsigned int offset,
    const vector<BlockHash>& microBlockHashes, uint32_t portNo) {
  LOG_MARKER();

  LookupGetMicroBlockFromLookup result;

  result.set_portno(portNo);

  for (const auto& hash : microBlockHashes) {
    result.add_mbhashes(hash.data(), hash.size);
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetMicroBlockFromLookup initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetMicroBlockFromLookup(
    const bytes& src, const unsigned int offset,
    vector<BlockHash>& microBlockHashes, uint32_t& portNo) {
  LOG_MARKER();

  LookupGetMicroBlockFromLookup result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetMicroBlockFromLookup initialization failed.");
    return false;
  }

  portNo = result.portno();

  for (const auto& hash : result.mbhashes()) {
    microBlockHashes.emplace_back();
    unsigned int size = min((unsigned int)hash.size(),
                            (unsigned int)microBlockHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         microBlockHashes.back().asArray().begin());
  }

  return true;
}

bool Messenger::SetLookupSetMicroBlockFromLookup(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<MicroBlock>& mbs) {
  LOG_MARKER();
  LookupSetMicroBlockFromLookup result;

  for (const auto& mb : mbs) {
    MicroBlockToProtobuf(mb, *result.add_microblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.microblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.microblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign micro blocks.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetMicroBlockFromLookup initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetMicroBlockFromLookup(const bytes& src,
                                                 const unsigned int offset,
                                                 PubKey& lookupPubKey,
                                                 vector<MicroBlock>& mbs) {
  LOG_MARKER();
  LookupSetMicroBlockFromLookup result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetMicroBlockFromLookup initialization failed");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.microblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.microblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in micro blocks.");
      return false;
    }
  }

  for (const auto& res_mb : result.microblocks()) {
    MicroBlock mb;

    ProtobufToMicroBlock(res_mb, mb);

    mbs.emplace_back(mb);
  }

  return true;
}

bool Messenger::SetLookupGetTxnsFromLookup(bytes& dst,
                                           const unsigned int offset,
                                           const vector<TxnHash>& txnhashes,
                                           uint32_t portNo) {
  LOG_MARKER();

  LookupGetTxnsFromLookup result;

  result.set_portno(portNo);

  for (const auto& txhash : txnhashes) {
    result.add_txnhashes(txhash.data(), txhash.size);
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromLookup initialization failure");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetTxnsFromLookup(const bytes& src,
                                           const unsigned int offset,
                                           vector<TxnHash>& txnhashes,
                                           uint32_t& portNo) {
  LOG_MARKER();

  LookupGetTxnsFromLookup result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  portNo = result.portno();

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromLookup initialization failure");
    return false;
  }

  for (const auto& hash : result.txnhashes()) {
    txnhashes.emplace_back();
    unsigned int size =
        min((unsigned int)hash.size(), (unsigned int)txnhashes.back().size);
    copy(hash.begin(), hash.begin() + size, txnhashes.back().asArray().begin());
  }
  return true;
}

bool Messenger::SetLookupSetTxnsFromLookup(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<TransactionWithReceipt>& txns) {
  LOG_MARKER();

  LookupSetTxnsFromLookup result;

  for (auto const& txn : txns) {
    SerializableToProtobufByteArray(txn, *result.add_transactions());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions.");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign transactions.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxnsFromLookup initialization failure");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxnsFromLookup(
    const bytes& src, const unsigned int offset, PubKey& lookupPubKey,
    vector<TransactionWithReceipt>& txns) {
  LOG_MARKER();

  LookupSetTxnsFromLookup result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxnsFromLookup initialization failed");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions.");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions.");
      return false;
    }
  }

  for (auto const& protoTxn : result.transactions()) {
    TransactionWithReceipt txn;
    ProtobufByteArrayToSerializable(protoTxn, txn);
    txns.emplace_back(txn);
  }

  return true;
}

bool Messenger::SetLookupGetDirectoryBlocksFromSeed(bytes& dst,
                                                    const unsigned int offset,
                                                    const uint32_t portNo,
                                                    const uint64_t& indexNum) {
  LookupGetDirectoryBlocksFromSeed result;

  result.set_portno(portNo);
  result.set_indexnum(indexNum);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDirectoryBlocksFromSeed(const bytes& src,
                                                    const unsigned int offset,
                                                    uint32_t& portNo,
                                                    uint64_t& indexNum) {
  LookupGetDirectoryBlocksFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  portNo = result.portno();

  indexNum = result.indexnum();

  return true;
}

bool Messenger::SetLookupSetDirectoryBlocksFromSeed(
    bytes& dst, const unsigned int offset,
    const vector<
        boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
        directoryBlocks,
    const uint64_t& indexNum) {
  LookupSetDirectoryBlocksFromSeed result;

  result.set_indexnum(indexNum);

  for (const auto& dirblock : directoryBlocks) {
    ProtoSingleDirectoryBlock* proto_dir_blocks = result.add_dirblocks();
    if (dirblock.type() == typeid(DSBlock)) {
      DSBlockToProtobuf(get<DSBlock>(dirblock),
                        *proto_dir_blocks->mutable_dsblock());
    } else if (dirblock.type() == typeid(VCBlock)) {
      VCBlockToProtobuf(get<VCBlock>(dirblock),
                        *proto_dir_blocks->mutable_vcblock());
    } else if (dirblock.type() == typeid(FallbackBlockWShardingStructure)) {
      FallbackBlockToProtobuf(
          get<FallbackBlockWShardingStructure>(dirblock).m_fallbackblock,
          *proto_dir_blocks->mutable_fallbackblockwshard()
               ->mutable_fallbackblock());
      ShardingStructureToProtobuf(
          get<FallbackBlockWShardingStructure>(dirblock).m_shards,
          *proto_dir_blocks->mutable_fallbackblockwshard()->mutable_sharding());
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed initialization failed");
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDirectoryBlocksFromSeed(
    const bytes& src, const unsigned int offset,
    vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
        directoryBlocks,
    uint64_t& indexNum) {
  LookupSetDirectoryBlocksFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  indexNum = result.indexnum();

  for (const auto& dirblock : result.dirblocks()) {
    DSBlock dsblock;
    VCBlock vcblock;
    FallbackBlockWShardingStructure fallbackblockwshard;
    switch (dirblock.directoryblock_case()) {
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kDsblock:
        if (!dirblock.dsblock().IsInitialized()) {
          LOG_GENERAL(WARNING, "DS block not initialized");
          continue;
        }
        if (!ProtobufToDSBlock(dirblock.dsblock(), dsblock)) {
          continue;
        }
        directoryBlocks.emplace_back(dsblock);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kVcblock:
        if (!dirblock.vcblock().IsInitialized()) {
          LOG_GENERAL(WARNING, "VC block not initialized");
          continue;
        }
        if (!ProtobufToVCBlock(dirblock.vcblock(), vcblock)) {
          continue;
        }
        directoryBlocks.emplace_back(vcblock);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kFallbackblockwshard:
        if (!dirblock.fallbackblockwshard().IsInitialized()) {
          LOG_GENERAL(WARNING, "FallbackBlock not initialized");
          continue;
        }
        ProtobufToFallbackBlock(dirblock.fallbackblockwshard().fallbackblock(),
                                fallbackblockwshard.m_fallbackblock);
        if (!ProtobufToShardingStructure(
                dirblock.fallbackblockwshard().sharding(),
                fallbackblockwshard.m_shards)) {
          continue;
        }
        directoryBlocks.emplace_back(fallbackblockwshard);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::
          DIRECTORYBLOCK_NOT_SET:
      default:
        LOG_GENERAL(WARNING, "Error in the blocktype");
        break;
    }
  }
  return true;
}

// ============================================================================
// Consensus messages
// ============================================================================

bool Messenger::SetConsensusCommit(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t backupID,
    const CommitPoint& commitPoint, const CommitPointHash& commitPointHash,
    const pair<PrivKey, PubKey>& backupKey) {
  LOG_MARKER();

  ConsensusCommit result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_backupid(backupID);

  SerializableToProtobufByteArray(
      commitPoint, *result.mutable_consensusinfo()->mutable_commitpoint());

  SerializableToProtobufByteArray(
      commitPointHash,
      *result.mutable_consensusinfo()->mutable_commitpointhash());

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCommit(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
    CommitPoint& commitPoint, CommitPointHash& commitPointHash,
    const deque<pair<PubKey, Peer>>& committeeKeys) {
  LOG_MARKER();

  ConsensusCommit result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  backupID = result.consensusinfo().backupid();

  if (backupID >= committeeKeys.size()) {
    LOG_GENERAL(WARNING, "Backup ID beyond shard size. Backup ID: "
                             << backupID
                             << " Shard size: " << committeeKeys.size());
    return false;
  }

  ProtobufByteArrayToSerializable(result.consensusinfo().commitpoint(),
                                  commitPoint);
  ProtobufByteArrayToSerializable(result.consensusinfo().commitpointhash(),
                                  commitPointHash);

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit.");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusChallenge(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const uint16_t subsetID, const bytes& blockHash,
    const uint16_t leaderID, const CommitPoint& aggregatedCommit,
    const PubKey& aggregatedKey, const Challenge& challenge,
    const pair<PrivKey, PubKey>& leaderKey) {
  LOG_MARKER();

  ConsensusChallenge result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_leaderid(leaderID);
  result.mutable_consensusinfo()->set_subsetid(subsetID);
  SerializableToProtobufByteArray(
      aggregatedCommit,
      *result.mutable_consensusinfo()->mutable_aggregatedcommit());
  SerializableToProtobufByteArray(
      aggregatedKey, *result.mutable_consensusinfo()->mutable_aggregatedkey());
  SerializableToProtobufByteArray(
      challenge, *result.mutable_consensusinfo()->mutable_challenge());

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign challenge.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusChallenge(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, uint16_t& subsetID, const bytes& blockHash,
    const uint16_t leaderID, CommitPoint& aggregatedCommit,
    PubKey& aggregatedKey, Challenge& challenge, const PubKey& leaderKey) {
  LOG_MARKER();

  ConsensusChallenge result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  if (result.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << result.consensusinfo().leaderid());
    return false;
  }

  subsetID = result.consensusinfo().subsetid();

  ProtobufByteArrayToSerializable(result.consensusinfo().aggregatedcommit(),
                                  aggregatedCommit);
  ProtobufByteArrayToSerializable(result.consensusinfo().aggregatedkey(),
                                  aggregatedKey);
  ProtobufByteArrayToSerializable(result.consensusinfo().challenge(),
                                  challenge);

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in challenge.");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusResponse(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const uint16_t subsetID, const bytes& blockHash,
    const uint16_t backupID, const Response& response,
    const pair<PrivKey, PubKey>& backupKey) {
  LOG_MARKER();

  ConsensusResponse result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_backupid(backupID);
  result.mutable_consensusinfo()->set_subsetid(subsetID);
  SerializableToProtobufByteArray(
      response, *result.mutable_consensusinfo()->mutable_response());

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign response.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusResponse(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
    uint16_t& subsetID, Response& response,
    const deque<pair<PubKey, Peer>>& committeeKeys) {
  LOG_MARKER();

  ConsensusResponse result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  backupID = result.consensusinfo().backupid();

  if (backupID >= committeeKeys.size()) {
    LOG_GENERAL(WARNING, "Backup ID beyond shard size. Backup ID: "
                             << backupID
                             << " Shard size: " << committeeKeys.size());
    return false;
  }

  subsetID = result.consensusinfo().subsetid();

  ProtobufByteArrayToSerializable(result.consensusinfo().response(), response);

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in response.");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusCollectiveSig(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const Signature& collectiveSig, const vector<bool>& bitmap,
    const pair<PrivKey, PubKey>& leaderKey) {
  LOG_MARKER();

  ConsensusCollectiveSig result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_leaderid(leaderID);
  SerializableToProtobufByteArray(
      collectiveSig, *result.mutable_consensusinfo()->mutable_collectivesig());
  for (const auto& i : bitmap) {
    result.mutable_consensusinfo()->add_bitmap(i);
  }

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign collectivesig.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCollectiveSig(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    vector<bool>& bitmap, Signature& collectiveSig, const PubKey& leaderKey) {
  LOG_MARKER();

  ConsensusCollectiveSig result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  if (result.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << result.consensusinfo().leaderid());
    return false;
  }

  ProtobufByteArrayToSerializable(result.consensusinfo().collectivesig(),
                                  collectiveSig);

  for (const auto& i : result.consensusinfo().bitmap()) {
    bitmap.emplace_back(i);
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in collectivesig.");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusCommitFailure(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t backupID,
    const bytes& errorMsg, const pair<PrivKey, PubKey>& backupKey) {
  LOG_MARKER();

  ConsensusCommitFailure result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_backupid(backupID);
  result.mutable_consensusinfo()->set_errormsg(errorMsg.data(),
                                               errorMsg.size());

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommitFailure.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit failure.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommitFailure initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCommitFailure(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
    bytes& errorMsg, const deque<pair<PubKey, Peer>>& committeeKeys) {
  LOG_MARKER();

  ConsensusCommitFailure result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommitFailure initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  backupID = result.consensusinfo().backupid();

  if (backupID >= committeeKeys.size()) {
    LOG_GENERAL(WARNING, "Backup ID beyond shard size. Backup ID: "
                             << backupID
                             << " Shard size: " << committeeKeys.size());
    return false;
  }

  errorMsg.resize(result.consensusinfo().errormsg().size());
  copy(result.consensusinfo().errormsg().begin(),
       result.consensusinfo().errormsg().end(), errorMsg.begin());

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit failure.");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusConsensusFailure(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const pair<PrivKey, PubKey>& leaderKey) {
  LOG_MARKER();

  ConsensusConsensusFailure result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_leaderid(leaderID);

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ConsensusConsensusFailure.Data initialization failed.");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign ConsensusConsensusFailure.Data.");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusConsensusFailure initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusConsensusFailure(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& leaderID,
    const PubKey& leaderKey) {
  LOG_MARKER();

  ConsensusConsensusFailure result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusConsensusFailure initialization failed.");
    return false;
  }

  if (result.consensusinfo().consensusid() != consensusID) {
    LOG_GENERAL(WARNING, "Consensus ID mismatch. Expected: "
                             << consensusID << " Actual: "
                             << result.consensusinfo().consensusid());
    return false;
  }

  if (result.consensusinfo().blocknumber() != blockNumber) {
    LOG_GENERAL(WARNING, "Block number mismatch. Expected: "
                             << blockNumber << " Actual: "
                             << result.consensusinfo().blocknumber());
    return false;
  }

  const auto& tmpBlockHash = result.consensusinfo().blockhash();
  if (!std::equal(blockHash.begin(), blockHash.end(), tmpBlockHash.begin(),
                  tmpBlockHash.end(),
                  [](const unsigned char left, const char right) -> bool {
                    return left == (unsigned char)right;
                  })) {
    bytes remoteBlockHash(tmpBlockHash.size());
    std::copy(tmpBlockHash.begin(), tmpBlockHash.end(),
              remoteBlockHash.begin());
    LOG_GENERAL(WARNING,
                "Block hash mismatch. Expected: "
                    << DataConversion::Uint8VecToHexStr(blockHash)
                    << " Actual: "
                    << DataConversion::Uint8VecToHexStr(remoteBlockHash));
    return false;
  }

  if (result.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << result.consensusinfo().leaderid());
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  ProtobufByteArrayToSerializable(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in ConsensusConsensusFailure.");
    return false;
  }

  return true;
}

// ============================================================================
// View change pre check messages
// ============================================================================

bool Messenger::SetLookupGetDSTxBlockFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t dsLowBlockNum,
    const uint64_t dsHighBlockNum, const uint64_t txLowBlockNum,
    const uint64_t txHighBlockNum, const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetDSTxBlockFromSeed result;

  result.set_dslowblocknum(dsLowBlockNum);
  result.set_dshighblocknum(dsHighBlockNum);
  result.set_txlowblocknum(txLowBlockNum);
  result.set_txhighblocknum(txHighBlockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSTxBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSTxBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& dsLowBlockNum,
    uint64_t& dsHighBlockNum, uint64_t& txLowBlockNum, uint64_t& txHighBlockNum,
    uint32_t& listenPort) {
  LOG_MARKER();

  LookupGetDSTxBlockFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSTxBlockFromSeed initialization failed.");
    return false;
  }

  dsLowBlockNum = result.dslowblocknum();
  dsHighBlockNum = result.dshighblocknum();
  txLowBlockNum = result.txlowblocknum();
  txHighBlockNum = result.txhighblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetVCNodeSetDSTxBlockFromSeed(
    bytes& dst, const unsigned int offset,
    const std::pair<PrivKey, PubKey>& lookupKey,
    const vector<DSBlock>& DSBlocks, const vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  VCNodeSetDSTxBlockFromSeed result;

  for (const auto& dsblock : DSBlocks) {
    DSBlockToProtobuf(dsblock, *result.add_dsblocks());
  }

  for (const auto& txblock : txBlocks) {
    TxBlockToProtobuf(txblock, *result.add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  bytes tmp;
  if (result.dsblocks().size() > 0) {
    if (!RepeatableToArray(result.dsblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize ds blocks.");
      return false;
    }
  }

  if (result.txblocks().size() > 0) {
    if (!RepeatableToArray(result.txblocks(), tmp, tmp.size())) {
      LOG_GENERAL(WARNING, "Failed to serialize tx blocks.");
      return false;
    }
  }

  if (result.txblocks().size() > 0 || result.txblocks().size() > 0) {
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign tx blocks.");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "VCNodeSetDSTxBlockFromSeed initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetVCNodeSetDSTxBlockFromSeed(const bytes& src,
                                              const unsigned int offset,
                                              vector<DSBlock>& dsBlocks,
                                              vector<TxBlock>& txBlocks,
                                              PubKey& lookupPubKey) {
  LOG_MARKER();
  VCNodeSetDSTxBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "VCNodeSetDSTxBlockFromSeed initialization failed.");
    return false;
  }

  for (const auto& proto_dsblock : result.dsblocks()) {
    DSBlock dsblock;
    if (!ProtobufToDSBlock(proto_dsblock, dsblock)) {
      continue;
    }
    dsBlocks.emplace_back(dsblock);
  }

  for (const auto& txblock : result.txblocks()) {
    TxBlock block;
    if (!ProtobufToTxBlock(txblock, block)) {
      continue;
    }
    txBlocks.emplace_back(block);
  }

  ProtobufByteArrayToSerializable(result.pubkey(), lookupPubKey);

  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  bytes tmp;
  if (result.dsblocks().size() > 0) {
    if (!RepeatableToArray(result.dsblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize DS blocks.");
      return false;
    }
  }

  if (result.txblocks().size() > 0) {
    if (!RepeatableToArray(result.txblocks(), tmp, tmp.size())) {
      LOG_GENERAL(WARNING, "Failed to serialize tx blocks.");
      return false;
    }
  }

  if (result.txblocks().size() > 0 || result.txblocks().size() > 0) {
    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in tx blocks.");
      return false;
    }
  }

  return true;
}

bool Messenger::SetDSLookupNewDSGuardNetworkInfo(
    bytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
    const Peer& dsGuardNewNetworkInfo, const uint64_t timestamp,
    const pair<PrivKey, PubKey>& dsguardkey) {
  LOG_MARKER();
  DSLookupSetDSGuardNetworkInfoUpdate result;

  result.mutable_data()->set_dsepochnumber(dsEpochNumber);
  SerializableToProtobufByteArray(
      dsguardkey.second, *result.mutable_data()->mutable_dsguardpubkey());
  PeerToProtobuf(dsGuardNewNetworkInfo,
                 *result.mutable_data()->mutable_dsguardnewnetworkinfo());
  result.mutable_data()->set_timestamp(timestamp);

  if (result.data().IsInitialized()) {
    bytes tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!Schnorr::GetInstance().Sign(tmp, dsguardkey.first, dsguardkey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign ds guard identity update.");
      return false;
    }
    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(
        WARNING,
        "DSLookupSetDSGuardNetworkInfoUpdate.Data initialization failed.");
    return false;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "DSLookupSetDSGuardNetworkInfoUpdate initialization failed.");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSLookupNewDSGuardNetworkInfo(
    const bytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
    Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey) {
  LOG_MARKER();

  DSLookupSetDSGuardNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "DSLookupSetDSGuardNetworkInfoUpdate initialization failed.");
    return false;
  }

  // First deserialize the fields needed just for signature check
  ProtobufByteArrayToSerializable(result.data().dsguardpubkey(), dsGuardPubkey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);

  // Check signature
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     dsGuardPubkey)) {
    LOG_GENERAL(WARNING,
                "DSLookupSetDSGuardNetworkInfoUpdate signature wrong.");
    return false;
  }

  // Deserialize the remaining fields
  dsEpochNumber = result.data().dsepochnumber();
  ProtobufToPeer(result.data().dsguardnewnetworkinfo(), dsGuardNewNetworkInfo);
  timestamp = result.data().timestamp();

  return true;
}

bool Messenger::SetLookupGetNewDSGuardNetworkInfoFromLookup(
    bytes& dst, const unsigned int offset, const uint32_t portNo,
    const uint64_t dsEpochNumber) {
  LOG_MARKER();

  NodeGetGuardNodeNetworkInfoUpdate result;
  result.set_portno(portNo);
  result.set_dsepochnumber(dsEpochNumber);

  if (!result.IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "SetLookupGetNewDSGuardNetworkInfoFromLookup initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetNewDSGuardNetworkInfoFromLookup(
    const bytes& src, const unsigned int offset, uint32_t& portNo,
    uint64_t& dsEpochNumber) {
  LOG_MARKER();

  NodeGetGuardNodeNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "GetLookupGetNewDSGuardNetworkInfoFromLookup initialization failed.");
    return false;
  }

  portNo = result.portno();
  dsEpochNumber = result.dsepochnumber();

  return true;
}

bool Messenger::SetNodeSetNewDSGuardNetworkInfo(
    bytes& dst, unsigned int offset,
    const vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
    const std::pair<PrivKey, PubKey>& lookupKey) {
  LOG_MARKER();
  NodeSetGuardNodeNetworkInfoUpdate result;

  for (const auto& dsguardupdate : vecOfDSGuardUpdateStruct) {
    ProtoDSGuardUpdateStruct* proto_DSGuardUpdateStruct =
        result.mutable_data()->add_dsguardupdatestruct();
    SerializableToProtobufByteArray(
        dsguardupdate.m_dsGuardPubkey,
        *proto_DSGuardUpdateStruct->mutable_dsguardpubkey());
    PeerToProtobuf(dsguardupdate.m_dsGuardNewNetworkInfo,
                   *proto_DSGuardUpdateStruct->mutable_dsguardnewnetworkinfo());
    proto_DSGuardUpdateStruct->set_timestamp(dsguardupdate.m_timestamp);
  }

  if (result.data().IsInitialized()) {
    bytes tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign ds guard identity update.");
      return false;
    }
    SerializableToProtobufByteArray(lookupKey.second,
                                    *result.mutable_lookuppubkey());
    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(
        WARNING,
        "NodeSetGuardNodeNetworkInfoUpdate.Data initialization failed.");
    return false;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "SetNodeSetNewDSGuardNetworkInfo initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetNodeGetNewDSGuardNetworkInfo(
    const bytes& src, const unsigned int offset,
    vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
    PubKey& lookupPubKey) {
  LOG_MARKER();
  NodeSetGuardNodeNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetGuardNodeNetworkInfoUpdate initialization failed.");
    return false;
  }

  ProtobufByteArrayToSerializable(result.lookuppubkey(), lookupPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     lookupPubKey)) {
    LOG_GENERAL(WARNING, "NodeSetGuardNodeNetworkInfoUpdate signature wrong.");
    return false;
  }

  for (const auto& proto_DSGuardUpdateStruct :
       result.data().dsguardupdatestruct()) {
    PubKey tempPubk;
    ProtobufByteArrayToSerializable(proto_DSGuardUpdateStruct.dsguardpubkey(),
                                    tempPubk);
    Peer tempPeer;
    ProtobufToPeer(proto_DSGuardUpdateStruct.dsguardnewnetworkinfo(), tempPeer);
    uint64_t tempTimestamp = proto_DSGuardUpdateStruct.timestamp();
    vecOfDSGuardUpdateStruct.emplace_back(
        DSGuardUpdateStruct(tempPubk, tempPeer, tempTimestamp));
  }

  return true;
}

bool Messenger::SetSeedNodeHistoricalDB(
    bytes& dst, const unsigned int offset,
    const pair<PrivKey, PubKey>& archivalKeys, const uint32_t code,
    const string& path) {
  SeedSetHistoricalDB result;

  result.mutable_data()->set_code(code);
  result.mutable_data()->set_path(path);
  SerializableToProtobufByteArray(archivalKeys.second,
                                  *result.mutable_pubkey());

  if (result.data().IsInitialized()) {
    vector<unsigned char> tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());
    Signature signature;
    if (!Schnorr::GetInstance().Sign(tmp, archivalKeys.first,
                                     archivalKeys.second, signature)) {
      LOG_GENERAL(WARNING, "Failed to sign SeedSetHistoricalDB");
      return false;
    }
    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB.Data initialization failed");
    return false;
  }
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB initialization failed.");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetSeedNodeHistoricalDB(const bytes& src,
                                        const unsigned int offset,
                                        PubKey& archivalPubKey, uint32_t& code,
                                        string& path) {
  SeedSetHistoricalDB result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB initialization failed ");
    return false;
  }

  ProtobufByteArrayToSerializable(result.pubkey(), archivalPubKey);
  Signature signature;
  ProtobufByteArrayToSerializable(result.signature(), signature);
  vector<unsigned char> tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     archivalPubKey)) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB signature wrong.");
    return false;
  }
  code = result.data().code();
  path = result.data().path();

  return true;
}
