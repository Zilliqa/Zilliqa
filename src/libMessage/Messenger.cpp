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

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
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

bool ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                     Serializable& serializable) {
  bytes tmp;
  copy(byteArray.data().begin(), byteArray.data().end(), back_inserter(tmp));
  return serializable.Deserialize(tmp, 0) == 0;
}

// Temporary function for use by data blocks
void SerializableToProtobufByteArray(const SerializableDataBlock& serializable,
                                     ByteArray& byteArray) {
  bytes tmp;
  serializable.Serialize(tmp, 0);
  byteArray.set_data(tmp.data(), tmp.size());
}

// Temporary function for use by data blocks
bool ProtobufByteArrayToSerializable(const ByteArray& byteArray,
                                     SerializableDataBlock& serializable) {
  bytes tmp;
  copy(byteArray.data().begin(), byteArray.data().end(), back_inserter(tmp));
  return serializable.Deserialize(tmp, 0);
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
  return protoBlockLink.has_version() && protoBlockLink.has_index() &&
         protoBlockLink.has_dsindex() && protoBlockLink.has_blocktype() &&
         protoBlockLink.has_blockhash();
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
  return protoDSBlockHeader.has_leaderpubkey() &&
         protoDSBlockHeader.has_blocknum() &&
         protoDSBlockHeader.has_epochnum() && protoDSBlockHeader.has_swinfo() &&
         protoDSBlockHeader.has_hash() &&
         protoDSBlockHeader.has_blockheaderbase() &&
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
  return protoDSCommittee.has_version();
}

inline bool CheckRequiredFieldsProtoMicroBlockMicroBlockHeader(
    const ProtoMicroBlock::MicroBlockHeader& protoMicroBlockHeader) {
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
  return protoShardingStructure.has_version();
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
  return protoTxBlockHeader.has_gaslimit() &&
         protoTxBlockHeader.has_gasused() && protoTxBlockHeader.has_rewards() &&
         protoTxBlockHeader.has_blocknum() && protoTxBlockHeader.has_hash() &&
         protoTxBlockHeader.has_numtxs() &&
         protoTxBlockHeader.has_minerpubkey() &&
         protoTxBlockHeader.has_dsblocknum() &&
         protoTxBlockHeader.has_blockheaderbase() &&
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
         protoVCBlockHeader.has_blockheaderbase();
}

inline bool CheckRequiredFieldsProtoVCBlock(const ProtoVCBlock& protoVCBlock) {
  return protoVCBlock.has_header() && protoVCBlock.has_blockbase();
}

inline bool CheckRequiredFieldsProtoFallbackBlockFallbackBlockHeader(
    const ProtoFallbackBlock::FallbackBlockHeader& protoFallbackBlockHeader) {
  // Don't need to enforce check on repeated member faultyleaders
  return protoFallbackBlockHeader.has_fallbackdsepochno() &&
         protoFallbackBlockHeader.has_fallbackepochno() &&
         protoFallbackBlockHeader.has_fallbackstate() &&
         protoFallbackBlockHeader.has_stateroothash() &&
         protoFallbackBlockHeader.has_leaderconsensusid() &&
         protoFallbackBlockHeader.has_leadernetworkinfo() &&
         protoFallbackBlockHeader.has_leaderpubkey() &&
         protoFallbackBlockHeader.has_blockheaderbase() &&
         protoFallbackBlockHeader.has_shardid();
}

inline bool CheckRequiredFieldsProtoFallbackBlock(
    const ProtoFallbackBlock& protoFallbackBlock) {
  // Don't need to enforce check on repeated member mbinfos
  return protoFallbackBlock.has_header() && protoFallbackBlock.has_blockbase();
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

inline bool CheckRequiredFieldsProtoBlockHeaderBase(
    const ProtoBlockHeaderBase& protoBlockHeaderBase) {
  return protoBlockHeaderBase.has_version() &&
         protoBlockHeaderBase.has_committeehash() &&
         protoBlockHeaderBase.has_prevhash();
}

inline bool CheckRequiredFieldsProtoAccountBase(
    const ProtoAccountBase& protoAccountBase) {
  return protoAccountBase.has_version() && protoAccountBase.has_balance() &&
         protoAccountBase.has_nonce();
}

inline bool CheckRequiredFieldsProtoAccount(const ProtoAccount& protoAccount) {
  return protoAccount.has_base();
}

inline bool CheckRequiredFieldsProtoStateData(
    const ProtoStateData& protoStateData) {
  return protoStateData.has_version() && protoStateData.has_vname() &&
         protoStateData.has_ismutable() && protoStateData.has_type() &&
         protoStateData.has_value();
}

inline bool CheckRequiredFieldsProtoTransaction(
    const ProtoTransaction& protoTransaction) {
  return protoTransaction.has_tranid() && protoTransaction.has_info() &&
         protoTransaction.has_signature();
}

inline bool CheckRequiredFieldsProtoTransactionCoreInfo(
    const ProtoTransactionCoreInfo& protoTxnCoreInfo) {
  return protoTxnCoreInfo.has_version() && protoTxnCoreInfo.has_nonce() &&
         protoTxnCoreInfo.has_toaddr() && protoTxnCoreInfo.has_senderpubkey() &&
         protoTxnCoreInfo.has_amount() && protoTxnCoreInfo.has_gasprice() &&
         protoTxnCoreInfo.has_gaslimit();
}

inline bool CheckRequiredFieldsProtoTransactionReceipt(
    const ProtoTransactionReceipt& protoTransactionReceipt) {
  return protoTransactionReceipt.has_receipt() &&
         protoTransactionReceipt.has_cumgas();
}

inline bool CheckRequiredFieldsProtoTransactionWithReceipt(
    const ProtoTransactionWithReceipt& protoTxnWReceipt) {
  return protoTxnWReceipt.has_transaction() && protoTxnWReceipt.has_receipt();
}

// ============================================================================
// Protobuf <-> Primitives conversion functions
// ============================================================================

void AccountBaseToProtobuf(const AccountBase& accountbase,
                           ProtoAccountBase& protoAccountBase) {
  protoAccountBase.set_version(accountbase.GetVersion());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      accountbase.GetBalance(), *protoAccountBase.mutable_balance());
  protoAccountBase.set_nonce(accountbase.GetNonce());
  if (accountbase.GetCodeHash() != dev::h256()) {
    protoAccountBase.set_codehash(accountbase.GetCodeHash().data(),
                                  accountbase.GetCodeHash().size);
  }
  if (accountbase.GetStorageRoot() != dev::h256()) {
    protoAccountBase.set_storageroot(accountbase.GetStorageRoot().data(),
                                     accountbase.GetStorageRoot().size);
  }
}

bool ProtobufToAccountBase(const ProtoAccountBase& protoAccountBase,
                           AccountBase& accountBase) {
  if (!CheckRequiredFieldsProtoAccountBase(protoAccountBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoAccountBase failed");
    return false;
  }

  accountBase.SetVersion(protoAccountBase.version());

  uint128_t tmpNumber;
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(protoAccountBase.balance(),
                                                     tmpNumber);
  accountBase.SetBalance(tmpNumber);
  accountBase.SetNonce(protoAccountBase.nonce());

  if (protoAccountBase.has_codehash()) {
    dev::h256 tmpCodeHash;
    if (!Messenger::CopyWithSizeCheck(protoAccountBase.codehash(),
                                      tmpCodeHash.asArray())) {
      return false;
    }
    accountBase.SetCodeHash(tmpCodeHash);
  }

  if (protoAccountBase.has_storageroot()) {
    dev::h256 tmpStorageRoot;
    if (!Messenger::CopyWithSizeCheck(protoAccountBase.storageroot(),
                                      tmpStorageRoot.asArray())) {
      return false;
    }
    accountBase.SetStorageRoot(tmpStorageRoot);
  }

  return true;
}

void AccountToProtobuf(const Account& account, ProtoAccount& protoAccount) {
  ZilliqaMessage::ProtoAccountBase* protoAccountBase =
      protoAccount.mutable_base();

  AccountBaseToProtobuf(account, *protoAccountBase);

  if (protoAccountBase->has_codehash()) {
    bytes codebytes = account.GetCode();
    protoAccount.set_code(codebytes.data(), codebytes.size());
    for (const auto& keyHash : account.GetStorageKeyHashes(false)) {
      ProtoAccount::StorageData* entry = protoAccount.add_storage();
      entry->set_keyhash(keyHash.data(), keyHash.size);
      entry->set_data(account.GetRawStorage(keyHash, false));
    }
  }
}

bool ProtobufToAccount(const ProtoAccount& protoAccount, Account& account,
                       const Address& addr) {
  if (!CheckRequiredFieldsProtoAccount(protoAccount)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoAccount failed");
    return false;
  }

  const ZilliqaMessage::ProtoAccountBase& protoAccountBase =
      protoAccount.base();

  if (!ProtobufToAccountBase(protoAccountBase, account)) {
    LOG_GENERAL(WARNING, "ProtobufToAccountBase failed");
    return false;
  }

  if (account.GetCodeHash() != dev::h256()) {
    dev::h256 tmpCodeHash = account.GetCodeHash();

    if (!protoAccount.has_code()) {
      LOG_GENERAL(WARNING, "Account has valid codehash but no code content");
      return false;
    }
    bytes tmpVec;
    tmpVec.resize(protoAccount.code().size());
    copy(protoAccount.code().begin(), protoAccount.code().end(),
         tmpVec.begin());
    account.SetCode(tmpVec);

    if (account.GetCodeHash() != tmpCodeHash) {
      LOG_GENERAL(WARNING, "Code hash mismatch. Expected: "
                               << account.GetCodeHash().hex()
                               << " Actual: " << tmpCodeHash.hex());
      return false;
    }

    dev::h256 tmpHash, tmpStorageRoot = account.GetStorageRoot();

    vector<pair<dev::h256, bytes>> entries;
    for (const auto& entry : protoAccount.storage()) {
      if (!Messenger::CopyWithSizeCheck(entry.keyhash(), tmpHash.asArray())) {
        return false;
      }

      entries.emplace_back(tmpHash,
                           DataConversion::StringToCharArray(entry.data()));
    }

    if (!account.SetStorage(addr, entries, false)) {
      return false;
    }

    if (account.GetStorageRoot() != tmpStorageRoot) {
      LOG_GENERAL(WARNING, "Storage root mismatch. Expected: "
                               << account.GetStorageRoot().hex()
                               << " Actual: " << tmpStorageRoot.hex());
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

  AccountBase accbase;

  accbase.SetVersion(newAccount.GetVersion());

  int256_t balanceDelta =
      int256_t(newAccount.GetBalance()) - int256_t(oldAccount->GetBalance());
  protoAccount.set_numbersign(balanceDelta > 0);
  accbase.SetBalance(uint128_t(abs(balanceDelta)));

  uint64_t nonceDelta = 0;
  if (!SafeMath<uint64_t>::sub(newAccount.GetNonce(), oldAccount->GetNonce(),
                               nonceDelta)) {
    return;
  }
  accbase.SetNonce(nonceDelta);

  if (newAccount.isContract()) {
    if (fullCopy) {
      accbase.SetCodeHash(newAccount.GetCodeHash());
      protoAccount.set_code(newAccount.GetCode().data(),
                            newAccount.GetCode().size());
    }

    if (fullCopy ||
        newAccount.GetStorageRoot() != oldAccount->GetStorageRoot()) {
      accbase.SetStorageRoot(newAccount.GetStorageRoot());

      for (const auto& keyHash : newAccount.GetStorageKeyHashes(true)) {
        string rlpStr = newAccount.GetRawStorage(keyHash, true);
        if (fullCopy || rlpStr != oldAccount->GetRawStorage(keyHash, false)) {
          ProtoAccount::StorageData* entry = protoAccount.add_storage();
          entry->set_keyhash(keyHash.data(), keyHash.size);
          entry->set_data(rlpStr);
        }
      }
    }
  }

  ZilliqaMessage::ProtoAccountBase* protoAccountBase =
      protoAccount.mutable_base();

  AccountBaseToProtobuf(accbase, *protoAccountBase);
}

bool ProtobufToAccountDelta(const ProtoAccount& protoAccount, Account& account,
                            const Address& addr, const bool fullCopy, bool temp,
                            bool revertible = false) {
  if (!CheckRequiredFieldsProtoAccount(protoAccount)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoAccount failed");
    return false;
  }

  AccountBase accbase;

  const ZilliqaMessage::ProtoAccountBase& protoAccountBase =
      protoAccount.base();

  if (!ProtobufToAccountBase(protoAccountBase, accbase)) {
    LOG_GENERAL(WARNING, "ProtobufToAccountBase failed");
    return false;
  }

  if (accbase.GetVersion() != ACCOUNT_VERSION) {
    LOG_GENERAL(WARNING, "Account delta version doesn't match, expected "
                             << ACCOUNT_VERSION << " received "
                             << accbase.GetVersion());
    return false;
  }

  if (!protoAccount.has_numbersign()) {
    LOG_GENERAL(WARNING, "numbersign is not found in ProtoAccount for Delta");
    return false;
  }

  int256_t balanceDelta = protoAccount.numbersign()
                              ? accbase.GetBalance().convert_to<int256_t>()
                              : 0 - accbase.GetBalance().convert_to<int256_t>();
  account.ChangeBalance(balanceDelta);

  if (!account.IncreaseNonceBy(accbase.GetNonce())) {
    LOG_GENERAL(WARNING, "IncreaseNonceBy failed");
    return false;
  }

  if ((protoAccount.has_code() && protoAccount.code().size() > 0) ||
      account.isContract()) {
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

      if (account.GetCodeHash() != accbase.GetCodeHash()) {
        LOG_GENERAL(WARNING, "Code hash mismatch. Expected: "
                                 << account.GetCodeHash().hex()
                                 << " Actual: " << accbase.GetCodeHash().hex());
        return false;
      }
    }

    if (accbase.GetStorageRoot() != account.GetStorageRoot()) {
      dev::h256 tmpHash;

      vector<pair<dev::h256, bytes>> entries;
      for (const auto& entry : protoAccount.storage()) {
        if (!Messenger::CopyWithSizeCheck(entry.keyhash(), tmpHash.asArray())) {
          return false;
        }

        entries.emplace_back(tmpHash,
                             DataConversion::StringToCharArray(entry.data()));
      }

      if (!account.SetStorage(addr, entries, temp, revertible)) {
        return false;
      }

      if (!entries.empty() &&
          accbase.GetStorageRoot() != account.GetStorageRoot()) {
        LOG_GENERAL(WARNING,
                    "Storage root mismatch. Expected: "
                        << account.GetStorageRoot().hex()
                        << " Actual: " << accbase.GetStorageRoot().hex());
        return false;
      }
    }
  }

  return true;
}

void DSCommitteeToProtobuf(const uint32_t version,
                           const DequeOfNode& dsCommittee,
                           ProtoDSCommittee& protoDSCommittee) {
  protoDSCommittee.set_version(version);
  for (const auto& node : dsCommittee) {
    ProtoDSNode* protodsnode = protoDSCommittee.add_dsnodes();
    SerializableToProtobufByteArray(node.first, *protodsnode->mutable_pubkey());
    SerializableToProtobufByteArray(node.second, *protodsnode->mutable_peer());
  }
}

bool ProtobufToDSCommittee(const ProtoDSCommittee& protoDSCommittee,
                           uint32_t& version, DequeOfNode& dsCommittee) {
  if (!CheckRequiredFieldsProtoDSCommittee(protoDSCommittee)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSCommittee failed");
    return false;
  }

  version = protoDSCommittee.version();

  for (const auto& dsnode : protoDSCommittee.dsnodes()) {
    if (!CheckRequiredFieldsProtoDSNode(dsnode)) {
      LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSNode failed");
      return false;
    }

    PubKey pubkey;
    Peer peer;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(dsnode.pubkey(), pubkey);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(dsnode.peer(), peer);
    dsCommittee.emplace_back(pubkey, peer);
  }

  return true;
}

void FaultyLeaderToProtobuf(const VectorOfNode& faultyLeaders,
                            ProtoVCBlock::VCBlockHeader& protoVCBlockHeader) {
  for (const auto& node : faultyLeaders) {
    ProtoDSNode* protodsnode = protoVCBlockHeader.add_faultyleaders();
    SerializableToProtobufByteArray(node.first, *protodsnode->mutable_pubkey());
    SerializableToProtobufByteArray(node.second, *protodsnode->mutable_peer());
  }
}

bool ProtobufToFaultyDSMembers(
    const ProtoVCBlock::VCBlockHeader& protoVCBlockHeader,
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

void DSCommitteeToProtoCommittee(const DequeOfNode& dsCommittee,
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

void StateIndexToProtobuf(const vector<Contract::Index>& indexes,
                          ProtoStateIndex& protoStateIndex) {
  for (const auto& index : indexes) {
    protoStateIndex.add_index(index.data(), index.size);
  }
}

bool ProtobufToStateIndex(const ProtoStateIndex& protoStateIndex,
                          vector<Contract::Index>& indexes) {
  for (const auto& index : protoStateIndex.index()) {
    indexes.emplace_back();
    unsigned int size =
        min((unsigned int)index.size(), (unsigned int)indexes.back().size);
    copy(index.begin(), index.begin() + size, indexes.back().asArray().begin());
  }

  return true;
}

void StateDataToProtobuf(const Contract::StateEntry& entry,
                         ProtoStateData& protoStateData) {
  protoStateData.set_version(CONTRACT_STATE_VERSION);
  protoStateData.set_vname(std::get<Contract::VNAME>(entry));
  protoStateData.set_ismutable(std::get<Contract::MUTABLE>(entry));
  protoStateData.set_type(std::get<Contract::TYPE>(entry));

  string value = std::get<Contract::VALUE>(entry);
  if (value.front() == '"') {
    value.erase(0, 1);
  }
  if (value.back() == '"') {
    value.erase(value.size() - 1);
  }

  protoStateData.set_value(value);
}

bool ProtobufToStateData(const ProtoStateData& protoStateData,
                         Contract::StateEntry& indexes, uint32_t& version) {
  if (!CheckRequiredFieldsProtoStateData(protoStateData)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoStateData failed");
    return false;
  }

  version = protoStateData.version();

  indexes = std::make_tuple(protoStateData.vname(), protoStateData.ismutable(),
                            protoStateData.type(), protoStateData.value());
  return true;
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
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockBase failed");
    return false;
  }

  // Deserialize cosigs
  CoSignatures cosigs;
  cosigs.m_B1.resize(protoBlockBase.cosigs().b1().size());
  cosigs.m_B2.resize(protoBlockBase.cosigs().b2().size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoBlockBase.cosigs().cs1(), cosigs.m_CS1);
  copy(protoBlockBase.cosigs().b1().begin(), protoBlockBase.cosigs().b1().end(),
       cosigs.m_B1.begin());
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoBlockBase.cosigs().cs2(), cosigs.m_CS2);
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

void BlockHeaderBaseToProtobuf(const BlockHeaderBase& base,
                               ProtoBlockHeaderBase& protoBlockHeaderBase) {
  // version
  protoBlockHeaderBase.set_version(base.GetVersion());
  // committee hash
  protoBlockHeaderBase.set_committeehash(base.GetCommitteeHash().data(),
                                         base.GetCommitteeHash().size);
  protoBlockHeaderBase.set_prevhash(base.GetPrevHash().data(),
                                    base.GetPrevHash().size);
}

bool ProtobufToBlockHeaderBase(const ProtoBlockHeaderBase& protoBlockHeaderBase,
                               BlockHeaderBase& base) {
  if (!CheckRequiredFieldsProtoBlockHeaderBase(protoBlockHeaderBase)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockHeaderBase failed");
    return false;
  }

  // Deserialize the version
  uint32_t version;
  version = protoBlockHeaderBase.version();

  base.SetVersion(version);

  // Deserialize committee hash
  CommitteeHash committeeHash;
  if (!Messenger::CopyWithSizeCheck(protoBlockHeaderBase.committeehash(),
                                    committeeHash.asArray())) {
    return false;
  }
  base.SetCommitteeHash(committeeHash);

  // Deserialize prev hash
  BlockHash prevHash;
  if (!Messenger::CopyWithSizeCheck(protoBlockHeaderBase.prevhash(),
                                    prevHash.asArray())) {
    return false;
  }
  base.SetPrevHash(prevHash);

  return true;
}

void ShardingStructureToProtobuf(
    const uint32_t& version, const DequeOfShard& shards,
    ProtoShardingStructure& protoShardingStructure) {
  protoShardingStructure.set_version(version);
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
    const ProtoShardingStructure& protoShardingStructure, uint32_t& version,
    DequeOfShard& shards) {
  if (!CheckRequiredFieldsProtoShardingStructure(protoShardingStructure)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoShardingStructure failed");
    return false;
  }

  version = protoShardingStructure.version();

  for (const auto& proto_shard : protoShardingStructure.shards()) {
    if (!CheckRequiredFieldsProtoShardingStructureShard(proto_shard)) {
      LOG_GENERAL(WARNING,
                  "CheckRequiredFieldsProtoShardingStructureShard failed");
      return false;
    }

    shards.emplace_back();

    for (const auto& proto_member : proto_shard.members()) {
      if (!CheckRequiredFieldsProtoShardingStructureMember(proto_member)) {
        LOG_GENERAL(WARNING,
                    "CheckRequiredFieldsProtoShardingStructureMember failed");
        return false;
      }

      PubKey key;
      Peer peer;

      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.pubkey(), key);
      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.peerinfo(), peer);

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

bool ProtobufToShardingStructureAnnouncement(
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

      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.pubkey(), key);
      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.peerinfo(), peer);

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

  return true;
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
  if (!txnCoreInfo.code.empty()) {
    protoTxnCoreInfo.set_code(txnCoreInfo.code.data(), txnCoreInfo.code.size());
  }
  if (!txnCoreInfo.data.empty()) {
    protoTxnCoreInfo.set_data(txnCoreInfo.data.data(), txnCoreInfo.data.size());
  }
}

bool ProtobufToTransactionCoreInfo(
    const ProtoTransactionCoreInfo& protoTxnCoreInfo,
    TransactionCoreInfo& txnCoreInfo) {
  if (!CheckRequiredFieldsProtoTransactionCoreInfo(protoTxnCoreInfo)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTransactionCoreInfo failed");
    return false;
  }
  txnCoreInfo.version = protoTxnCoreInfo.version();
  txnCoreInfo.nonce = protoTxnCoreInfo.nonce();
  copy(protoTxnCoreInfo.toaddr().begin(),
       protoTxnCoreInfo.toaddr().begin() +
           min((unsigned int)protoTxnCoreInfo.toaddr().size(),
               (unsigned int)txnCoreInfo.toAddr.size),
       txnCoreInfo.toAddr.asArray().begin());
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoTxnCoreInfo.senderpubkey(),
                                  txnCoreInfo.senderPubKey);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(protoTxnCoreInfo.amount(),
                                                     txnCoreInfo.amount);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxnCoreInfo.gasprice(), txnCoreInfo.gasPrice);
  txnCoreInfo.gasLimit = protoTxnCoreInfo.gaslimit();
  if (protoTxnCoreInfo.has_code() && protoTxnCoreInfo.code().size() > 0) {
    txnCoreInfo.code.resize(protoTxnCoreInfo.code().size());
    copy(protoTxnCoreInfo.code().begin(), protoTxnCoreInfo.code().end(),
         txnCoreInfo.code.begin());
  }
  if (protoTxnCoreInfo.has_data() && protoTxnCoreInfo.data().size() > 0) {
    txnCoreInfo.data.resize(protoTxnCoreInfo.data().size());
    copy(protoTxnCoreInfo.data().begin(), protoTxnCoreInfo.data().end(),
         txnCoreInfo.data.begin());
  }
  return true;
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

bool ProtobufToTransaction(const ProtoTransaction& protoTransaction,
                           Transaction& transaction) {
  if (!CheckRequiredFieldsProtoTransaction(protoTransaction)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTransaction failed");
    return false;
  }

  TxnHash tranID;
  TransactionCoreInfo txnCoreInfo;
  Signature signature;

  copy(protoTransaction.tranid().begin(),
       protoTransaction.tranid().begin() +
           min((unsigned int)protoTransaction.tranid().size(),
               (unsigned int)tranID.size),
       tranID.asArray().begin());

  if (!ProtobufToTransactionCoreInfo(protoTransaction.info(), txnCoreInfo)) {
    LOG_GENERAL(WARNING, "ProtobufToTransactionCoreInfo failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoTransaction.signature(), signature);

  bytes txnData;
  if (!SerializeToArray(protoTransaction.info(), txnData, 0)) {
    LOG_GENERAL(WARNING, "Serialize protoTransaction core info failed");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(txnData);
  const bytes& hash = sha2.Finalize();

  if (!std::equal(hash.begin(), hash.end(), tranID.begin(), tranID.end())) {
    TxnHash expected;
    copy(hash.begin(), hash.end(), expected.asArray().begin());
    LOG_GENERAL(WARNING, "TranID verification failed. Expected: "
                             << expected << " Actual: " << tranID);
    return false;
  }

  // Verify signature
  if (!Schnorr::GetInstance().Verify(txnData, signature,
                                     txnCoreInfo.senderPubKey)) {
    LOG_GENERAL(WARNING, "Signature verification failed");
    return false;
  }

  transaction = Transaction(
      tranID, txnCoreInfo.version, txnCoreInfo.nonce, txnCoreInfo.toAddr,
      txnCoreInfo.senderPubKey, txnCoreInfo.amount, txnCoreInfo.gasPrice,
      txnCoreInfo.gasLimit, txnCoreInfo.code, txnCoreInfo.data, signature);

  return true;
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

bool ProtobufToTransactionArray(
    const ProtoTransactionArray& protoTransactionArray,
    std::vector<Transaction>& txns) {
  for (const auto& protoTransaction : protoTransactionArray.transactions()) {
    Transaction txn;
    if (!ProtobufToTransaction(protoTransaction, txn)) {
      LOG_GENERAL(WARNING, "ProtobufToTransaction failed");
      return false;
    }
    txns.push_back(txn);
  }

  return true;
}

void TransactionReceiptToProtobuf(const TransactionReceipt& transReceipt,
                                  ProtoTransactionReceipt& protoTransReceipt) {
  protoTransReceipt.set_receipt(transReceipt.GetString());
  // protoTransReceipt.set_cumgas(transReceipt.GetCumGas());
  protoTransReceipt.set_cumgas(transReceipt.GetCumGas());
}

bool ProtobufToTransactionReceipt(
    const ProtoTransactionReceipt& protoTransactionReceipt,
    TransactionReceipt& transactionReceipt) {
  if (!CheckRequiredFieldsProtoTransactionReceipt(protoTransactionReceipt)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoTransactionReceipt failed");
    return false;
  }
  std::string tranReceiptStr;
  tranReceiptStr.resize(protoTransactionReceipt.receipt().size());
  copy(protoTransactionReceipt.receipt().begin(),
       protoTransactionReceipt.receipt().end(), tranReceiptStr.begin());
  transactionReceipt.SetString(tranReceiptStr);
  transactionReceipt.SetCumGas(protoTransactionReceipt.cumgas());

  return true;
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

bool ProtobufToTransactionWithReceipt(
    const ProtoTransactionWithReceipt& protoWithTransaction,
    TransactionWithReceipt& transactionWithReceipt) {
  Transaction transaction;
  if (!ProtobufToTransaction(protoWithTransaction.transaction(), transaction)) {
    LOG_GENERAL(WARNING, "ProtobufToTransaction failed");
    return false;
  }

  TransactionReceipt receipt;
  if (!ProtobufToTransactionReceipt(protoWithTransaction.receipt(), receipt)) {
    LOG_GENERAL(WARNING, "ProtobufToTransactionReceipt failed");
    return false;
  }

  transactionWithReceipt = TransactionWithReceipt(transaction, receipt);

  return true;
}

void PeerToProtobuf(const Peer& peer, ProtoPeer& protoPeer) {
  NumberToProtobufByteArray<uint128_t, sizeof(uint128_t)>(
      peer.GetIpAddress(), *protoPeer.mutable_ipaddress());

  protoPeer.set_listenporthost(peer.GetListenPortHost());
}

void ProtobufToPeer(const ProtoPeer& protoPeer, Peer& peer) {
  uint128_t ipAddress;
  ProtobufByteArrayToNumber<uint128_t, sizeof(uint128_t)>(protoPeer.ipaddress(),
                                                          ipAddress);

  peer = Peer(ipAddress, protoPeer.listenporthost());
}

void DSBlockHeaderToProtobuf(const DSBlockHeader& dsBlockHeader,
                             ProtoDSBlock::DSBlockHeader& protoDSBlockHeader,
                             bool concreteVarsOnly = false) {
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoDSBlockHeader.mutable_blockheaderbase();
  BlockHeaderBaseToProtobuf(dsBlockHeader, *protoBlockHeaderBase);

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

  dsBlockHeader = DSBlockHeader(
      dsdifficulty, difficulty, leaderPubKey, protoDSBlockHeader.blocknum(),
      protoDSBlockHeader.epochnum(), gasprice, swInfo, powDSWinners, hash);

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoDSBlockHeader.blockheaderbase();

  return ProtobufToBlockHeaderBase(protoBlockHeaderBase, dsBlockHeader);
}

bool ProtobufToDSBlock(const ProtoDSBlock& protoDSBlock, DSBlock& dsBlock) {
  // Deserialize header

  if (!CheckRequiredFieldsProtoDSBlock(protoDSBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoDSBlock failed");
    return false;
  }

  const ZilliqaMessage::ProtoDSBlock::DSBlockHeader& protoHeader =
      protoDSBlock.header();

  DSBlockHeader header;

  if (!ProtobufToDSBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToDSBlockHeader failed");
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
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoMicroBlockHeader.mutable_blockheaderbase();
  BlockHeaderBaseToProtobuf(microBlockHeader, *protoBlockHeaderBase);

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

bool ProtobufToDSPowSolution(const DSPoWSubmission& dsPowSubmission,
                             DSPowSolution& powSolution) {
  const uint64_t& blockNumber = dsPowSubmission.data().blocknumber();
  const uint8_t& difficultyLevel = dsPowSubmission.data().difficultylevel();
  Peer submitterPeer;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(dsPowSubmission.data().submitterpeer(),
                                  submitterPeer);
  PubKey submitterKey;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(dsPowSubmission.data().submitterpubkey(),
                                  submitterKey);
  const uint64_t& nonce = dsPowSubmission.data().nonce();
  const std::string& resultingHash = dsPowSubmission.data().resultinghash();
  const std::string& mixHash = dsPowSubmission.data().mixhash();
  const uint32_t& lookupId = dsPowSubmission.data().lookupid();
  uint128_t gasPrice;
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      dsPowSubmission.data().gasprice(), gasPrice);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(dsPowSubmission.signature(), signature);

  DSPowSolution result(blockNumber, difficultyLevel, submitterPeer,
                       submitterKey, nonce, resultingHash, mixHash, lookupId,
                       gasPrice, signature);
  powSolution = result;

  return true;
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

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.txroothash(),
                                    txRootHash.asArray())) {
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoMicroBlockHeader.minerpubkey(),
                                  minerPubKey);

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.statedeltahash(),
                                    stateDeltaHash.asArray())) {
    return false;
  }

  if (!Messenger::CopyWithSizeCheck(protoMicroBlockHeader.tranreceipthash(),
                                    tranReceiptHash.asArray())) {
    return false;
  }

  microBlockHeader =
      MicroBlockHeader(protoMicroBlockHeader.shardid(), gasLimit, gasUsed,
                       rewards, protoMicroBlockHeader.epochnum(),
                       {txRootHash, stateDeltaHash, tranReceiptHash},
                       protoMicroBlockHeader.numtxs(), minerPubKey,
                       protoMicroBlockHeader.dsblocknum());

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoMicroBlockHeader.blockheaderbase();

  return ProtobufToBlockHeaderBase(protoBlockHeaderBase, microBlockHeader);
}

bool ProtobufToMicroBlock(const ProtoMicroBlock& protoMicroBlock,
                          MicroBlock& microBlock) {
  if (!CheckRequiredFieldsProtoMicroBlock(protoMicroBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMicroBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoMicroBlock::MicroBlockHeader& protoHeader =
      protoMicroBlock.header();

  MicroBlockHeader header;

  if (!ProtobufToMicroBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToMicroBlockHeader failed");
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
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoMbInfo failed");
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
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoTxBlockHeader.mutable_blockheaderbase();
  BlockHeaderBaseToProtobuf(txBlockHeader, *protoBlockHeaderBase);

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

  txBlockHeader =
      TxBlockHeader(gasLimit, gasUsed, rewards, protoTxBlockHeader.blocknum(),
                    hash, protoTxBlockHeader.numtxs(), minerPubKey,
                    protoTxBlockHeader.dsblocknum());

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoTxBlockHeader.blockheaderbase();

  return ProtobufToBlockHeaderBase(protoBlockHeaderBase, txBlockHeader);
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
    LOG_GENERAL(WARNING, "ProtobufToTxBlockHeader failed");
    return false;
  }

  // Deserialize body
  vector<MicroBlockInfo> mbInfos;

  for (const auto& protoMbInfo : protoTxBlock.mbinfos()) {
    MicroBlockInfo mbInfo;
    if (!ProtobufToMbInfo(protoMbInfo, mbInfo)) {
      return false;
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
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoVCBlockHeader.mutable_blockheaderbase();
  BlockHeaderBaseToProtobuf(vcBlockHeader, *protoBlockHeaderBase);

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
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlockVCBlockHeader failed");
    return false;
  }

  Peer candidateLeaderNetworkInfo;
  PubKey candidateLeaderPubKey;
  CommitteeHash committeeHash;
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

  vcBlockHeader = VCBlockHeader(
      protoVCBlockHeader.viewchangedsepochno(),
      protoVCBlockHeader.viewchangeepochno(),
      protoVCBlockHeader.viewchangestate(), candidateLeaderNetworkInfo,
      candidateLeaderPubKey, protoVCBlockHeader.vccounter(), faultyLeaders);

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoVCBlockHeader.blockheaderbase();

  return ProtobufToBlockHeaderBase(protoBlockHeaderBase, vcBlockHeader);
}

bool ProtobufToVCBlock(const ProtoVCBlock& protoVCBlock, VCBlock& vcBlock) {
  if (!CheckRequiredFieldsProtoVCBlock(protoVCBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoVCBlock failed");
    return false;
  }

  // Deserialize header

  const ZilliqaMessage::ProtoVCBlock::VCBlockHeader& protoHeader =
      protoVCBlock.header();

  VCBlockHeader header;

  if (!ProtobufToVCBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToVCBlockHeader failed");
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
  ZilliqaMessage::ProtoBlockHeaderBase* protoBlockHeaderBase =
      protoFallbackBlockHeader.mutable_blockheaderbase();
  BlockHeaderBaseToProtobuf(fallbackBlockHeader, *protoBlockHeaderBase);

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

bool ProtobufToFallbackBlockHeader(
    const ProtoFallbackBlock::FallbackBlockHeader& protoFallbackBlockHeader,
    FallbackBlockHeader& fallbackBlockHeader) {
  if (!CheckRequiredFieldsProtoFallbackBlockFallbackBlockHeader(
          protoFallbackBlockHeader)) {
    LOG_GENERAL(
        WARNING,
        "CheckRequiredFieldsProtoFallbackBlockFallbackBlockHeader failed");
    return false;
  }

  Peer leaderNetworkInfo;
  PubKey leaderPubKey;
  StateHash stateRootHash;
  CommitteeHash committeeHash;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoFallbackBlockHeader.leadernetworkinfo(),
                                  leaderNetworkInfo);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(protoFallbackBlockHeader.leaderpubkey(),
                                  leaderPubKey);

  copy(protoFallbackBlockHeader.stateroothash().begin(),
       protoFallbackBlockHeader.stateroothash().begin() +
           min((unsigned int)protoFallbackBlockHeader.stateroothash().size(),
               (unsigned int)stateRootHash.size),
       stateRootHash.asArray().begin());

  fallbackBlockHeader = FallbackBlockHeader(
      protoFallbackBlockHeader.fallbackdsepochno(),
      protoFallbackBlockHeader.fallbackepochno(),
      protoFallbackBlockHeader.fallbackstate(), {stateRootHash},
      protoFallbackBlockHeader.leaderconsensusid(), leaderNetworkInfo,
      leaderPubKey, protoFallbackBlockHeader.shardid());

  const ZilliqaMessage::ProtoBlockHeaderBase& protoBlockHeaderBase =
      protoFallbackBlockHeader.blockheaderbase();

  return ProtobufToBlockHeaderBase(protoBlockHeaderBase, fallbackBlockHeader);
}

bool ProtobufToFallbackBlock(const ProtoFallbackBlock& protoFallbackBlock,
                             FallbackBlock& fallbackBlock) {
  if (!CheckRequiredFieldsProtoFallbackBlock(protoFallbackBlock)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoFallbackBlock failed");
    return false;
  }

  // Deserialize header
  const ZilliqaMessage::ProtoFallbackBlock::FallbackBlockHeader& protoHeader =
      protoFallbackBlock.header();

  FallbackBlockHeader header;

  if (!ProtobufToFallbackBlockHeader(protoHeader, header)) {
    LOG_GENERAL(WARNING, "ProtobufToFallbackBlockHeader failed");
    return false;
  }

  fallbackBlock = FallbackBlock(header, CoSignatures());

  const ZilliqaMessage::ProtoBlockBase& protoBlockBase =
      protoFallbackBlock.blockbase();

  return ProtobufToBlockBase(protoBlockBase, fallbackBlock);
}

bool SetConsensusAnnouncementCore(
    ZilliqaMessage::ConsensusAnnouncement& announcement,
    const uint32_t consensusID, uint64_t blockNumber, const bytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey) {
  LOG_MARKER();

  // Set the consensus parameters

  announcement.mutable_consensusinfo()->set_consensusid(consensusID);
  announcement.mutable_consensusinfo()->set_blocknumber(blockNumber);
  announcement.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                      blockHash.size());
  announcement.mutable_consensusinfo()->set_leaderid(leaderID);

  if (!announcement.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ConsensusAnnouncement.ConsensusInfo initialization failed");
    return false;
  }

  bytes tmp(announcement.consensusinfo().ByteSize());
  announcement.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second,
                                  *announcement.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *announcement.mutable_signature());

  // Sign the announcement

  bytes inputToSigning;

  switch (announcement.announcement_case()) {
    case ConsensusAnnouncement::AnnouncementCase::kDsblock:
      if (!announcement.dsblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement dsblock content not initialized");
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
        LOG_GENERAL(WARNING, "Announcement microblock content not initialized");
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
        LOG_GENERAL(WARNING, "Announcement finalblock content not initialized");
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
        LOG_GENERAL(WARNING, "Announcement vcblock content not initialized");
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
                    "Announcement fallbackblock content not initialized");
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
      LOG_GENERAL(WARNING, "Announcement content not set");
      return false;
  }

  Signature finalsignature;
  if (!Schnorr::GetInstance().Sign(inputToSigning, leaderKey.first,
                                   leaderKey.second, finalsignature)) {
    LOG_GENERAL(WARNING, "Failed to sign announcement");
    return false;
  }

  SerializableToProtobufByteArray(finalsignature,
                                  *announcement.mutable_finalsignature());

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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
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
    LOG_GENERAL(WARNING, "Announcement content not set");
    return false;
  }

  Signature finalsignature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(announcement.finalsignature(),
                                  finalsignature);

  if (!Schnorr::GetInstance().Verify(tmp, finalsignature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in announcement. leaderID = "
                             << leaderID << " leaderKey = " << leaderKey);
    return false;
  }

  return true;
}

// ============================================================================
// Primitives
// ============================================================================

bool Messenger::GetDSCommitteeHash(const DequeOfNode& dsCommittee,
                                   CommitteeHash& dst) {
  ProtoCommittee protoCommittee;

  DSCommitteeToProtoCommittee(dsCommittee, protoCommittee);

  if (!protoCommittee.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoCommittee initialization failed");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed");
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
    LOG_GENERAL(WARNING, "ProtoCommittee initialization failed");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::GetShardingStructureHash(const uint32_t& version,
                                         const DequeOfShard& shards,
                                         ShardingHash& dst) {
  ProtoShardingStructure protoShardingStructure;

  ShardingStructureToProtobuf(version, shards, protoShardingStructure);

  if (!protoShardingStructure.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure initialization failed");
    return false;
  }

  bytes tmp;

  if (!SerializeToArray(protoShardingStructure, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure serialization failed");
    return false;
  }

  SHA2<HASH_TYPE::HASH_VARIANT_256> sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::SetAccountBase(bytes& dst, const unsigned int offset,
                               const AccountBase& accountbase) {
  ProtoAccountBase result;

  AccountBaseToProtobuf(accountbase, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountBase initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetAccountBase(const bytes& src, const unsigned int offset,
                               AccountBase& accountbase) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoAccountBase result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  if (!ProtobufToAccountBase(result, accountbase)) {
    LOG_GENERAL(WARNING, "ProtobufToAccountBase failed");
    return false;
  }

  return true;
}

bool Messenger::SetAccount(bytes& dst, const unsigned int offset,
                           const Account& account) {
  ProtoAccount result;

  AccountToProtobuf(account, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetAccount(const bytes& src, const unsigned int offset,
                           Account& account) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoAccount result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  Address address;

  if (!ProtobufToAccount(result, account, address)) {
    LOG_GENERAL(WARNING, "ProtobufToAccount failed");
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
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <class MAP>
bool Messenger::SetAccountStore(bytes& dst, const unsigned int offset,
                                const MAP& addressToAccount) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Accounts to serialize: " << addressToAccount.size());

  for (const auto& entry : addressToAccount) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    AccountToProtobuf(entry.second, *protoEntryAccount);
    if (!protoEntryAccount->IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
      return false;
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <class MAP>
bool Messenger::GetAccountStore(const bytes& src, const unsigned int offset,
                                MAP& addressToAccount) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoAccountStore result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  LOG_GENERAL(INFO, "Accounts deserialized: " << result.entries().size());

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
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoAccountStore result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  LOG_GENERAL(INFO, "Accounts deserialized: " << result.entries().size());

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

    accountStore.AddAccountDuringDeserialization(address, account, Account());
  }

  return true;
}

bool Messenger::SetAccountStoreDelta(bytes& dst, const unsigned int offset,
                                     AccountStoreTemp& accountStoreTemp,
                                     AccountStore& accountStore) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Account deltas to serialize: "
                        << accountStoreTemp.GetNumOfAccounts());

  for (const auto& entry : *accountStoreTemp.GetAddressToAccount()) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    AccountDeltaToProtobuf(accountStore.GetAccount(entry.first), entry.second,
                           *protoEntryAccount);
    if (!protoEntryAccount->IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
      return false;
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::StateDeltaToAddressMap(
    const bytes& src, const unsigned int offset,
    unordered_map<Address, int256_t>& accountMap) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

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
        entry.account().base().balance(), tmpNumber);

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
                                     const bool revertible, bool temp) {
  ProtoAccountStore result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

  LOG_GENERAL(INFO,
              "Total Number of Accounts Delta: " << result.entries().size());

  for (const auto& entry : result.entries()) {
    Address address;
    Account account, t_account;

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

    t_account = *oriAccount;
    account = *oriAccount;
    if (!ProtobufToAccountDelta(entry.account(), account, address, fullCopy,
                                temp, revertible)) {
      LOG_GENERAL(WARNING,
                  "ProtobufToAccountDelta failed for account at address "
                      << entry.address());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account, t_account,
                                                 fullCopy, revertible);
  }

  return true;
}

bool Messenger::GetAccountStoreDelta(const bytes& src,
                                     const unsigned int offset,
                                     AccountStoreTemp& accountStoreTemp,
                                     bool temp) {
  ProtoAccountStore result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
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

    if (!ProtobufToAccountDelta(entry.account(), account, address, fullCopy,
                                temp)) {
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
      LOG_GENERAL(WARNING, "ProtoMbInfo initialization failed");
      return false;
    }

    SerializeToArray(ProtoMbInfo, tmp, tmp.size());
  }

  // Fix software crash because of tmp is empty triggered assertion in
  // sha2.update.git
  if (tmp.empty()) {
    LOG_GENERAL(WARNING, "ProtoMbInfo is empty, proceed without it");
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
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSBlockHeader(const bytes& src, const unsigned int offset,
                                 DSBlockHeader& dsBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoDSBlock::DSBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock::DSBlockHeader initialization failed");
    return false;
  }

  return ProtobufToDSBlockHeader(result, dsBlockHeader);
}

bool Messenger::SetDSBlock(bytes& dst, const unsigned int offset,
                           const DSBlock& dsBlock) {
  ProtoDSBlock result;

  DSBlockToProtobuf(dsBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSBlock(const bytes& src, const unsigned int offset,
                           DSBlock& dsBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoDSBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDSBlock initialization failed");
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
                "ProtoMicroBlock::MicroBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMicroBlockHeader(const bytes& src, const unsigned int offset,
                                    MicroBlockHeader& microBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoMicroBlock::MicroBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoMicroBlock::MicroBlockHeader initialization failed");
    return false;
  }

  return ProtobufToMicroBlockHeader(result, microBlockHeader);
}

bool Messenger::SetMicroBlock(bytes& dst, const unsigned int offset,
                              const MicroBlock& microBlock) {
  ProtoMicroBlock result;

  MicroBlockToProtobuf(microBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMicroBlock(const bytes& src, const unsigned int offset,
                              MicroBlock& microBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoMicroBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlock initialization failed");
    return false;
  }

  return ProtobufToMicroBlock(result, microBlock);
}

bool Messenger::SetTxBlockHeader(bytes& dst, const unsigned int offset,
                                 const TxBlockHeader& txBlockHeader) {
  ProtoTxBlock::TxBlockHeader result;

  TxBlockHeaderToProtobuf(txBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTxBlockHeader(const bytes& src, const unsigned int offset,
                                 TxBlockHeader& txBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTxBlock::TxBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock::TxBlockHeader initialization failed");
    return false;
  }

  return ProtobufToTxBlockHeader(result, txBlockHeader);
}

bool Messenger::SetTxBlock(bytes& dst, const unsigned int offset,
                           const TxBlock& txBlock) {
  ProtoTxBlock result;

  TxBlockToProtobuf(txBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTxBlock(const bytes& src, const unsigned int offset,
                           TxBlock& txBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTxBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxBlock initialization failed");
    return false;
  }

  return ProtobufToTxBlock(result, txBlock);
}

bool Messenger::SetVCBlockHeader(bytes& dst, const unsigned int offset,
                                 const VCBlockHeader& vcBlockHeader) {
  ProtoVCBlock::VCBlockHeader result;

  VCBlockHeaderToProtobuf(vcBlockHeader, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock::VCBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetVCBlockHeader(const bytes& src, const unsigned int offset,
                                 VCBlockHeader& vcBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoVCBlock::VCBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock::VCBlockHeader initialization failed");
    return false;
  }

  return ProtobufToVCBlockHeader(result, vcBlockHeader);
}

bool Messenger::SetVCBlock(bytes& dst, const unsigned int offset,
                           const VCBlock& vcBlock) {
  ProtoVCBlock result;

  VCBlockToProtobuf(vcBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetVCBlock(const bytes& src, const unsigned int offset,
                           VCBlock& vcBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoVCBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoVCBlock initialization failed");
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
        "ProtoFallbackBlock::FallbackBlockHeader initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlockHeader(
    const bytes& src, const unsigned int offset,
    FallbackBlockHeader& fallbackBlockHeader) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoFallbackBlock::FallbackBlockHeader result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "ProtoFallbackBlock::FallbackBlockHeader initialization failed");
    return false;
  }

  return ProtobufToFallbackBlockHeader(result, fallbackBlockHeader);
}

bool Messenger::SetFallbackBlock(bytes& dst, const unsigned int offset,
                                 const FallbackBlock& fallbackBlock) {
  ProtoFallbackBlock result;

  FallbackBlockToProtobuf(fallbackBlock, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoFallbackBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlock(const bytes& src, const unsigned int offset,
                                 FallbackBlock& fallbackBlock) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoFallbackBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoFallbackBlock initialization failed");
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
    LOG_GENERAL(WARNING, "ProtoTransactionCoreInfo initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionCoreInfo(const bytes& src,
                                       const unsigned int offset,
                                       TransactionCoreInfo& transaction) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTransactionCoreInfo result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionCoreInfo initialization failed");
    return false;
  }

  return ProtobufToTransactionCoreInfo(result, transaction);
}

bool Messenger::SetTransaction(bytes& dst, const unsigned int offset,
                               const Transaction& transaction) {
  ProtoTransaction result;

  TransactionToProtobuf(transaction, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransaction initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransaction(const bytes& src, const unsigned int offset,
                               Transaction& transaction) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTransaction result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransaction initialization failed");
    return false;
  }

  return ProtobufToTransaction(result, transaction);
}

bool Messenger::SetTransactionFileOffset(
    bytes& dst, const unsigned int offset,
    const std::vector<uint32_t>& txnOffsets) {
  ProtoTxnFileOffset result;
  TransactionOffsetToProtobuf(txnOffsets, result);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxnFileOffset initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionFileOffset(const bytes& src,
                                         const unsigned int offset,
                                         std::vector<uint32_t>& txnOffsets) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTxnFileOffset result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxnFileOffset initialization failed");
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
    LOG_GENERAL(WARNING, "ProtoTransactionArray initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionArray(const bytes& src, const unsigned int offset,
                                    std::vector<Transaction>& txns) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTransactionArray result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionArray initialization failed");
    return false;
  }

  return ProtobufToTransactionArray(result, txns);
}

bool Messenger::SetTransactionReceipt(
    bytes& dst, const unsigned int offset,
    const TransactionReceipt& transactionReceipt) {
  ProtoTransactionReceipt result;

  TransactionReceiptToProtobuf(transactionReceipt, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionReceipt initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionReceipt(const bytes& src,
                                      const unsigned int offset,
                                      TransactionReceipt& transactionReceipt) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTransactionReceipt result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionReceipt initialization failed");
    return false;
  }

  return ProtobufToTransactionReceipt(result, transactionReceipt);
}

bool Messenger::SetTransactionWithReceipt(
    bytes& dst, const unsigned int offset,
    const TransactionWithReceipt& transactionWithReceipt) {
  ProtoTransactionWithReceipt result;

  TransactionWithReceiptToProtobuf(transactionWithReceipt, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionWithReceipt initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionWithReceipt(
    const bytes& src, const unsigned int offset,
    TransactionWithReceipt& transactionWithReceipt) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTransactionWithReceipt result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionWithReceipt initialization failed");
    return false;
  }

  return ProtobufToTransactionWithReceipt(result, transactionWithReceipt);
}

bool Messenger::SetStateIndex(bytes& dst, const unsigned int offset,
                              const vector<Contract::Index>& indexes) {
  ProtoStateIndex result;

  StateIndexToProtobuf(indexes, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoStateIndex initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetStateIndex(const bytes& src, const unsigned int offset,
                              vector<Contract::Index>& indexes) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoStateIndex result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoStateIndex initialization failed");
    return false;
  }

  return ProtobufToStateIndex(result, indexes);
}

bool Messenger::SetStateData(bytes& dst, const unsigned int offset,
                             const Contract::StateEntry& entry) {
  ProtoStateData result;

  StateDataToProtobuf(entry, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoStateData initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetStateData(const bytes& src, const unsigned int offset,
                             Contract::StateEntry& entry, uint32_t& version) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoStateData result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoStateData initialization failed");
    return false;
  }

  return ProtobufToStateData(result, entry, version);
}

bool Messenger::SetPeer(bytes& dst, const unsigned int offset,
                        const Peer& peer) {
  ProtoPeer result;

  PeerToProtobuf(peer, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoPeer initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetPeer(const bytes& src, const unsigned int offset,
                        Peer& peer) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoPeer result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoPeer initialization failed");
    return false;
  }

  ProtobufToPeer(result, peer);

  return true;
}

bool Messenger::SetBlockLink(
    bytes& dst, const unsigned int offset,
    const std::tuple<uint32_t, uint64_t, uint64_t, BlockType, BlockHash>&
        blocklink) {
  ProtoBlockLink result;

  result.set_version(get<BlockLinkIndex::VERSION>(blocklink));
  result.set_index(get<BlockLinkIndex::INDEX>(blocklink));
  result.set_dsindex(get<BlockLinkIndex::DSINDEX>(blocklink));
  result.set_blocktype(get<BlockLinkIndex::BLOCKTYPE>(blocklink));
  BlockHash blkhash = get<BlockLinkIndex::BLOCKHASH>(blocklink);
  result.set_blockhash(blkhash.data(), blkhash.size);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoBlockLink initialization failed");
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetBlockLink(
    const bytes& src, const unsigned int offset,
    std::tuple<uint32_t, uint64_t, uint64_t, BlockType, BlockHash>& blocklink) {
  ProtoBlockLink result;
  BlockHash blkhash;

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoBlockLink initialization failed");
    return false;
  }

  if (!CheckRequiredFieldsProtoBlockLink(result)) {
    LOG_GENERAL(WARNING, "CheckRequiredFieldsProtoBlockLink failed");
    return false;
  }

  get<BlockLinkIndex::VERSION>(blocklink) = result.version();
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
    const uint32_t& shardingStructureVersion, const DequeOfShard& shards) {
  ProtoFallbackBlockWShardingStructure result;

  FallbackBlockToProtobuf(fallbackblock, *result.mutable_fallbackblock());
  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_sharding());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoFallbackBlockWShardingStructure initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetFallbackBlockWShardingStructure(
    const bytes& src, const unsigned int offset, FallbackBlock& fallbackblock,
    uint32_t& shardingStructureVersion, DequeOfShard& shards) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoFallbackBlockWShardingStructure result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ProtoFallbackBlockWShardingStructure initialization failed");
    return false;
  }

  if (!result.has_fallbackblock() || !result.has_sharding()) {
    LOG_GENERAL(
        WARNING,
        "GetFallbackBlockWShardingStructure check required field failed");
    return false;
  }

  ProtobufToFallbackBlock(result.fallbackblock(), fallbackblock);

  return ProtobufToShardingStructure(result.sharding(),
                                     shardingStructureVersion, shards);
}

bool Messenger::SetDiagnosticDataNodes(bytes& dst, const unsigned int offset,
                                       const uint32_t& shardingStructureVersion,
                                       const DequeOfShard& shards,
                                       const uint32_t& dsCommitteeVersion,
                                       const DequeOfNode& dsCommittee) {
  ProtoDiagnosticDataNodes result;

  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_shards());
  DSCommitteeToProtobuf(dsCommitteeVersion, dsCommittee,
                        *result.mutable_dscommittee());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticDataNodes initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDiagnosticDataNodes(const bytes& src,
                                       const unsigned int offset,
                                       uint32_t& shardingStructureVersion,
                                       DequeOfShard& shards,
                                       uint32_t& dsCommitteeVersion,
                                       DequeOfNode& dsCommittee) {
  ProtoDiagnosticDataNodes result;

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticDataNodes initialization failed");
    return false;
  }

  if (!ProtobufToShardingStructure(result.shards(), shardingStructureVersion,
                                   shards)) {
    LOG_GENERAL(WARNING, "ProtobufToShardingStructure failed");
    return false;
  }

  return ProtobufToDSCommittee(result.dscommittee(), dsCommitteeVersion,
                               dsCommittee);
}

bool Messenger::SetDiagnosticDataCoinbase(bytes& dst, const unsigned int offset,
                                          const DiagnosticDataCoinbase& entry) {
  ProtoDiagnosticDataCoinbase result;

  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.nodeCount, *result.mutable_nodecount());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.sigCount, *result.mutable_sigcount());
  result.set_lookupcount(entry.lookupCount);
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.totalReward, *result.mutable_totalreward());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.baseReward, *result.mutable_basereward());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.baseRewardEach, *result.mutable_baserewardeach());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.lookupReward, *result.mutable_lookupreward());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.rewardEachLookup, *result.mutable_rewardeachlookup());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.nodeReward, *result.mutable_nodereward());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.rewardEach, *result.mutable_rewardeach());
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      entry.balanceLeft, *result.mutable_balanceleft());
  SerializableToProtobufByteArray(entry.luckyDrawWinnerKey,
                                  *result.mutable_luckydrawwinnerkey());
  result.set_luckydrawwinneraddr(entry.luckyDrawWinnerAddr.data(),
                                 entry.luckyDrawWinnerAddr.size);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticDataCoinbase initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDiagnosticDataCoinbase(const bytes& src,
                                          const unsigned int offset,
                                          DiagnosticDataCoinbase& entry) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoDiagnosticDataCoinbase result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoDiagnosticDataCoinbase initialization failed");
    return false;
  }

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.nodecount(),
                                                     entry.nodeCount);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.sigcount(),
                                                     entry.sigCount);
  entry.lookupCount = result.lookupcount();
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.totalreward(),
                                                     entry.totalReward);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.basereward(),
                                                     entry.baseReward);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.baserewardeach(),
                                                     entry.baseRewardEach);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.lookupreward(),
                                                     entry.lookupReward);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.rewardeachlookup(),
                                                     entry.rewardEachLookup);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.nodereward(),
                                                     entry.nodeReward);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.rewardeach(),
                                                     entry.rewardEach);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.balanceleft(),
                                                     entry.balanceLeft);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.luckydrawwinnerkey(),
                                  entry.luckyDrawWinnerKey);
  copy(result.luckydrawwinneraddr().begin(),
       result.luckydrawwinneraddr().begin() +
           min((unsigned int)result.luckydrawwinneraddr().size(),
               (unsigned int)entry.luckyDrawWinnerAddr.size),
       entry.luckyDrawWinnerAddr.asArray().begin());

  return true;
}

// ============================================================================
// Peer Manager messages
// ============================================================================

bool Messenger::SetPMHello(bytes& dst, const unsigned int offset,
                           const PairOfKey& key, const uint32_t listenPort) {
  LOG_MARKER();

  PMHello result;

  SerializableToProtobufByteArray(key.second,
                                  *result.mutable_data()->mutable_pubkey());
  result.mutable_data()->set_listenport(listenPort);

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, key.first, key.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign PMHello.data");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetPMHello(const bytes& src, const unsigned int offset,
                           PubKey& pubKey, uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  PMHello result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().pubkey(), pubKey);
  listenPort = result.data().listenport();

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "PMHello signature wrong");
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
    const PairOfKey& submitterKey, const uint64_t nonce,
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

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  // We use MultiSig::SignKey to emphasize that this is for the
  // Proof-of-Possession (PoP) phase (refer to #1097)
  Signature signature;
  if (!MultiSig::GetInstance().SignKey(tmp, submitterKey, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign PoW");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  DSPoWSubmission result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission initialization failed");
    return false;
  }

  blockNumber = result.data().blocknumber();
  difficultyLevel = result.data().difficultylevel();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().submitterpeer(), submitterPeer);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().submitterpubkey(),
                                  submitterPubKey);
  nonce = result.data().nonce();
  resultingHash = result.data().resultinghash();
  mixHash = result.data().mixhash();
  lookupId = result.data().lookupid();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.data().gasprice(),
                                                     gasPrice);

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  // We use MultiSig::VerifyKey to emphasize that this is for the
  // Proof-of-Possession (PoP) phase (refer to #1097)
  if (!MultiSig::GetInstance().VerifyKey(tmp, signature, submitterPubKey)) {
    LOG_GENERAL(WARNING, "PoW submission signature wrong");
    return false;
  }

  return true;
}

bool Messenger::SetDSPoWPacketSubmission(
    bytes& dst, const unsigned int offset,
    const vector<DSPowSolution>& dsPowSolutions, const PairOfKey& keys) {
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
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSPowPacketSubmission(const bytes& src,
                                         const unsigned int offset,
                                         vector<DSPowSolution>& dsPowSolutions,
                                         PubKey& pubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  DSPoWPacketSubmission result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission signature wrong");
    return false;
  }

  for (const auto& powSubmission : result.data().dspowsubmissions()) {
    DSPowSolution sol;
    ProtobufToDSPowSolution(powSubmission, sol);
    dsPowSolutions.emplace_back(move(sol));
  }

  return true;
}

bool Messenger::SetDSMicroBlockSubmission(bytes& dst, const unsigned int offset,
                                          const unsigned char microBlockType,
                                          const uint64_t epochNumber,
                                          const vector<MicroBlock>& microBlocks,
                                          const vector<bytes>& stateDeltas,
                                          const PairOfKey& keys) {
  LOG_MARKER();

  DSMicroBlockSubmission result;

  result.mutable_data()->set_microblocktype(microBlockType);
  result.mutable_data()->set_epochnumber(epochNumber);
  for (const auto& microBlock : microBlocks) {
    MicroBlockToProtobuf(microBlock, *result.mutable_data()->add_microblocks());
  }
  for (const auto& stateDelta : stateDeltas) {
    result.mutable_data()->add_statedeltas(stateDelta.data(),
                                           stateDelta.size());
  }

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission.Data initialization failed");
    return false;
  }

  bytes tmp(result.mutable_data()->ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, keys.first, keys.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DSMicroBlockSubmission");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());
  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSMicroBlockSubmission(
    const bytes& src, const unsigned int offset, unsigned char& microBlockType,
    uint64_t& epochNumber, vector<MicroBlock>& microBlocks,
    vector<bytes>& stateDeltas, PubKey& pubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  DSMicroBlockSubmission result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission signature wrong");
    return false;
  }

  // Deserialize the remaining fields
  microBlockType = result.data().microblocktype();
  epochNumber = result.data().epochnumber();
  for (const auto& proto_mb : result.data().microblocks()) {
    MicroBlock microBlock;
    ProtobufToMicroBlock(proto_mb, microBlock);
    microBlocks.emplace_back(move(microBlock));
  }
  for (const auto& proto_delta : result.data().statedeltas()) {
    stateDeltas.emplace_back();
    copy(proto_delta.begin(), proto_delta.end(),
         std::back_inserter(stateDeltas.back()));
  }

  return true;
}

bool Messenger::SetDSDSBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey, const DSBlock& dsBlock,
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
    LOG_GENERAL(WARNING, "DSBlockHeader serialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

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
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed");
    return false;
  }

  // Get the DSBlock announcement parameters

  const DSDSBlockAnnouncement& dsblock = announcement.dsblock();

  if (!ProtobufToDSBlock(dsblock.dsblock(), dsBlock)) {
    return false;
  }

  if (!ProtobufToShardingStructureAnnouncement(dsblock.sharding(), shards,
                                               allPoWs)) {
    LOG_GENERAL(WARNING, "ProtobufToShardingStructureAnnouncement failed");
    return false;
  }

  dsWinnerPoWs.clear();
  for (const auto& protoDSWinnerPoW : dsblock.dswinnerpows()) {
    PubKey key;
    std::array<unsigned char, 32> result;
    std::array<unsigned char, 32> mixhash;
    uint128_t gasPrice;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(protoDSWinnerPoW.pubkey(), key);

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
    LOG_GENERAL(WARNING, "DSBlockHeader serialization failed");
    return false;
  }

  return true;
}

bool Messenger::SetDSFinalBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey, const TxBlock& txBlock,
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
    LOG_GENERAL(WARNING, "DSFinalBlockAnnouncement initialization failed");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!txBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusAnnouncement announcement;
  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed");
    return false;
  }

  if (!announcement.has_finalblock()) {
    LOG_GENERAL(WARNING, "DSFinalBlockAnnouncement initialization failed");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed");
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
    LOG_GENERAL(WARNING, "TxBlockHeader serialization failed");
    return false;
  }

  return true;
}

bool Messenger::SetDSVCBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey, const VCBlock& vcBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the VCBlock announcement parameters

  DSVCBlockAnnouncement* vcblock = announcement.mutable_vcblock();
  SerializableToProtobufByteArray(vcBlock, *vcblock->mutable_vcblock());

  if (!vcblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "DSVCBlockAnnouncement initialization failed");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!vcBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "VCBlockHeader serialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusAnnouncement announcement;
  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed");
    return false;
  }

  if (!announcement.has_vcblock()) {
    LOG_GENERAL(WARNING, "DSVCBlockAnnouncement initialization failed");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed");
    return false;
  }

  // Get the VCBlock announcement parameters

  const DSVCBlockAnnouncement& vcblock = announcement.vcblock();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(vcblock.vcblock(), vcBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!vcBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "VCBlockHeader serialization failed");
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
    LOG_GENERAL(WARNING, "DSMissingMicroBlocksErrorMsg initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSMissingMicroBlocksErrorMsg(
    const bytes& src, const unsigned int offset,
    vector<BlockHash>& missingMicroBlockHashes, uint64_t& epochNum,
    uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  DSMissingMicroBlocksErrorMsg result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMissingMicroBlocksErrorMsg initialization failed");
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

bool Messenger::SetNodeVCDSBlocksMessage(
    bytes& dst, const unsigned int offset, const uint32_t shardId,
    const DSBlock& dsBlock, const std::vector<VCBlock>& vcBlocks,
    const uint32_t& shardingStructureVersion, const DequeOfShard& shards) {
  LOG_MARKER();

  NodeDSBlock result;

  result.set_shardid(shardId);
  DSBlockToProtobuf(dsBlock, *result.mutable_dsblock());

  for (const auto& vcblock : vcBlocks) {
    VCBlockToProtobuf(vcblock, *result.add_vcblocks());
  }
  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_sharding());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeDSBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCDSBlocksMessage(const bytes& src,
                                         const unsigned int offset,
                                         uint32_t& shardId, DSBlock& dsBlock,
                                         std::vector<VCBlock>& vcBlocks,
                                         uint32_t& shardingStructureVersion,
                                         DequeOfShard& shards) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeDSBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeDSBlock initialization failed");
    return false;
  }

  shardId = result.shardid();
  if (!ProtobufToDSBlock(result.dsblock(), dsBlock)) {
    return false;
  }

  for (const auto& proto_vcblock : result.vcblocks()) {
    VCBlock vcblock;
    if (!ProtobufToVCBlock(proto_vcblock, vcblock)) {
      LOG_GENERAL(WARNING, "ProtobufToVCBlock failed");
      return false;
    }
    vcBlocks.emplace_back(move(vcblock));
  }

  return ProtobufToShardingStructure(result.sharding(),
                                     shardingStructureVersion, shards);
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
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFinalBlock(const bytes& src, const unsigned int offset,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  bytes& stateDelta) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeFinalBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed");
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
    LOG_GENERAL(WARNING, "SetNodeMBnForwardTransaction initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeMBnForwardTransaction result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTransaction initialization failed");
    return false;
  }

  ProtobufToMicroBlock(result.microblock(), entry.m_microBlock);

  unsigned int txnsCount = 0;

  for (const auto& txn : result.txnswithreceipt()) {
    TransactionWithReceipt txr;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(txn, txr);
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
    LOG_GENERAL(WARNING, "NodeVCBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCBlock(const bytes& src, const unsigned int offset,
                               VCBlock& vcBlock) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeVCBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeVCBlock initialization failed");
    return false;
  }

  return ProtobufToVCBlock(result.vcblock(), vcBlock);
}

bool Messenger::SetNodeForwardTxnBlock(
    bytes& dst, const unsigned int offset, const uint64_t& epochNumber,
    const uint64_t& dsBlockNum, const uint32_t& shardId,
    const PairOfKey& lookupKey, const std::vector<Transaction>& txnsCurrent,
    const std::vector<Transaction>& txnsGenerated) {
  LOG_MARKER();

  NodeForwardTxnBlock result;

  result.set_epochnumber(epochNumber);
  result.set_dsblocknum(dsBlockNum);
  result.set_shardid(shardId);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  unsigned int txnsCurrentCount = 0;
  unsigned int txnsGeneratedCount = 0;

  unsigned int msg_size = 0;

  for (const auto& txn : txnsCurrent) {
    if (msg_size >= PACKET_BYTESIZE_LIMIT) {
      break;
    }
    ProtoTransaction* protoTxn = new ProtoTransaction();
    TransactionToProtobuf(txn, *protoTxn);
    unsigned txn_size = protoTxn->ByteSize();
    if ((msg_size + txn_size) > PACKET_BYTESIZE_LIMIT &&
        txn_size >= SMALL_TXN_SIZE) {
      continue;
    }
    *result.add_transactions() = *protoTxn;
    txnsCurrentCount++;
    msg_size += protoTxn->ByteSize();
  }

  for (const auto& txn : txnsGenerated) {
    if (msg_size >= PACKET_BYTESIZE_LIMIT) {
      break;
    }
    ProtoTransaction* protoTxn = new ProtoTransaction();
    TransactionToProtobuf(txn, *protoTxn);
    unsigned txn_size = protoTxn->ByteSize();
    if ((msg_size + txn_size) > PACKET_BYTESIZE_LIMIT &&
        txn_size >= SMALL_TXN_SIZE) {
      continue;
    }
    *result.add_transactions() = *protoTxn;
    txnsGeneratedCount++;
    msg_size += txn_size;
  }

  Signature signature;
  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign transactions");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed");
    return false;
  }

  LOG_GENERAL(INFO, "Epoch: " << epochNumber << " shardId: " << shardId
                              << " Current txns: " << txnsCurrentCount
                              << " Generated txns: " << txnsGeneratedCount);

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetNodeForwardTxnBlock(bytes& dst, const unsigned int offset,
                                       const uint64_t& epochNumber,
                                       const uint64_t& dsBlockNum,
                                       const uint32_t& shardId,
                                       const PubKey& lookupKey,
                                       std::vector<Transaction>& txns,
                                       const Signature& signature) {
  LOG_MARKER();

  NodeForwardTxnBlock result;

  result.set_epochnumber(epochNumber);
  result.set_dsblocknum(dsBlockNum);
  result.set_shardid(shardId);
  SerializableToProtobufByteArray(lookupKey, *result.mutable_pubkey());

  unsigned int txnsCount = 0;

  unsigned int msg_size = 0;

  for (const auto& txn : txns) {
    if (msg_size >= PACKET_BYTESIZE_LIMIT) {
      break;
    }
    ProtoTransaction* protoTxn = new ProtoTransaction();
    TransactionToProtobuf(txn, *protoTxn);
    unsigned txn_size = protoTxn->ByteSize();
    if ((msg_size + txn_size) > PACKET_BYTESIZE_LIMIT &&
        txn_size >= SMALL_TXN_SIZE) {
      continue;
    }
    *result.add_transactions() = *protoTxn;
    txnsCount++;
    msg_size += txn_size;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed");
    return false;
  }

  LOG_GENERAL(INFO, "Epoch: " << epochNumber << " shardId: " << shardId
                              << " Txns: " << txnsCount);

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeForwardTxnBlock(
    const bytes& src, const unsigned int offset, uint64_t& epochNumber,
    uint64_t& dsBlockNum, uint32_t& shardId, PubKey& lookupPubKey,
    std::vector<Transaction>& txns, Signature& signature) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeForwardTxnBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed");
    return false;
  }

  epochNumber = result.epochnumber();
  dsBlockNum = result.dsblocknum();
  shardId = result.shardid();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);

  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }
    PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions");
      return false;
    }

    for (const auto& txn : result.transactions()) {
      Transaction t;
      if (!ProtobufToTransaction(txn, t)) {
        LOG_GENERAL(WARNING, "ProtobufToTransaction failed");
        return false;
      }
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
    const PairOfKey& leaderKey, const MicroBlock& microBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the MicroBlock announcement parameters

  NodeMicroBlockAnnouncement* microblock = announcement.mutable_microblock();
  MicroBlockToProtobuf(microBlock, *microblock->mutable_microblock());

  if (!microblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMicroBlockAnnouncement initialization failed");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!microBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusAnnouncement announcement;
  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed");
    return false;
  }

  if (!announcement.has_microblock()) {
    LOG_GENERAL(WARNING, "NodeMicroBlockAnnouncement initialization failed");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed");
    return false;
  }

  // Get the MicroBlock announcement parameters

  const NodeMicroBlockAnnouncement& microblock = announcement.microblock();
  ProtobufToMicroBlock(microblock.microblock(), microBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!microBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed");
    return false;
  }

  return true;
}

bool Messenger::SetNodeFallbackBlockAnnouncement(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey, const FallbackBlock& fallbackBlock,
    bytes& messageToCosign) {
  LOG_MARKER();

  ConsensusAnnouncement announcement;

  // Set the FallbackBlock announcement parameters

  NodeFallbackBlockAnnouncement* fallbackblock =
      announcement.mutable_fallbackblock();
  SerializableToProtobufByteArray(fallbackBlock,
                                  *fallbackblock->mutable_fallbackblock());

  if (!fallbackblock->IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFallbackBlockAnnouncement initialization failed");
    return false;
  }

  // Set the common consensus announcement parameters

  if (!SetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "SetConsensusAnnouncementCore failed");
    return false;
  }

  // Serialize the part of the announcement that should be co-signed during the
  // first round of consensus

  messageToCosign.clear();
  if (!fallbackBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "FallbackBlockHeader serialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusAnnouncement announcement;
  announcement.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!announcement.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusAnnouncement initialization failed");
    return false;
  }

  if (!announcement.has_fallbackblock()) {
    LOG_GENERAL(WARNING, "NodeFallbackBlockAnnouncement initialization failed");
    return false;
  }

  // Check the common consensus announcement parameters

  if (!GetConsensusAnnouncementCore(announcement, consensusID, blockNumber,
                                    blockHash, leaderID, leaderKey)) {
    LOG_GENERAL(WARNING, "GetConsensusAnnouncementCore failed");
    return false;
  }

  // Get the FallbackBlock announcement parameters

  const NodeFallbackBlockAnnouncement& fallbackblock =
      announcement.fallbackblock();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(fallbackblock.fallbackblock(), fallbackBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!fallbackBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "FallbackBlockHeader serialization failed");
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
    LOG_GENERAL(WARNING, "NodeFallbackBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFallbackBlock(const bytes& src,
                                     const unsigned int offset,
                                     FallbackBlock& fallbackBlock) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeFallbackBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFallbackBlock initialization failed");
    return false;
  }

  ProtobufToFallbackBlock(result.fallbackblock(), fallbackBlock);

  return true;
}

bool Messenger::ShardStructureToArray(bytes& dst, const unsigned int offset,
                                      const uint32_t& version,
                                      const DequeOfShard& shards) {
  ProtoShardingStructure protoShardingStructure;
  ShardingStructureToProtobuf(version, shards, protoShardingStructure);

  if (!protoShardingStructure.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure initialization failed");
    return false;
  }

  if (!SerializeToArray(protoShardingStructure, dst, offset)) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure serialization failed");
    return false;
  }

  return true;
}

bool Messenger::ArrayToShardStructure(const bytes& src,
                                      const unsigned int offset,
                                      uint32_t& version, DequeOfShard& shards) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoShardingStructure protoShardingStructure;
  protoShardingStructure.ParseFromArray(src.data() + offset,
                                        src.size() - offset);
  return ProtobufToShardingStructure(protoShardingStructure, version, shards);
}

bool Messenger::SetNodeMissingTxnsErrorMsg(
    bytes& dst, const unsigned int offset,
    const vector<TxnHash>& missingTxnHashes, const uint64_t epochNum,
    const uint32_t listenPort) {
  LOG_MARKER();

  NodeMissingTxnsErrorMsg result;

  for (const auto& hash : missingTxnHashes) {
    LOG_EPOCH(INFO, epochNum, "Missing txn: " << hash);
    result.add_txnhashes(hash.data(), hash.size);
  }

  result.set_epochnum(epochNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMissingTxnsErrorMsg initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeMissingTxnsErrorMsg result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMissingTxnsErrorMsg initialization failed");
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
    LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetSeedPeers(const bytes& src,
                                      const unsigned int offset,
                                      uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetSeedPeers result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetSeedPeers(bytes& dst, const unsigned int offset,
                                      const PairOfKey& lookupKey,
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
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds");
      return false;
    }
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign candidate seeds");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetSeedPeers(const bytes& src,
                                      const unsigned int offset,
                                      PubKey& lookupPubKey,
                                      vector<Peer>& candidateSeeds) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetSeedPeers result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetSeedPeers initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);

  for (const auto& peer : result.candidateseeds()) {
    Peer seedPeer;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(peer, seedPeer);
    candidateSeeds.emplace_back(seedPeer);
  }

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (result.candidateseeds().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.candidateseeds(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in candidate seeds");
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
    LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSInfoFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort,
                                           bool& initialDS) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetDSInfoFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed");
    return false;
  }

  listenPort = result.listenport();
  initialDS = result.initialds();

  return true;
}

bool Messenger::SetLookupSetDSInfoFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const PairOfKey& senderKey,
                                           const uint32_t& dsCommitteeVersion,
                                           const DequeOfNode& dsNodes,
                                           const bool initialDS) {
  LOG_MARKER();

  LookupSetDSInfoFromSeed result;

  DSCommitteeToProtobuf(dsCommitteeVersion, dsNodes,
                        *result.mutable_dscommittee());

  SerializableToProtobufByteArray(senderKey.second, *result.mutable_pubkey());

  bytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee");
    return false;
  }

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, senderKey.first, senderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DS committee");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  result.set_initialds(initialDS);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSInfoFromSeed(
    const bytes& src, const unsigned int offset, PubKey& senderPubKey,
    uint32_t& dsCommitteeVersion, DequeOfNode& dsNodes, bool& initialDS) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetDSInfoFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSInfoFromSeed initialization failed");
    return false;
  }

  if (!ProtobufToDSCommittee(result.dscommittee(), dsCommitteeVersion,
                             dsNodes)) {
    LOG_GENERAL(WARNING, "ProtobufToDSCommittee failed");
    return false;
  }

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  bytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee");
    return false;
  }

  initialDS = result.initialds();

  if (!Schnorr::GetInstance().Verify(tmp, signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in DS nodes info");
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
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetDSBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetDSBlockFromSeed(bytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const PairOfKey& lookupKey,
                                            const vector<DSBlock>& dsBlocks) {
  LOG_MARKER();

  LookupSetDSBlockFromSeed result;

  result.mutable_data()->set_lowblocknum(lowBlockNum);
  result.mutable_data()->set_highblocknum(highBlockNum);

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  for (const auto& dsblock : dsBlocks) {
    DSBlockToProtobuf(dsblock, *result.mutable_data()->add_dsblocks());
  }

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DS blocks");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<DSBlock>& dsBlocks) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetDSBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.data().lowblocknum();
  highBlockNum = result.data().highblocknum();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);

  for (const auto& proto_dsblock : result.data().dsblocks()) {
    DSBlock dsblock;
    if (!ProtobufToDSBlock(proto_dsblock, dsblock)) {
      LOG_GENERAL(WARNING, "ProtobufToDSBlock failed");
      return false;
    }
    dsBlocks.emplace_back(dsblock);
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetDSBlockFromSeed");
    return false;
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
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetTxBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxBlockFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetTxBlockFromSeed(bytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const PairOfKey& lookupKey,
                                            const vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  LookupSetTxBlockFromSeed result;

  result.mutable_data()->set_lowblocknum(lowBlockNum);
  result.mutable_data()->set_highblocknum(highBlockNum);

  for (const auto& txblock : txBlocks) {
    TxBlockToProtobuf(txblock, *result.mutable_data()->add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign tx blocks");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetTxBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  LookupSetTxBlockFromSeed result;

  google::protobuf::io::ArrayInputStream arrayIn(src.data() + offset,
                                                 src.size() - offset);
  google::protobuf::io::CodedInputStream codedIn(&arrayIn);
  codedIn.SetTotalBytesLimit(MAX_READ_WATERMARK_IN_BYTES,
                             MAX_READ_WATERMARK_IN_BYTES);

  if (!result.ParseFromCodedStream(&codedIn) ||
      !codedIn.ConsumedEntireMessage() || !result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.data().lowblocknum();
  highBlockNum = result.data().highblocknum();

  for (const auto& txblock : result.data().txblocks()) {
    TxBlock block;
    if (!ProtobufToTxBlock(txblock, block)) {
      LOG_GENERAL(WARNING, "ProtobufToTxBlock failed");
      return false;
    }
    txBlocks.emplace_back(block);
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetTxBlockFromSeed");
    return false;
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
    LOG_GENERAL(WARNING, "LookupGetStateDeltaFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetLookupGetStateDeltasFromSeed(bytes& dst,
                                                const unsigned int offset,
                                                uint64_t& lowBlockNum,
                                                uint64_t& highBlockNum,
                                                const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetStateDeltasFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateDeltasFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStateDeltaFromSeed(const bytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetStateDeltaFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateDeltaFromSeed initialization failed");
    return false;
  }

  blockNum = result.blocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::GetLookupGetStateDeltasFromSeed(const bytes& src,
                                                const unsigned int offset,
                                                uint64_t& lowBlockNum,
                                                uint64_t& highBlockNum,
                                                uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetStateDeltasFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateDeltasFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.lowblocknum();
  highBlockNum = result.highblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetStateDeltaFromSeed(bytes& dst,
                                               const unsigned int offset,
                                               const uint64_t blockNum,
                                               const PairOfKey& lookupKey,
                                               const bytes& stateDelta) {
  LOG_MARKER();

  LookupSetStateDeltaFromSeed result;

  result.mutable_data()->set_blocknum(blockNum);

  result.mutable_data()->set_statedelta(stateDelta.data(), stateDelta.size());

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetStateDeltaFromSeed.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign StateDelta");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltaFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetLookupSetStateDeltasFromSeed(
    bytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
    const uint64_t highBlockNum, const PairOfKey& lookupKey,
    const vector<bytes>& stateDeltas) {
  LOG_MARKER();

  LookupSetStateDeltasFromSeed result;

  result.mutable_data()->set_lowblocknum(lowBlockNum);
  result.mutable_data()->set_highblocknum(highBlockNum);

  for (const auto& delta : stateDeltas) {
    result.mutable_data()->add_statedeltas(delta.data(), delta.size());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetStateDeltasFromSeed.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign StateDeltas");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltasFromSeed initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetStateDeltaFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltaFromSeed initialization failed");
    return false;
  }

  blockNum = result.data().blocknum();

  stateDelta.resize(result.data().statedelta().size());
  std::copy(result.data().statedelta().begin(),
            result.data().statedelta().end(), stateDelta.begin());

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in state delta");
    return false;
  }

  return true;
}

bool Messenger::GetLookupSetStateDeltasFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<bytes>& stateDeltas) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetStateDeltasFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateDeltasFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.data().lowblocknum();
  highBlockNum = result.data().highblocknum();
  stateDeltas.clear();
  for (const auto& delta : result.data().statedeltas()) {
    bytes tmp;
    tmp.resize(delta.size());
    std::copy(delta.begin(), delta.end(), tmp.begin());
    stateDeltas.emplace_back(tmp);
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in state deltas");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetStateFromSeed(bytes& dst, const unsigned int offset,
                                          const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetStateFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStateFromSeed(const bytes& src,
                                          const unsigned int offset,
                                          uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetStateFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateFromSeed initialization failed");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetStateFromSeed(bytes& dst, const unsigned int offset,
                                          const PairOfKey& lookupKey,
                                          const AccountStore& accountStore) {
  LOG_MARKER();

  LookupSetStateFromSeed result;

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;

  bytes tmp;

  if (!accountStore.Serialize(tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize AccountStore");
    return false;
  }
  result.mutable_accountstore()->set_data(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign accounts");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed");
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

  google::protobuf::io::ArrayInputStream arrayIn(src.data() + offset,
                                                 src.size() - offset);
  google::protobuf::io::CodedInputStream codedIn(&arrayIn);
  codedIn.SetTotalBytesLimit(MAX_READ_WATERMARK_IN_BYTES,
                             MAX_READ_WATERMARK_IN_BYTES);

  if (!result.ParseFromCodedStream(&codedIn) ||
      !codedIn.ConsumedEntireMessage() || !result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStateFromSeed initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  copy(result.accountstore().data().begin(), result.accountstore().data().end(),
       back_inserter(accountStoreBytes));

  if (!Schnorr::GetInstance().Verify(accountStoreBytes, signature,
                                     lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in accounts");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetLookupOffline(bytes& dst, const unsigned int offset,
                                          const uint8_t msgType,
                                          const uint32_t listenPort,
                                          const PairOfKey& lookupKey) {
  LOG_MARKER();

  LookupSetLookupOffline result;

  result.mutable_data()->set_msgtype(msgType);
  result.mutable_data()->set_listenport(listenPort);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign set lookup offline message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOffline(const bytes& src,
                                          const unsigned int offset,
                                          uint8_t& msgType,
                                          uint32_t& listenPort,
                                          PubKey& lookupPubkey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetLookupOffline result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline initialization failed");
    return false;
  }

  listenPort = result.data().listenport();
  msgType = result.data().msgtype();

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubkey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubkey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetLookupOffline");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetLookupOnline(bytes& dst, const unsigned int offset,
                                         const uint8_t msgType,
                                         const uint32_t listenPort,
                                         const PairOfKey& lookupKey) {
  LOG_MARKER();

  LookupSetLookupOnline result;

  result.mutable_data()->set_msgtype(msgType);
  result.mutable_data()->set_listenport(listenPort);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign set lookup online message");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetLookupOnline(const bytes& src,
                                         const unsigned int offset,
                                         uint8_t& msgType, uint32_t& listenPort,
                                         PubKey& pubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetLookupOnline result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline initialization failed");
    return false;
  }

  msgType = result.data().msgtype();
  listenPort = result.data().listenport();

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, pubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetLookupOnline");
    return false;
  }
  return true;
}

bool Messenger::SetLookupGetOfflineLookups(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetOfflineLookups result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetOfflineLookups(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetOfflineLookups result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetLookupSetOfflineLookups(bytes& dst,
                                           const unsigned int offset,
                                           const PairOfKey& lookupKey,
                                           const vector<Peer>& nodes) {
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
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign offline lookup nodes");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetOfflineLookups(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           vector<Peer>& nodes) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetOfflineLookups result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetOfflineLookups initialization failed");
    return false;
  }

  for (const auto& lookup : result.nodes()) {
    Peer node;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(lookup, node);
    nodes.emplace_back(node);
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (result.nodes().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.nodes(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in offline lookup nodes");
      return false;
    }
  }

  return true;
}

bool Messenger::GetLookupSetRaiseStartPoW(const bytes& src,
                                          const unsigned int offset,
                                          uint8_t& msgType,
                                          uint64_t& blockNumber,
                                          PubKey& dsPubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupRaiseStartPoW result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupRaiseStartPoW initialization failed");
    return false;
  }

  msgType = result.data().msgtype();
  blockNumber = result.data().blocknumber();

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), dsPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, dsPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in raise start PoW message");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetRaiseStartPoW(bytes& dst, const unsigned int offset,
                                          const uint8_t msgType,
                                          const uint64_t blockNumber,
                                          const PairOfKey& dsKey) {
  LOG_MARKER();

  LookupRaiseStartPoW result;

  result.mutable_data()->set_msgtype(msgType);
  result.mutable_data()->set_blocknumber(blockNumber);
  SerializableToProtobufByteArray(dsKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupRaiseStartPoW.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, dsKey.first, dsKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign raise start PoW message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupRaiseStartPoW initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetLookupGetStartPoWFromSeed(bytes& dst,
                                             const unsigned int offset,
                                             const uint32_t listenPort,
                                             const uint64_t blockNumber,
                                             const PairOfKey& keys) {
  LOG_MARKER();

  LookupGetStartPoWFromSeed result;

  result.mutable_data()->set_listenport(listenPort);
  result.mutable_data()->set_blocknumber(blockNumber);

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetStartPoWFromSeed.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, keys.first, keys.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign GetStartPoWFromSeed message");
    return false;
  }

  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStartPoWFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetStartPoWFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             uint32_t& listenPort,
                                             uint64_t& blockNumber) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetStartPoWFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStartPoWFromSeed initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PubKey pubKey;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, pubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetStartPoWFromSeed message");
    return false;
  }

  listenPort = result.data().listenport();
  blockNumber = result.data().blocknumber();

  return true;
}

bool Messenger::SetLookupSetStartPoWFromSeed(bytes& dst,
                                             const unsigned int offset,
                                             const uint64_t blockNumber,
                                             const PairOfKey& lookupKey) {
  LOG_MARKER();

  LookupSetStartPoWFromSeed result;

  result.set_blocknumber(blockNumber);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  bytes tmp;
  NumberToArray<uint64_t, sizeof(uint64_t)>(blockNumber, tmp, 0);

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign start PoW message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStartPoWFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetStartPoWFromSeed(const bytes& src,
                                             const unsigned int offset,
                                             PubKey& lookupPubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetStartPoWFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetStartPoWFromSeed initialization failed");
    return false;
  }

  bytes tmp;
  NumberToArray<uint64_t, sizeof(uint64_t)>(result.blocknumber(), tmp, 0);

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in start PoW message");
    return false;
  }

  return true;
}

bool Messenger::SetForwardTxnBlockFromSeed(
    bytes& dst, const unsigned int offset,
    const vector<Transaction>& shardTransactions,
    const vector<Transaction>& dsTransactions) {
  LookupForwardTxnsFromSeed result;

  if (!shardTransactions.empty()) {
    TransactionArrayToProtobuf(shardTransactions,
                               *result.mutable_shardtransactions());
  }
  if (!dsTransactions.empty()) {
    TransactionArrayToProtobuf(dsTransactions,
                               *result.mutable_dstransactions());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupForwardTxnsFromSeed initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetForwardTxnBlockFromSeed(
    const bytes& src, const unsigned int offset,
    vector<Transaction>& shardTransactions,
    vector<Transaction>& dsTransactions) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupForwardTxnsFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupForwardTxnsFromSeed initialization failed");
    return false;
  }

  if (!ProtobufToTransactionArray(result.shardtransactions(),
                                  shardTransactions)) {
    LOG_GENERAL(WARNING, "ProtobufToTransactionArray failed");
    return false;
  }

  return ProtobufToTransactionArray(result.dstransactions(), dsTransactions);
}

// UNUSED
bool Messenger::SetLookupGetShardsFromSeed(bytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LOG_MARKER();

  LookupGetShardsFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetShardsFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

// UNUSED
bool Messenger::GetLookupGetShardsFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetShardsFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetShardsFromSeed initialization failed");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

// UNUSED
bool Messenger::SetLookupSetShardsFromSeed(
    bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
    const uint32_t& shardingStructureVersion, const DequeOfShard& shards) {
  LOG_MARKER();

  LookupSetShardsFromSeed result;

  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_sharding());

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  bytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure");
    return false;
  }

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign sharding structure");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetShardsFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetShardsFromSeed(const bytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           uint32_t& shardingStructureVersion,
                                           DequeOfShard& shards) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetShardsFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetShardsFromSeed initialization failed");
    return false;
  }

  if (!ProtobufToShardingStructure(result.sharding(), shardingStructureVersion,
                                   shards)) {
    LOG_GENERAL(WARNING, "ProtobufToShardingStructure failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  bytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure");
    return false;
  }

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in sharding structure");
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
    LOG_GENERAL(WARNING, "LookupGetMicroBlockFromLookup initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

// UNUSED
bool Messenger::GetLookupGetMicroBlockFromLookup(
    const bytes& src, const unsigned int offset,
    vector<BlockHash>& microBlockHashes, uint32_t& portNo) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetMicroBlockFromLookup result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetMicroBlockFromLookup initialization failed");
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
    bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
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
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign micro blocks");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetMicroBlockFromLookup result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetMicroBlockFromLookup initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (result.microblocks().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.microblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in micro blocks");
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

// UNUSED
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

// UNUSED
bool Messenger::GetLookupGetTxnsFromLookup(const bytes& src,
                                           const unsigned int offset,
                                           vector<TxnHash>& txnhashes,
                                           uint32_t& portNo) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

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

// UNUSED
bool Messenger::SetLookupSetTxnsFromLookup(
    bytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
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
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }

    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign transactions");
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

// UNUSED
bool Messenger::GetLookupSetTxnsFromLookup(
    const bytes& src, const unsigned int offset, PubKey& lookupPubKey,
    vector<TransactionWithReceipt>& txns) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetTxnsFromLookup result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxnsFromLookup initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (result.transactions().size() > 0) {
    bytes tmp;
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }

    if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions");
      return false;
    }
  }

  for (auto const& protoTxn : result.transactions()) {
    TransactionWithReceipt txn;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(protoTxn, txn);
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
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

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
    const uint32_t& shardingStructureVersion,
    const vector<
        boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
        directoryBlocks,
    const uint64_t& indexNum, const PairOfKey& lookupKey) {
  LookupSetDirectoryBlocksFromSeed result;

  result.mutable_data()->set_indexnum(indexNum);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  for (const auto& dirblock : directoryBlocks) {
    ProtoSingleDirectoryBlock* proto_dir_blocks =
        result.mutable_data()->add_dirblocks();
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
          shardingStructureVersion,
          get<FallbackBlockWShardingStructure>(dirblock).m_shards,
          *proto_dir_blocks->mutable_fallbackblockwshard()->mutable_sharding());
    }
  }

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign set LookupSetDirectoryBlocksFromSeed message");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed initialization failed");
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDirectoryBlocksFromSeed(
    const bytes& src, const unsigned int offset,
    uint32_t& shardingStructureVersion,
    vector<boost::variant<DSBlock, VCBlock, FallbackBlockWShardingStructure>>&
        directoryBlocks,
    uint64_t& indexNum, PubKey& pubKey) {
  LookupSetDirectoryBlocksFromSeed result;

  google::protobuf::io::ArrayInputStream arrayIn(src.data() + offset,
                                                 src.size() - offset);
  google::protobuf::io::CodedInputStream codedIn(&arrayIn);
  codedIn.SetTotalBytesLimit(MAX_READ_WATERMARK_IN_BYTES,
                             MAX_READ_WATERMARK_IN_BYTES);

  if (!result.ParseFromCodedStream(&codedIn) ||
      !codedIn.ConsumedEntireMessage() || !result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, pubKey)) {
    LOG_GENERAL(WARNING,
                "Invalid signature in LookupSetDirectoryBlocksFromSeed");
    return false;
  }

  indexNum = result.data().indexnum();

  for (const auto& dirblock : result.data().dirblocks()) {
    DSBlock dsblock;
    VCBlock vcblock;
    FallbackBlockWShardingStructure fallbackblockwshard;
    switch (dirblock.directoryblock_case()) {
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kDsblock:
        if (!dirblock.dsblock().IsInitialized()) {
          LOG_GENERAL(WARNING, "DS block not initialized");
          return false;
        }
        if (!ProtobufToDSBlock(dirblock.dsblock(), dsblock)) {
          LOG_GENERAL(WARNING, "ProtobufToDSBlock failed");
          return false;
        }
        directoryBlocks.emplace_back(dsblock);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kVcblock:
        if (!dirblock.vcblock().IsInitialized()) {
          LOG_GENERAL(WARNING, "VC block not initialized");
          return false;
        }
        if (!ProtobufToVCBlock(dirblock.vcblock(), vcblock)) {
          LOG_GENERAL(WARNING, "ProtobufToVCBlock failed");
          return false;
        }
        directoryBlocks.emplace_back(vcblock);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kFallbackblockwshard:
        if (!dirblock.fallbackblockwshard().IsInitialized()) {
          LOG_GENERAL(WARNING, "FallbackBlock not initialized");
          return false;
        }
        if (!ProtobufToFallbackBlock(
                dirblock.fallbackblockwshard().fallbackblock(),
                fallbackblockwshard.m_fallbackblock)) {
          LOG_GENERAL(WARNING, "ProtobufToFallbackBlock failed");
          return false;
        }
        if (!ProtobufToShardingStructure(
                dirblock.fallbackblockwshard().sharding(),
                shardingStructureVersion, fallbackblockwshard.m_shards)) {
          LOG_GENERAL(WARNING, "ProtobufToShardingStructure failed");
          return false;
        }
        directoryBlocks.emplace_back(fallbackblockwshard);
        break;
      case ProtoSingleDirectoryBlock::DirectoryblockCase::
          DIRECTORYBLOCK_NOT_SET:
      default:
        LOG_GENERAL(WARNING, "Error in the blocktype");
        return false;
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
    const PairOfKey& backupKey) {
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
    LOG_GENERAL(WARNING, "ConsensusCommit.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit");
    return false;
  }

  SerializableToProtobufByteArray(backupKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCommit(const bytes& src, const unsigned int offset,
                                   const uint32_t consensusID,
                                   const uint64_t blockNumber,
                                   const bytes& blockHash, uint16_t& backupID,
                                   CommitPoint& commitPoint,
                                   CommitPointHash& commitPointHash,
                                   const DequeOfNode& committeeKeys) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusCommit result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
    return false;
  }

  backupID = result.consensusinfo().backupid();

  if (backupID >= committeeKeys.size()) {
    LOG_GENERAL(WARNING, "Backup ID beyond shard size. Backup ID: "
                             << backupID
                             << " Shard size: " << committeeKeys.size());
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.consensusinfo().commitpoint(),
                                  commitPoint);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.consensusinfo().commitpointhash(),
                                  commitPointHash);

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusChallenge(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const vector<ChallengeSubsetInfo>& subsetInfo, const PairOfKey& leaderKey) {
  LOG_MARKER();

  ConsensusChallenge result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_leaderid(leaderID);

  for (const auto& subset : subsetInfo) {
    ConsensusChallenge::SubsetInfo* si =
        result.mutable_consensusinfo()->add_subsetinfo();

    SerializableToProtobufByteArray(subset.aggregatedCommit,
                                    *si->mutable_aggregatedcommit());
    SerializableToProtobufByteArray(subset.aggregatedKey,
                                    *si->mutable_aggregatedkey());
    SerializableToProtobufByteArray(subset.challenge, *si->mutable_challenge());
  }

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign challenge");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusChallenge(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    vector<ChallengeSubsetInfo>& subsetInfo, const PubKey& leaderKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusChallenge result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusChallenge initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }
    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
    return false;
  }

  if (result.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << result.consensusinfo().leaderid());
    return false;
  }

  for (const auto& proto_si : result.consensusinfo().subsetinfo()) {
    ChallengeSubsetInfo si;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_si.aggregatedcommit(),
                                    si.aggregatedCommit);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_si.aggregatedkey(), si.aggregatedKey);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_si.challenge(), si.challenge);

    subsetInfo.emplace_back(si);
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in challenge");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusResponse(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t backupID,
    const vector<ResponseSubsetInfo>& subsetInfo, const PairOfKey& backupKey) {
  LOG_MARKER();

  ConsensusResponse result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_backupid(backupID);

  for (const auto& subset : subsetInfo) {
    ConsensusResponse::SubsetInfo* si =
        result.mutable_consensusinfo()->add_subsetinfo();
    SerializableToProtobufByteArray(subset.response, *si->mutable_response());
  }

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign response");
    return false;
  }

  SerializableToProtobufByteArray(backupKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusResponse(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
    vector<ResponseSubsetInfo>& subsetInfo, const DequeOfNode& committeeKeys) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusResponse result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusResponse initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
    return false;
  }

  backupID = result.consensusinfo().backupid();

  if (backupID >= committeeKeys.size()) {
    LOG_GENERAL(WARNING, "Backup ID beyond shard size. Backup ID: "
                             << backupID
                             << " Shard size: " << committeeKeys.size());
    return false;
  }

  for (const auto& proto_si : result.consensusinfo().subsetinfo()) {
    ResponseSubsetInfo si;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_si.response(), si.response);

    subsetInfo.emplace_back(si);
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in response");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusCollectiveSig(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const Signature& collectiveSig, const vector<bool>& bitmap,
    const PairOfKey& leaderKey) {
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
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign collectivesig");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCollectiveSig(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    vector<bool>& bitmap, Signature& collectiveSig, const PubKey& leaderKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusCollectiveSig result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
    return false;
  }

  if (result.consensusinfo().leaderid() != leaderID) {
    LOG_GENERAL(WARNING, "Leader ID mismatch. Expected: "
                             << leaderID << " Actual: "
                             << result.consensusinfo().leaderid());
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.consensusinfo().collectivesig(),
                                  collectiveSig);

  for (const auto& i : result.consensusinfo().bitmap()) {
    bitmap.emplace_back(i);
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in collectivesig");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusCommitFailure(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t backupID,
    const bytes& errorMsg, const PairOfKey& backupKey) {
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
    LOG_GENERAL(WARNING, "ConsensusCommitFailure.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, backupKey.first, backupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit failure");
    return false;
  }

  SerializableToProtobufByteArray(backupKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommitFailure initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCommitFailure(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& backupID,
    bytes& errorMsg, const DequeOfNode& committeeKeys) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusCommitFailure result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommitFailure initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
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

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature,
                                     committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit failure");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusConsensusFailure(
    bytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, const uint16_t leaderID,
    const PairOfKey& leaderKey) {
  LOG_MARKER();

  ConsensusConsensusFailure result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_leaderid(leaderID);

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "ConsensusConsensusFailure.Data initialization failed");
    return false;
  }

  bytes tmp(result.consensusinfo().ByteSize());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::GetInstance().Sign(tmp, leaderKey.first, leaderKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign ConsensusConsensusFailure.Data");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusConsensusFailure initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusConsensusFailure(
    const bytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const bytes& blockHash, uint16_t& leaderID,
    const PubKey& leaderKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ConsensusConsensusFailure result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusConsensusFailure initialization failed");
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

    std::string blockhashStr, remoteblockhashStr;
    if (!DataConversion::Uint8VecToHexStr(blockHash, blockhashStr)) {
      return false;
    }

    if (!DataConversion::Uint8VecToHexStr(remoteBlockHash,
                                          remoteblockhashStr)) {
      return false;
    }

    LOG_GENERAL(WARNING, "Block hash mismatch. Expected: "
                             << blockhashStr
                             << " Actual: " << remoteblockhashStr);
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

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in ConsensusConsensusFailure");
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
    LOG_GENERAL(WARNING, "LookupGetDSTxBlockFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSTxBlockFromSeed(
    const bytes& src, const unsigned int offset, uint64_t& dsLowBlockNum,
    uint64_t& dsHighBlockNum, uint64_t& txLowBlockNum, uint64_t& txHighBlockNum,
    uint32_t& listenPort) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetDSTxBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSTxBlockFromSeed initialization failed");
    return false;
  }

  dsLowBlockNum = result.dslowblocknum();
  dsHighBlockNum = result.dshighblocknum();
  txLowBlockNum = result.txlowblocknum();
  txHighBlockNum = result.txhighblocknum();
  listenPort = result.listenport();

  return true;
}

bool Messenger::SetVCNodeSetDSTxBlockFromSeed(bytes& dst,
                                              const unsigned int offset,
                                              const PairOfKey& lookupKey,
                                              const vector<DSBlock>& DSBlocks,
                                              const vector<TxBlock>& txBlocks) {
  LOG_MARKER();

  VCNodeSetDSTxBlockFromSeed result;

  for (const auto& dsblock : DSBlocks) {
    DSBlockToProtobuf(dsblock, *result.mutable_data()->add_dsblocks());
  }

  for (const auto& txblock : txBlocks) {
    TxBlockToProtobuf(txblock, *result.mutable_data()->add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "VCNodeSetDSTxBlockFromSeed.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign DS and Tx blocks");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "VCNodeSetDSTxBlockFromSeed initialization failed");
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

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  VCNodeSetDSTxBlockFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "VCNodeSetDSTxBlockFromSeed initialization failed");
    return false;
  }

  for (const auto& proto_dsblock : result.data().dsblocks()) {
    DSBlock dsblock;
    if (!ProtobufToDSBlock(proto_dsblock, dsblock)) {
      LOG_GENERAL(WARNING, "ProtobufToDSBlock failed");
      return false;
    }
    dsBlocks.emplace_back(dsblock);
  }

  for (const auto& txblock : result.data().txblocks()) {
    TxBlock block;
    if (!ProtobufToTxBlock(txblock, block)) {
      LOG_GENERAL(WARNING, "ProtobufToTxBlock failed");
      return false;
    }
    txBlocks.emplace_back(block);
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::GetInstance().Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in VCNodeSetDSTxBlockFromSeed");
    return false;
  }

  return true;
}

bool Messenger::SetDSLookupNewDSGuardNetworkInfo(
    bytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
    const Peer& dsGuardNewNetworkInfo, const uint64_t timestamp,
    const PairOfKey& dsguardkey) {
  LOG_MARKER();
  DSLookupSetDSGuardNetworkInfoUpdate result;

  result.mutable_data()->set_dsepochnumber(dsEpochNumber);
  SerializableToProtobufByteArray(
      dsguardkey.second, *result.mutable_data()->mutable_dsguardpubkey());
  PeerToProtobuf(dsGuardNewNetworkInfo,
                 *result.mutable_data()->mutable_dsguardnewnetworkinfo());
  result.mutable_data()->set_timestamp(timestamp);

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "DSLookupSetDSGuardNetworkInfoUpdate.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, dsguardkey.first, dsguardkey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign ds guard identity update");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "DSLookupSetDSGuardNetworkInfoUpdate initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetDSLookupNewDSGuardNetworkInfo(
    const bytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
    Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  DSLookupSetDSGuardNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "DSLookupSetDSGuardNetworkInfoUpdate initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().dsguardpubkey(), dsGuardPubkey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     dsGuardPubkey)) {
    LOG_GENERAL(WARNING, "DSLookupSetDSGuardNetworkInfoUpdate signature wrong");
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
    const uint64_t dsEpochNumber, const PairOfKey& lookupKey) {
  LOG_MARKER();

  NodeGetGuardNodeNetworkInfoUpdate result;
  result.mutable_data()->set_portno(portNo);
  result.mutable_data()->set_dsepochnumber(dsEpochNumber);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  if (result.data().IsInitialized()) {
    bytes tmp(result.data().ByteSize());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                     signature)) {
      LOG_GENERAL(WARNING, "Failed to sign ds guard identity update");
      return false;
    }
    SerializableToProtobufByteArray(signature, *result.mutable_signature());
  } else {
    LOG_GENERAL(
        WARNING,
        "SetLookupGetNewDSGuardNetworkInfoFromLookup initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetNewDSGuardNetworkInfoFromLookup(
    const bytes& src, const unsigned int offset, uint32_t& portNo,
    uint64_t& dsEpochNumber) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeGetGuardNodeNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(
        WARNING,
        "GetLookupGetNewDSGuardNetworkInfoFromLookup initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check

  // We don't return senderPubKey since timing issues may make it difficult for
  // the lookup to check it against the shard structure
  PubKey senderPubKey;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     senderPubKey)) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission signature wrong");
    return false;
  }

  portNo = result.data().portno();
  dsEpochNumber = result.data().dsepochnumber();

  return true;
}

bool Messenger::SetNodeSetNewDSGuardNetworkInfo(
    bytes& dst, unsigned int offset,
    const vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
    const PairOfKey& lookupKey) {
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

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetGuardNodeNetworkInfoUpdate.Data initialization failed");
    return false;
  }
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, lookupKey.first, lookupKey.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign ds guard identity update");
    return false;
  }
  SerializableToProtobufByteArray(lookupKey.second,
                                  *result.mutable_lookuppubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "SetNodeSetNewDSGuardNetworkInfo initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetNodeGetNewDSGuardNetworkInfo(
    const bytes& src, const unsigned int offset,
    vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
    PubKey& lookupPubKey) {
  LOG_MARKER();

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeSetGuardNodeNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetGuardNodeNetworkInfoUpdate initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.lookuppubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     lookupPubKey)) {
    LOG_GENERAL(WARNING, "NodeSetGuardNodeNetworkInfoUpdate signature wrong");
    return false;
  }

  for (const auto& proto_DSGuardUpdateStruct :
       result.data().dsguardupdatestruct()) {
    PubKey tempPubk;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_DSGuardUpdateStruct.dsguardpubkey(),
                                    tempPubk);
    Peer tempPeer;
    ProtobufToPeer(proto_DSGuardUpdateStruct.dsguardnewnetworkinfo(), tempPeer);
    uint64_t tempTimestamp = proto_DSGuardUpdateStruct.timestamp();
    vecOfDSGuardUpdateStruct.emplace_back(
        DSGuardUpdateStruct(tempPubk, tempPeer, tempTimestamp));
  }

  return true;
}

bool Messenger::SetSeedNodeHistoricalDB(bytes& dst, const unsigned int offset,
                                        const PairOfKey& archivalKeys,
                                        const uint32_t code,
                                        const string& path) {
  SeedSetHistoricalDB result;

  result.mutable_data()->set_code(code);
  result.mutable_data()->set_path(path);
  SerializableToProtobufByteArray(archivalKeys.second,
                                  *result.mutable_pubkey());

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB.Data initialization failed");
    return false;
  }

  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  Signature signature;
  if (!Schnorr::GetInstance().Sign(tmp, archivalKeys.first, archivalKeys.second,
                                   signature)) {
    LOG_GENERAL(WARNING, "Failed to sign SeedSetHistoricalDB");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetSeedNodeHistoricalDB(const bytes& src,
                                        const unsigned int offset,
                                        PubKey& archivalPubKey, uint32_t& code,
                                        string& path) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  SeedSetHistoricalDB result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB initialization failed ");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), archivalPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  bytes tmp(result.data().ByteSize());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::GetInstance().Verify(tmp, 0, tmp.size(), signature,
                                     archivalPubKey)) {
    LOG_GENERAL(WARNING, "SeedSetHistoricalDB signature wrong");
    return false;
  }
  code = result.data().code();
  path = result.data().path();

  return true;
}
