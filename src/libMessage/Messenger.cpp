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
#include "depends/common/FixedHash.h"
#include "libBlockchain/Serialization.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountStore/AccountStore.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libDirectoryService/DirectoryService.h"
#include "libUtils/SafeMath.h"

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

template bool SerializeToArray<ProtoAccountStore>(
    const ProtoAccountStore& protoMessage, zbytes& dst,
    const unsigned int offset);

template <class T>
bool RepeatableToArray(const T& repeatable, zbytes& dst,
                       const unsigned int offset) {
  int tempOffset = offset;
  for (const auto& element : repeatable) {
    if (!SerializeToArray(element, dst, tempOffset)) {
      LOG_GENERAL(WARNING, "SerializeToArray failed, offset: " << tempOffset);
      return false;
    }
    tempOffset += element.ByteSizeLong();
  }
  return true;
}

template <class T, size_t S>
void NumberToArray(const T& number, zbytes& dst, const unsigned int offset) {
  Serializable::SetNumber<T>(dst, offset, number, S);
}

// ============================================================================
// Functions to check for fields in primitives that are used for persistent
// storage. Remove fields from the checks once they are deprecated.
// ============================================================================

inline bool CheckRequiredFieldsProtoBlockLink(
    const ProtoBlockLink& protoBlockLink) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockLink.has_version() && protoBlockLink.has_index() &&
         protoBlockLink.has_dsindex() && protoBlockLink.has_blocktype() &&
         protoBlockLink.has_blockhash();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoDSNode(const ProtoDSNode& protoDSNode) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoDSNode.has_pubkey() && protoDSNode.has_peer();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoDSCommittee(
    const ProtoDSCommittee& protoDSCommittee) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member dsnodes
  return protoDSCommittee.has_version();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoShardingStructureMember(
    const ProtoShardingStructure::Member& protoMember) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoMember.has_pubkey() && protoMember.has_peerinfo() &&
         protoMember.has_reputation();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoShardingStructureShard(
    const ProtoShardingStructure::Shard& protoShard) {
  // Don't need to enforce check on repeated member members
  return true;
}

inline bool CheckRequiredFieldsProtoShardingStructure(
    const ProtoShardingStructure& protoShardingStructure) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated member shards
  return protoShardingStructure.has_version();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoBlockBaseCoSignatures(
    const ProtoBlockBase::CoSignatures& protoCoSignatures) {
// TODO: Check if default value is acceptable for each field
#if 0
  // Don't need to enforce check on repeated members b1 and b2
  return protoCoSignatures.has_cs1() && protoCoSignatures.has_cs2();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoBlockBase(
    const ProtoBlockBase& protoBlockBase) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockBase.has_blockhash() && protoBlockBase.has_cosigs() &&
         protoBlockBase.has_timestamp() &&
         CheckRequiredFieldsProtoBlockBaseCoSignatures(protoBlockBase.cosigs());
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoBlockHeaderBase(
    const ProtoBlockHeaderBase& protoBlockHeaderBase) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoBlockHeaderBase.has_version() &&
         protoBlockHeaderBase.has_committeehash() &&
         protoBlockHeaderBase.has_prevhash();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoAccountBase(
    const ProtoAccountBase& protoAccountBase) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoAccountBase.has_version() && protoAccountBase.has_balance() &&
         protoAccountBase.has_nonce();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoAccount(const ProtoAccount& protoAccount) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoAccount.has_base();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoStateData(
    const ProtoStateData& protoStateData) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoStateData.has_version() && protoStateData.has_vname() &&
         protoStateData.has_ismutable() && protoStateData.has_type() &&
         protoStateData.has_value();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoTransaction(
    const ProtoTransaction& protoTransaction) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoTransaction.has_tranid() && protoTransaction.has_info() &&
         protoTransaction.has_signature();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoTransactionCoreInfo(
    const ProtoTransactionCoreInfo& protoTxnCoreInfo) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoTxnCoreInfo.has_version() && protoTxnCoreInfo.has_nonce() &&
         protoTxnCoreInfo.has_toaddr() && protoTxnCoreInfo.has_senderpubkey() &&
         protoTxnCoreInfo.has_amount() && protoTxnCoreInfo.has_gasprice() &&
         protoTxnCoreInfo.has_gaslimit();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoTransactionReceipt(
    const ProtoTransactionReceipt& protoTransactionReceipt) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoTransactionReceipt.has_receipt() &&
         protoTransactionReceipt.has_cumgas();
#endif
  return true;
}

inline bool CheckRequiredFieldsProtoTransactionWithReceipt(
    const ProtoTransactionWithReceipt& protoTxnWReceipt) {
// TODO: Check if default value is acceptable for each field
#if 0
  return protoTxnWReceipt.has_transaction() && protoTxnWReceipt.has_receipt();
#endif
  return true;
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

  if (!protoAccountBase.codehash().empty()) {
    dev::h256 tmpCodeHash;
    if (!Messenger::CopyWithSizeCheck(protoAccountBase.codehash(),
                                      tmpCodeHash.asArray())) {
      return false;
    }
    accountBase.SetCodeHash(tmpCodeHash);
  }

  if (!protoAccountBase.storageroot().empty()) {
    dev::h256 tmpStorageRoot;
    if (!Messenger::CopyWithSizeCheck(protoAccountBase.storageroot(),
                                      tmpStorageRoot.asArray())) {
      return false;
    }
    accountBase.SetStorageRoot(tmpStorageRoot);
  }

  return true;
}

bool AccountToProtobuf(const Account& account, ProtoAccount& protoAccount) {
  ZilliqaMessage::ProtoAccountBase* protoAccountBase =
      protoAccount.mutable_base();

  AccountBaseToProtobuf(account, *protoAccountBase);

  if (!protoAccountBase->codehash().empty()) {
    zbytes codebytes = account.GetCode();
    protoAccount.set_code(codebytes.data(), codebytes.size());

    // set initdata
    zbytes initbytes = account.GetInitData();
    protoAccount.set_initdata(initbytes.data(), initbytes.size());

    // set data
    map<std::string, zbytes> t_states;
    set<std::string> deletedIndices;
    if (!account.GetUpdatedStates(t_states, deletedIndices, false)) {
      LOG_GENERAL(WARNING, "Account::GetUpdatedStates failed");
      return false;
    }
    for (const auto& state : t_states) {
      ProtoAccount::StorageData2* entry = protoAccount.add_storage2();
      entry->set_key(state.first);
      entry->set_data(state.second.data(), state.second.size());
    }
    for (const auto& todelete : deletedIndices) {
      protoAccount.add_todelete(todelete);
    }
  }
  return true;
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

    if (protoAccount.code().empty()) {
      LOG_GENERAL(WARNING, "Account has valid codehash but no code content");
      return false;
    }
    zbytes codeBytes, initBytes;
    codeBytes.resize(protoAccount.code().size());
    copy(protoAccount.code().begin(), protoAccount.code().end(),
         codeBytes.begin());
    initBytes.resize(protoAccount.initdata().size());
    copy(protoAccount.initdata().begin(), protoAccount.initdata().end(),
         initBytes.begin());

    account.SetImmutable(codeBytes, initBytes);

    if (account.GetCodeHash() != tmpCodeHash) {
      LOG_GENERAL(WARNING, "Code hash mismatch. Expected: "
                               << account.GetCodeHash().hex()
                               << " Actual: " << tmpCodeHash.hex());
      return false;
    }

    dev::h256 tmpStorageRoot = account.GetStorageRoot();

    map<string, zbytes> t_states;
    vector<std::string> toDeleteIndices;

    for (const auto& entry : protoAccount.storage2()) {
      t_states.emplace(entry.key(),
                       DataConversion::StringToCharArray(entry.data()));
    }

    for (const auto& entry : protoAccount.todelete()) {
      toDeleteIndices.emplace_back(entry);
    }

    if (!account.UpdateStates(addr, t_states, toDeleteIndices, false)) {
      LOG_GENERAL(WARNING, "Account::UpdateStates failed");
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

bool AccountDeltaToProtobuf(const Account* oldAccount,
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
    return false;
  }
  accbase.SetNonce(nonceDelta);

  if (newAccount.isContract() || newAccount.IsLibrary()) {
    if (fullCopy) {
      accbase.SetCodeHash(newAccount.GetCodeHash());
      protoAccount.set_code(newAccount.GetCode().data(),
                            newAccount.GetCode().size());
      protoAccount.set_initdata(newAccount.GetInitData().data(),
                                newAccount.GetInitData().size());
    }

    if (fullCopy ||
        newAccount.GetStorageRoot() != oldAccount->GetStorageRoot()) {
      accbase.SetStorageRoot(newAccount.GetStorageRoot());

      map<std::string, zbytes> t_states;
      set<std::string> deletedIndices;
      if (!newAccount.GetUpdatedStates(t_states, deletedIndices, true)) {
        return false;
      }
      LOG_GENERAL(INFO, "t_states size: " << t_states.size());
      for (const auto& state : t_states) {
        ProtoAccount::StorageData2* entry = protoAccount.add_storage2();
        entry->set_key(state.first);
        entry->set_data(state.second.data(), state.second.size());
      }
      for (const auto& deleted : deletedIndices) {
        protoAccount.add_todelete(deleted);
      }
    }
  }

  ZilliqaMessage::ProtoAccountBase* protoAccountBase =
      protoAccount.mutable_base();

  AccountBaseToProtobuf(accbase, *protoAccountBase);

  return true;
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

  if (accbase.GetVersion() != Account::VERSION) {
    LOG_GENERAL(WARNING, "Account delta version doesn't match, expected "
                             << Account::VERSION << " received "
                             << accbase.GetVersion());
    return false;
  }

#if 0
  if (!protoAccount.has_numbersign()) {
    LOG_GENERAL(WARNING, "numbersign is not found in ProtoAccount for Delta");
    return false;
  }
#endif

  int256_t balanceDelta = protoAccount.numbersign()
                              ? accbase.GetBalance().convert_to<int256_t>()
                              : 0 - accbase.GetBalance().convert_to<int256_t>();
  account.ChangeBalance(balanceDelta);

  if (!account.IncreaseNonceBy(accbase.GetNonce())) {
    LOG_GENERAL(WARNING, "IncreaseNonceBy failed");
    return false;
  }

  if ((protoAccount.code().size() > 0) || account.isContract()) {
    if (fullCopy) {
      zbytes codeBytes, initDataBytes;

      if (protoAccount.code().size() > MAX_CODE_SIZE_IN_BYTES) {
        LOG_GENERAL(WARNING, "Code size "
                                 << protoAccount.code().size()
                                 << " greater than MAX_CODE_SIZE_IN_BYTES "
                                 << MAX_CODE_SIZE_IN_BYTES);
        return false;
      }
      codeBytes.resize(protoAccount.code().size());
      copy(protoAccount.code().begin(), protoAccount.code().end(),
           codeBytes.begin());
      initDataBytes.resize(protoAccount.initdata().size());
      copy(protoAccount.initdata().begin(), protoAccount.initdata().end(),
           initDataBytes.begin());
      if (codeBytes != account.GetCode() ||
          initDataBytes != account.GetInitData()) {
        if (!account.SetImmutable(codeBytes, initDataBytes)) {
          LOG_GENERAL(WARNING, "Account::SetImmutable failed");
          return false;
        }
      }

      if (account.GetCodeHash() != accbase.GetCodeHash()) {
        LOG_GENERAL(WARNING, "Code hash mismatch. Expected: "
                                 << account.GetCodeHash().hex()
                                 << " Actual: " << accbase.GetCodeHash().hex());
        return false;
      }
    }

    if (LOG_SC) {
      LOG_GENERAL(INFO, "Storage Root: " << accbase.GetStorageRoot());
      LOG_GENERAL(INFO, "Address: " << addr.hex());
    }

    if (accbase.GetStorageRoot() == dev::h256()) {
      dev::h256 tmpHash;

      map<string, zbytes> t_states;
      vector<std::string> toDeleteIndices;

      for (const auto& entry : protoAccount.storage2()) {
        t_states.emplace(entry.key(),
                         DataConversion::StringToCharArray(entry.data()));
        if (LOG_SC) {
          LOG_GENERAL(INFO, "Key: " << entry.key() << "  "
                                    << "Data: "
                                    << DataConversion::Uint8VecToHexStrRet(
                                           toZbytes(entry.data())));
        }
      }

      for (const auto& entry : protoAccount.todelete()) {
        toDeleteIndices.emplace_back(entry);
      }

      if (!account.UpdateStates(addr, t_states, toDeleteIndices, temp,
                                revertible)) {
        LOG_GENERAL(WARNING, "Account::UpdateStates failed");
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

void DSCommitteeToProtoCommittee(const DequeOfNode& dsCommittee,
                                 ProtoCommittee& protoCommittee) {
  for (const auto& node : dsCommittee) {
    SerializableToProtobufByteArray(node.first, *protoCommittee.add_members());
  }
}

void ShardToProtoCommittee(const DequeOfShardMembers& shard,
                           ProtoCommittee& protoCommittee) {
  for (const auto& node : shard) {
    SerializableToProtobufByteArray(std::get<SHARD_NODE_PUBKEY>(node),
                                    *protoCommittee.add_members());
  }
}

void ShardingStructureToProtobuf(
    const uint32_t& version, const DequeOfShardMembers& shards,
    ProtoShardingStructure& protoShardingStructure) {
  protoShardingStructure.set_version(version);
  ProtoShardingStructure::Shard* proto_shard =
      protoShardingStructure.add_shards();

  for (const auto& node : shards) {
    ProtoShardingStructure::Member* proto_member = proto_shard->add_members();

    SerializableToProtobufByteArray(std::get<SHARD_NODE_PUBKEY>(node),
                                    *proto_member->mutable_pubkey());
    SerializableToProtobufByteArray(std::get<SHARD_NODE_PEER>(node),
                                    *proto_member->mutable_peerinfo());
    proto_member->set_reputation(std::get<SHARD_NODE_REPUTATION>(node));
  }
}

bool ProtobufToShardingStructure(
    const ProtoShardingStructure& protoShardingStructure, uint32_t& version,
    DequeOfShardMembers& shardMembers) {
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

      shardMembers.emplace_back(key, peer, proto_member.reputation());
    }
  }

  return true;
}

void AnnouncementShardingStructureToProtobuf(
    const DequeOfShardMembers& shardMembers, const MapOfPubKeyPoW& allPoWs,
    ProtoShardingStructureWithPoWSolns& protoShardingStructure) {
  ProtoShardingStructureWithPoWSolns::Shard* proto_shard =
      protoShardingStructure.add_shards();
  for (const auto& node : shardMembers) {
    ProtoShardingStructureWithPoWSolns::Member* proto_member =
        proto_shard->add_members();

    const PubKey& key = std::get<SHARD_NODE_PUBKEY>(node);

    SerializableToProtobufByteArray(key, *proto_member->mutable_pubkey());
    SerializableToProtobufByteArray(std::get<SHARD_NODE_PEER>(node),
                                    *proto_member->mutable_peerinfo());
    proto_member->set_reputation(std::get<SHARD_NODE_REPUTATION>(node));

    ProtoPoWSolution* proto_soln = proto_member->mutable_powsoln();
    const auto soln = allPoWs.find(key);
    proto_soln->set_nonce(soln->second.m_nonce);
    proto_soln->set_result(soln->second.m_result.data(),
                           soln->second.m_result.size());
    proto_soln->set_mixhash(soln->second.m_mixhash.data(),
                            soln->second.m_mixhash.size());
    proto_soln->set_lookupid(soln->second.m_lookupId);
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        soln->second.m_gasPrice, *proto_soln->mutable_gasprice());
    if (proto_soln->govdata().IsInitialized()) {
      proto_soln->mutable_govdata()->set_proposalid(
          soln->second.m_govProposal.first);
      proto_soln->mutable_govdata()->set_votevalue(
          soln->second.m_govProposal.second);
    }
    proto_soln->set_extradata(soln->second.m_extraData.data(),
                              soln->second.m_extraData.size());
  }
}

bool ProtobufToShardingStructureAnnouncement(
    const ProtoShardingStructureWithPoWSolns& protoShardingStructure,
    DequeOfShardMembers& shardMembers, MapOfPubKeyPoW& allPoWs) {
  std::array<unsigned char, 32> result{};
  std::array<unsigned char, 32> mixhash{};
  uint128_t gasPrice;
  uint32_t govProposalId{};
  uint32_t govVoteValue{};
  zbytes extraData;
  for (const auto& proto_shard : protoShardingStructure.shards()) {
    for (const auto& proto_member : proto_shard.members()) {
      PubKey key;
      Peer peer;

      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.pubkey(), key);
      PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_member.peerinfo(), peer);

      shardMembers.emplace_back(key, peer, proto_member.reputation());

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
      if (proto_member.powsoln().govdata().IsInitialized()) {
        govProposalId = proto_member.powsoln().govdata().proposalid();
        govVoteValue = proto_member.powsoln().govdata().votevalue();
      }
      if (proto_member.powsoln().extradata().size() > 32) {
        LOG_GENERAL(WARNING, "extra data is too large");
        return false;
      }
      zbytes extraData(proto_member.powsoln().extradata().begin(),
                       proto_member.powsoln().extradata().end());
      allPoWs.emplace(
          key,
          PoWSolution(proto_member.powsoln().nonce(), result, mixhash,
                      proto_member.powsoln().lookupid(), gasPrice,
                      std::make_pair(govProposalId, govVoteValue), extraData));
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
  for (const auto& item : txnCoreInfo.accessList) {
    AccessListItem* accessListItem = protoTxnCoreInfo.add_accesslist();
    accessListItem->set_address(item.first.data(), item.first.size);
    for (const auto& storageKey : item.second) {
      accessListItem->add_storagekeys(storageKey.data(), storageKey.size);
    }
  }
  if (txnCoreInfo.maxPriorityFeePerGas != 0) {
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        txnCoreInfo.maxPriorityFeePerGas,
        *protoTxnCoreInfo.mutable_maxpriorityfeepergas());
  }
  if (txnCoreInfo.maxFeePerGas != 0) {
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        txnCoreInfo.maxFeePerGas, *protoTxnCoreInfo.mutable_maxfeepergas());
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
  if (protoTxnCoreInfo.code().size() > 0) {
    txnCoreInfo.code.resize(protoTxnCoreInfo.code().size());
    copy(protoTxnCoreInfo.code().begin(), protoTxnCoreInfo.code().end(),
         txnCoreInfo.code.begin());
  }
  if (protoTxnCoreInfo.data().size() > 0) {
    txnCoreInfo.data.resize(protoTxnCoreInfo.data().size());
    copy(protoTxnCoreInfo.data().begin(), protoTxnCoreInfo.data().end(),
         txnCoreInfo.data.begin());
  }
  txnCoreInfo.accessList.reserve(protoTxnCoreInfo.accesslist_size());
  for (const auto& item : protoTxnCoreInfo.accesslist()) {
    dev::h160 address(item.address(),
                      dev::h160::ConstructFromStringType::FromBinary);
    std::vector<dev::h256> storageKeys;
    storageKeys.reserve(item.storagekeys_size());
    for (const auto& key : item.storagekeys()) {
      dev::h256 storageKey(key, dev::h256::ConstructFromStringType::FromBinary);
      storageKeys.push_back(storageKey);
    }
    auto accessListItem = std::make_pair(address, storageKeys);
    txnCoreInfo.accessList.push_back(std::move(accessListItem));
  }
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxnCoreInfo.maxpriorityfeepergas(),
      txnCoreInfo.maxPriorityFeePerGas);
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      protoTxnCoreInfo.maxfeepergas(), txnCoreInfo.maxFeePerGas);
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

  zbytes txnData;
  if (!SerializeToArray(protoTransaction.info(), txnData, 0)) {
    LOG_GENERAL(WARNING, "Serialize protoTransaction core info failed");
    return false;
  }

  transaction =
      Transaction(txnCoreInfo.version, txnCoreInfo.nonce, txnCoreInfo.toAddr,
                  txnCoreInfo.senderPubKey, txnCoreInfo.amount,
                  txnCoreInfo.gasPrice, txnCoreInfo.gasLimit, txnCoreInfo.code,
                  txnCoreInfo.data, signature, txnCoreInfo.accessList,
                  txnCoreInfo.maxPriorityFeePerGas, txnCoreInfo.maxFeePerGas);

  if (transaction.GetTranID() != tranID) {
    LOG_GENERAL(WARNING, "TranID verification failed. Expected: "
                             << tranID
                             << " Actual: " << transaction.GetTranID());
    return false;
  }

  if (!transaction.IsSigned(txnData)) {
    LOG_GENERAL(WARNING,
                "Signature verification failed when converting tx to protobuf");
    return false;
  }

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

void TransactionArrayToProtobuf(const vector<Transaction>& txns,
                                ProtoTransactionArray& protoTransactionArray) {
  for (const auto& txn : txns) {
    TransactionToProtobuf(txn, *protoTransactionArray.add_transactions());
  }
}

void TransactionArrayToProtobuf(const deque<pair<Transaction, uint32_t>>& txns,
                                ProtoTransactionArray& protoTransactionArray) {
  for (const auto& txn_and_count : txns) {
    TransactionToProtobuf(txn_and_count.first,
                          *protoTransactionArray.add_transactions());
  }
}

bool ProtobufToTransactionArray(
    const ProtoTransactionArray& protoTransactionArray,
    std::vector<Transaction>& txns) {
  txns.reserve(protoTransactionArray.transactions_size());
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
  dsPowSubmission.mutable_data()->set_extradata(
      powSolution.GetExtraData().data(), powSolution.GetExtraData().size());
  dsPowSubmission.mutable_data()->set_lookupid(powSolution.GetLookupId());
  if (dsPowSubmission.mutable_data()->govdata().IsInitialized()) {
    dsPowSubmission.mutable_data()->mutable_govdata()->set_proposalid(
        powSolution.GetGovProposalId());
    dsPowSubmission.mutable_data()->mutable_govdata()->set_votevalue(
        powSolution.GetGovVoteValue());
  }

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
  if (dsPowSubmission.data().extradata().size() > 32) {
    LOG_GENERAL(WARNING, "extra data is too large");
    return false;
  }
  zbytes extraData(dsPowSubmission.data().extradata().begin(),
                   dsPowSubmission.data().extradata().end());
  const uint32_t& lookupId = dsPowSubmission.data().lookupid();
  uint128_t gasPrice;
  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
      dsPowSubmission.data().gasprice(), gasPrice);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(dsPowSubmission.signature(), signature);

  const uint32_t& govProposalId = dsPowSubmission.data().govdata().proposalid();
  const uint32_t& govVoteValue = dsPowSubmission.data().govdata().votevalue();

  DSPowSolution result(blockNumber, difficultyLevel, submitterPeer,
                       submitterKey, nonce, resultingHash, mixHash, extraData,
                       lookupId, gasPrice,
                       std::make_pair(govProposalId, govVoteValue), signature);
  powSolution = result;

  return true;
}

bool SetConsensusAnnouncementCore(
    ZilliqaMessage::ConsensusAnnouncement& announcement,
    const uint32_t consensusID, uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey) {
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

  zbytes tmp(announcement.consensusinfo().ByteSizeLong());
  announcement.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, leaderKey.first, leaderKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign commit");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second,
                                  *announcement.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *announcement.mutable_signature());

  // Sign the announcement

  zbytes inputToSigning;

  switch (announcement.announcement_case()) {
    case ConsensusAnnouncement::AnnouncementCase::kDsblock:
      if (!announcement.dsblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement dsblock content not initialized");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSizeLong() +
                            announcement.dsblock().ByteSizeLong());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSizeLong());
      announcement.dsblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSizeLong(),
          announcement.dsblock().ByteSizeLong());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kMicroblock:
      if (!announcement.microblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement microblock content not initialized");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSizeLong() +
                            announcement.microblock().ByteSizeLong());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSizeLong());
      announcement.microblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSizeLong(),
          announcement.microblock().ByteSizeLong());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kFinalblock:
      if (!announcement.finalblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement finalblock content not initialized");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSizeLong() +
                            announcement.finalblock().ByteSizeLong());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSizeLong());
      announcement.finalblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSizeLong(),
          announcement.finalblock().ByteSizeLong());
      break;
    case ConsensusAnnouncement::AnnouncementCase::kVcblock:
      if (!announcement.vcblock().IsInitialized()) {
        LOG_GENERAL(WARNING, "Announcement vcblock content not initialized");
        return false;
      }
      inputToSigning.resize(announcement.consensusinfo().ByteSizeLong() +
                            announcement.vcblock().ByteSizeLong());
      announcement.consensusinfo().SerializeToArray(
          inputToSigning.data(), announcement.consensusinfo().ByteSizeLong());
      announcement.vcblock().SerializeToArray(
          inputToSigning.data() + announcement.consensusinfo().ByteSizeLong(),
          announcement.vcblock().ByteSizeLong());
      break;
    case ConsensusAnnouncement::AnnouncementCase::ANNOUNCEMENT_NOT_SET:
    default:
      LOG_GENERAL(WARNING, "Announcement content not set");
      return false;
  }

  Signature finalsignature;
  if (!Schnorr::Sign(inputToSigning, leaderKey.first, leaderKey.second,
                     finalsignature)) {
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
    const zbytes& blockHash, const uint16_t leaderID, const PubKey& leaderKey) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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
  zbytes tmp;

  if (announcement.has_dsblock() && announcement.dsblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSizeLong() +
               announcement.dsblock().ByteSizeLong());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSizeLong());
    announcement.dsblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSizeLong(),
        announcement.dsblock().ByteSizeLong());
  } else if (announcement.has_microblock() &&
             announcement.microblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSizeLong() +
               announcement.microblock().ByteSizeLong());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSizeLong());
    announcement.microblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSizeLong(),
        announcement.microblock().ByteSizeLong());
  } else if (announcement.has_finalblock() &&
             announcement.finalblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSizeLong() +
               announcement.finalblock().ByteSizeLong());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSizeLong());
    announcement.finalblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSizeLong(),
        announcement.finalblock().ByteSizeLong());
  } else if (announcement.has_vcblock() &&
             announcement.vcblock().IsInitialized()) {
    tmp.resize(announcement.consensusinfo().ByteSizeLong() +
               announcement.vcblock().ByteSizeLong());
    announcement.consensusinfo().SerializeToArray(
        tmp.data(), announcement.consensusinfo().ByteSizeLong());
    announcement.vcblock().SerializeToArray(
        tmp.data() + announcement.consensusinfo().ByteSizeLong(),
        announcement.vcblock().ByteSizeLong());
  } else {
    LOG_GENERAL(WARNING, "Announcement content not set");
    return false;
  }

  Signature finalsignature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(announcement.finalsignature(),
                                  finalsignature);

  if (!Schnorr::Verify(tmp, finalsignature, leaderKey)) {
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

  zbytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed");
    return false;
  }

  SHA256Calculator sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::GetShardHash(const DequeOfShardMembers& shardMembers,
                             CommitteeHash& dst) {
  ProtoCommittee protoCommittee;

  ShardToProtoCommittee(shardMembers, protoCommittee);

  if (!protoCommittee.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoCommittee initialization failed");
    return false;
  }

  zbytes tmp;

  if (!SerializeToArray(protoCommittee, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoCommittee serialization failed");
    return false;
  }

  SHA256Calculator sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::GetShardingStructureHash(const uint32_t& version,
                                         const DequeOfShardMembers& shards,
                                         ShardingHash& dst) {
  ProtoShardingStructure protoShardingStructure;

  ShardingStructureToProtobuf(version, shards, protoShardingStructure);

  if (!protoShardingStructure.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure initialization failed");
    return false;
  }

  zbytes tmp;

  if (!SerializeToArray(protoShardingStructure, tmp, 0)) {
    LOG_GENERAL(WARNING, "ProtoShardingStructure serialization failed");
    return false;
  }

  SHA256Calculator sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::SetAccountBase(zbytes& dst, const unsigned int offset,
                               const AccountBase& accountbase) {
  ProtoAccountBase result;

  AccountBaseToProtobuf(accountbase, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountBase initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetAccountBase(const zbytes& src, const unsigned int offset,
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

bool Messenger::GetAccountBase(const string& src, const unsigned int offset,
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

bool Messenger::SetAccount(zbytes& dst, const unsigned int offset,
                           const Account& account) {
  ProtoAccount result;

  if (!AccountToProtobuf(account, result)) {
    LOG_GENERAL(WARNING, "AccountToProtobuf failed");
    return false;
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetAccount(const zbytes& src, const unsigned int offset,
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

bool Messenger::SetAccountDelta(zbytes& dst, const unsigned int offset,
                                Account* oldAccount,
                                const Account& newAccount) {
  ProtoAccount result;

  if (!AccountDeltaToProtobuf(oldAccount, newAccount, result)) {
    LOG_GENERAL(WARNING, "AccountDeltaToProtobuf failed");
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccount initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

template <class MAP>
bool Messenger::SetAccountStore(zbytes& dst, const unsigned int offset,
                                const MAP& addressToAccount) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Accounts to serialize: " << addressToAccount.size());

  for (const auto& entry : addressToAccount) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    if (!AccountToProtobuf(entry.second, *protoEntryAccount)) {
      LOG_GENERAL(WARNING, "AccountToProtobuf failed");
      return false;
    }
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
bool Messenger::GetAccountStore(const zbytes& src, const unsigned int offset,
                                MAP& addressToAccount) {
  if (offset > src.size()) {
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
                               << address.hex());
      return false;
    }

    addressToAccount[address] = account;
  }

  return true;
}

bool Messenger::GetAccountStore(const zbytes& src, const unsigned int offset,
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
                               << address.hex());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account, Account());
  }

  return true;
}

bool Messenger::GetAccountStore(const string& src, const unsigned int offset,
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
                               << address.hex());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account, Account());
  }

  return true;
}

bool Messenger::SetAccountStoreDelta(zbytes& dst, const unsigned int offset,
                                     AccountStoreTemp& accountStoreTemp,
                                     AccountStore& accountStore) {
  ProtoAccountStore result;

  LOG_GENERAL(INFO, "Account deltas to serialize: "
                        << accountStoreTemp.GetNumOfAccounts());

  std::vector<std::pair<Address, Account>> accountsToSerialize;
  accountsToSerialize.reserve(accountStoreTemp.GetAddressToAccount()->size());
  for (const auto& entry : *accountStoreTemp.GetAddressToAccount()) {
    accountsToSerialize.push_back(entry);
  }

  std::sort(std::begin(accountsToSerialize), std::end(accountsToSerialize),
            [](const auto& l, const auto& r) { return l.first < r.first; });

  for (const auto& entry : accountsToSerialize) {
    ProtoAccountStore::AddressAccount* protoEntry = result.add_entries();
    protoEntry->set_address(entry.first.data(), entry.first.size);
    ProtoAccount* protoEntryAccount = protoEntry->mutable_account();
    if (!AccountDeltaToProtobuf(accountStore.GetAccount(entry.first, true),
                                entry.second, *protoEntryAccount)) {
      LOG_GENERAL(WARNING, "AccountDeltaToProtobuf failed");
      return false;
    }
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
    const zbytes& src, const unsigned int offset,
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

bool Messenger::GetAccountStoreDelta(const zbytes& src,
                                     const unsigned int offset,
                                     AccountStore& accountStore,
                                     const bool revertible, bool temp) {
  ProtoAccountStore result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoAccountStore initialization failed");
    return false;
  }

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
                      << address.hex());
      return false;
    }

    accountStore.AddAccountDuringDeserialization(address, account, t_account,
                                                 fullCopy, revertible);
  }

  return true;
}

bool Messenger::GetAccountStoreDelta(const zbytes& src,
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

    LOG_GENERAL(WARNING,
                "Messenger::GetAccountStoreDelta address: " << address.hex());

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
    LOG_GENERAL(
        WARNING,
        "Messenger::GetAccountStoreDelta ProtobufToAccountDelta for addr: "
            << address.hex());
    if (!ProtobufToAccountDelta(entry.account(), account, address, fullCopy,
                                temp)) {
      LOG_GENERAL(WARNING,
                  "ProtobufToAccountDelta failed for account at address "
                      << address.hex());
      return false;
    }
    LOG_GENERAL(WARNING, "DESERIALIZE DELTA ACC: "
                             << address.hex()
                             << ", balance: " << account.GetBalance()
                             << ", nonce: " << account.GetNonce());
    accountStoreTemp.AddAccountDuringDeserialization(address, account);
  }

  return true;
}

bool Messenger::GetMbInfoHash(const std::vector<MicroBlockInfo>& mbInfos,
                              MBInfoHash& dst) {
  zbytes tmp;

  for (const auto& mbInfo : mbInfos) {
    ProtoMbInfo ProtoMbInfo;

    io::MbInfoToProtobuf(mbInfo, ProtoMbInfo);

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

  SHA256Calculator sha2;
  sha2.Update(tmp);
  tmp = sha2.Finalize();

  copy(tmp.begin(), tmp.end(), dst.asArray().begin());

  return true;
}

bool Messenger::SetTransactionCoreInfo(zbytes& dst, const unsigned int offset,
                                       const TransactionCoreInfo& transaction) {
  ProtoTransactionCoreInfo result;

  TransactionCoreInfoToProtobuf(transaction, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionCoreInfo initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionCoreInfo(const zbytes& src,
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

bool Messenger::SetTransaction(zbytes& dst, const unsigned int offset,
                               const Transaction& transaction) {
  ProtoTransaction result;

  TransactionToProtobuf(transaction, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransaction initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransaction(const zbytes& src, const unsigned int offset,
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

bool Messenger::GetTransaction(const string& src, const unsigned int offset,
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
    zbytes& dst, const unsigned int offset,
    const std::vector<uint32_t>& txnOffsets) {
  ProtoTxnFileOffset result;
  TransactionOffsetToProtobuf(txnOffsets, result);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxnFileOffset initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionFileOffset(const zbytes& src,
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

bool Messenger::SetTransactionArray(zbytes& dst, const unsigned int offset,
                                    const std::vector<Transaction>& txns) {
  ProtoTransactionArray result;
  TransactionArrayToProtobuf(txns, result);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionArray initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionArray(const zbytes& src,
                                    const unsigned int offset,
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
    zbytes& dst, const unsigned int offset,
    const TransactionReceipt& transactionReceipt) {
  ProtoTransactionReceipt result;

  TransactionReceiptToProtobuf(transactionReceipt, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTransactionReceipt initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTransactionReceipt(const zbytes& src,
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

bool Messenger::GetTransactionReceipt(const string& src,
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
    zbytes& dst, const unsigned int offset,
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
    const zbytes& src, const unsigned int offset,
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

bool Messenger::GetTransactionWithReceipt(
    const string& src, const unsigned int offset,
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

bool Messenger::SetPeer(zbytes& dst, const unsigned int offset,
                        const Peer& peer) {
  ProtoPeer result;

  PeerToProtobuf(peer, result);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoPeer initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetPeer(const zbytes& src, const unsigned int offset,
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
    zbytes& dst, const unsigned int offset,
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
    const zbytes& src, const unsigned int offset,
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

bool Messenger::SetDiagnosticDataNodes(zbytes& dst, const unsigned int offset,
                                       const uint32_t& shardingStructureVersion,
                                       const DequeOfShardMembers& shards,
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

bool Messenger::GetDiagnosticDataNodes(const zbytes& src,
                                       const unsigned int offset,
                                       uint32_t& shardingStructureVersion,
                                       DequeOfShardMembers& shards,
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

bool Messenger::SetDiagnosticDataCoinbase(zbytes& dst,
                                          const unsigned int offset,
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

bool Messenger::GetDiagnosticDataCoinbase(const zbytes& src,
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

bool Messenger::SetPMHello(zbytes& dst, const unsigned int offset,
                           const PairOfKey& key, const uint32_t listenPort) {
  PMHello result;

  SerializableToProtobufByteArray(key.second,
                                  *result.mutable_data()->mutable_pubkey());
  result.mutable_data()->set_listenport(listenPort);

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "PMHello.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, key.first, key.second, signature)) {
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

bool Messenger::GetPMHello(const zbytes& src, const unsigned int offset,
                           PubKey& pubKey, uint32_t& listenPort) {
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

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "PMHello signature wrong");
    return false;
  }

  return true;
}

// ============================================================================
// Directory Service messages
// ============================================================================

bool Messenger::SetDSPoWSubmission(
    zbytes& dst, const unsigned int offset, const uint64_t blockNumber,
    const uint8_t difficultyLevel, const Peer& submitterPeer,
    const PairOfKey& submitterKey, const uint64_t nonce,
    const string& resultingHash, const string& mixHash, const zbytes& extraData,
    const uint32_t& lookupId, const uint128_t& gasPrice,
    const GovProposalIdVotePair& govProposal, const string& version) {
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
  result.mutable_data()->set_extradata(extraData.data(), extraData.size());
  result.mutable_data()->set_lookupid(lookupId);

  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      gasPrice, *result.mutable_data()->mutable_gasprice());

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSPoWSubmission.Data initialization failed");
    return false;
  }

  // [Gov] first=proposalId,second=votevalue
  if (govProposal.first > 0 && govProposal.second > 0) {
    result.mutable_data()->mutable_govdata()->set_proposalid(govProposal.first);
    result.mutable_data()->mutable_govdata()->set_votevalue(govProposal.second);
    if (!result.data().govdata().IsInitialized()) {
      LOG_GENERAL(WARNING, "DSPoWSubmission [Gov] data initialization failed");
    }
  }

  result.mutable_data()->set_version(version);

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  // We use MultiSig::SignKey to emphasize that this is for the
  // Proof-of-Possession (PoP) phase (refer to #1097)
  Signature signature;
  if (!MultiSig::SignKey(tmp, submitterKey, signature)) {
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

bool Messenger::GetDSPoWSubmission(
    const zbytes& src, const unsigned int offset, uint64_t& blockNumber,
    uint8_t& difficultyLevel, Peer& submitterPeer, PubKey& submitterPubKey,
    uint64_t& nonce, string& resultingHash, string& mixHash, zbytes& extraData,
    Signature& signature, uint32_t& lookupId, uint128_t& gasPrice,
    uint32_t& govProposalId, uint32_t& govVoteValue, string& version) {
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
  if (result.data().extradata().size() > 32) {
    LOG_GENERAL(WARNING, "extra data is too large");
    return false;
  }
  extraData.resize(result.data().extradata().size());
  std::copy(result.data().extradata().begin(), result.data().extradata().end(),
            extraData.begin());
  lookupId = result.data().lookupid();
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.data().gasprice(),
                                                     gasPrice);
  if (result.data().govdata().IsInitialized()) {
    govProposalId = result.data().govdata().proposalid();
    govVoteValue = result.data().govdata().votevalue();
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  version = result.data().version();

  // We use MultiSig::VerifyKey to emphasize that this is for the
  // Proof-of-Possession (PoP) phase (refer to #1097)
  if (!MultiSig::VerifyKey(tmp, signature, submitterPubKey)) {
    LOG_GENERAL(WARNING, "PoW submission signature wrong");
    return false;
  }

  return true;
}

bool Messenger::SetDSPoWPacketSubmission(
    zbytes& dst, const unsigned int offset,
    const vector<DSPowSolution>& dsPowSolutions, const PairOfKey& keys) {
  DSPoWPacketSubmission result;

  for (const auto& sol : dsPowSolutions) {
    DSPowSolutionToProtobuf(sol,
                            *result.mutable_data()->add_dspowsubmissions());
  }

  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  Signature signature;
  if (!Schnorr::Sign(tmp, keys.first, keys.second, signature)) {
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

bool Messenger::GetDSPowPacketSubmission(const zbytes& src,
                                         const unsigned int offset,
                                         vector<DSPowSolution>& dsPowSolutions,
                                         PubKey& pubKey) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "DSPoWPacketSubmission signature wrong");
    return false;
  }

  for (const auto& powSubmission : result.data().dspowsubmissions()) {
    DSPowSolution sol;
    ProtobufToDSPowSolution(powSubmission, sol);
    dsPowSolutions.emplace_back(std::move(sol));
  }

  return true;
}

bool Messenger::SetDSMicroBlockSubmission(
    zbytes& dst, const unsigned int offset, const unsigned char microBlockType,
    const uint64_t epochNumber, const vector<MicroBlock>& microBlocks,
    const vector<zbytes>& stateDeltas, const PairOfKey& keys) {
  DSMicroBlockSubmission result;

  result.mutable_data()->set_microblocktype(microBlockType);
  result.mutable_data()->set_epochnumber(epochNumber);
  for (const auto& microBlock : microBlocks) {
    io::MicroBlockToProtobuf(microBlock,
                             *result.mutable_data()->add_microblocks());
  }
  for (const auto& stateDelta : stateDeltas) {
    result.mutable_data()->add_statedeltas(stateDelta.data(),
                                           stateDelta.size());
  }

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission.Data initialization failed");
    return false;
  }

  zbytes tmp(result.mutable_data()->ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, keys.first, keys.second, signature)) {
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
    const zbytes& src, const unsigned int offset, unsigned char& microBlockType,
    uint64_t& epochNumber, vector<MicroBlock>& microBlocks,
    vector<zbytes>& stateDeltas, PubKey& pubKey) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission signature wrong");
    return false;
  }

  // Deserialize the remaining fields
  microBlockType = result.data().microblocktype();
  epochNumber = result.data().epochnumber();
  for (const auto& proto_mb : result.data().microblocks()) {
    MicroBlock microBlock;
    io::ProtobufToMicroBlock(proto_mb, microBlock);
    microBlocks.emplace_back(std::move(microBlock));
  }

  for (const auto& proto_delta : result.data().statedeltas()) {
    stateDeltas.emplace_back(zbytes(proto_delta.size()));
    copy(proto_delta.begin(), proto_delta.end(), stateDeltas.back().begin());
  }

  return true;
}

bool Messenger::SetDSDSBlockAnnouncement(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey, const DSBlock& dsBlock,
    const DequeOfShardMembers& shards, const MapOfPubKeyPoW& allPoWs,
    const MapOfPubKeyPoW& dsWinnerPoWs, zbytes& messageToCosign) {
  ConsensusAnnouncement announcement;

  // Set the DSBlock announcement parameters

  DSDSBlockAnnouncement* dsblock = announcement.mutable_dsblock();

  io::DSBlockToProtobuf(dsBlock, *dsblock->mutable_dsblock());

  AnnouncementShardingStructureToProtobuf(shards, allPoWs,
                                          *dsblock->mutable_sharding());

  for (const auto& kv : dsWinnerPoWs) {
    auto protoDSWinnerPoW = dsblock->add_dswinnerpows();
    SerializableToProtobufByteArray(kv.first,
                                    *protoDSWinnerPoW->mutable_pubkey());
    ProtoPoWSolution* proto_soln = protoDSWinnerPoW->mutable_powsoln();
    const auto soln = kv.second;
    proto_soln->set_nonce(soln.m_nonce);
    proto_soln->set_result(soln.m_result.data(), soln.m_result.size());
    proto_soln->set_mixhash(soln.m_mixhash.data(), soln.m_mixhash.size());
    proto_soln->set_lookupid(soln.m_lookupId);
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        soln.m_gasPrice, *proto_soln->mutable_gasprice());
    proto_soln->mutable_govdata()->set_proposalid(soln.m_govProposal.first);
    proto_soln->mutable_govdata()->set_votevalue(soln.m_govProposal.second);
    proto_soln->set_extradata(soln.m_extraData.data(), soln.m_extraData.size());
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey, DSBlock& dsBlock,
    DequeOfShardMembers& shards, MapOfPubKeyPoW& allPoWs,
    MapOfPubKeyPoW& dsWinnerPoWs, zbytes& messageToCosign) {
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

  if (!announcement.dsblock().IsInitialized()) {
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

  if (!io::ProtobufToDSBlock(dsblock.dsblock(), dsBlock)) {
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
    std::array<unsigned char, 32> result{};
    std::array<unsigned char, 32> mixhash{};
    uint128_t gasPrice;
    uint32_t govProposalId{};
    uint32_t govVoteValue{};

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
    if (protoDSWinnerPoW.powsoln().govdata().IsInitialized()) {
      govProposalId = protoDSWinnerPoW.powsoln().govdata().proposalid();
      govVoteValue = protoDSWinnerPoW.powsoln().govdata().votevalue();
    }
    if (protoDSWinnerPoW.powsoln().extradata().size() > 32) {
      LOG_GENERAL(WARNING, "extra data is too large");
      return false;
    }
    zbytes extraData(protoDSWinnerPoW.powsoln().extradata().begin(),
                     protoDSWinnerPoW.powsoln().extradata().end());
    dsWinnerPoWs.emplace(
        key,
        PoWSolution(protoDSWinnerPoW.powsoln().nonce(), result, mixhash,
                    protoDSWinnerPoW.powsoln().lookupid(), gasPrice,
                    std::make_pair(govProposalId, govVoteValue), extraData));
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
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey, const TxBlock& txBlock,
    const shared_ptr<MicroBlock>& microBlock, zbytes& messageToCosign) {
  ConsensusAnnouncement announcement;

  // Set the FinalBlock announcement parameters

  DSFinalBlockAnnouncement* finalblock = announcement.mutable_finalblock();
  io::TxBlockToProtobuf(txBlock, *finalblock->mutable_txblock());
  if (microBlock != nullptr) {
    io::MicroBlockToProtobuf(*microBlock, *finalblock->mutable_microblock());
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey, TxBlock& txBlock,
    shared_ptr<MicroBlock>& microBlock, zbytes& messageToCosign) {
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

  if (!announcement.finalblock().IsInitialized()) {
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
  if (!io::ProtobufToTxBlock(finalblock.txblock(), txBlock)) {
    return false;
  }

  if (finalblock.microblock().IsInitialized()) {
    io::ProtobufToMicroBlock(finalblock.microblock(), *microBlock);
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
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey, const VCBlock& vcBlock,
    zbytes& messageToCosign) {
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey, VCBlock& vcBlock,
    zbytes& messageToCosign) {
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

  if (!announcement.vcblock().IsInitialized()) {
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
    zbytes& dst, const unsigned int offset,
    const vector<BlockHash>& missingMicroBlockHashes, const uint64_t epochNum,
    const uint32_t listenPort) {
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
    const zbytes& src, const unsigned int offset,
    vector<BlockHash>& missingMicroBlockHashes, uint64_t& epochNum,
    uint32_t& listenPort) {
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
    zbytes& dst, const unsigned int offset, const uint32_t shardId,
    const DSBlock& dsBlock, const std::vector<VCBlock>& vcBlocks,
    const uint32_t& shardingStructureVersion,
    const DequeOfShardMembers& shards) {
  NodeDSBlock result;

  result.set_shardid(shardId);
  io::DSBlockToProtobuf(dsBlock, *result.mutable_dsblock());

  for (const auto& vcblock : vcBlocks) {
    io::VCBlockToProtobuf(vcblock, *result.add_vcblocks());
  }
  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_sharding());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeDSBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCDSBlocksMessage(const zbytes& src,
                                         const unsigned int offset,
                                         uint32_t& shardId, DSBlock& dsBlock,
                                         std::vector<VCBlock>& vcBlocks,
                                         uint32_t& shardingStructureVersion,
                                         DequeOfShardMembers& shards) {
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
  if (!io::ProtobufToDSBlock(result.dsblock(), dsBlock)) {
    return false;
  }

  for (const auto& proto_vcblock : result.vcblocks()) {
    VCBlock vcblock;
    if (!io::ProtobufToVCBlock(proto_vcblock, vcblock)) {
      LOG_GENERAL(WARNING, "ProtobufToVCBlock failed");
      return false;
    }
    vcBlocks.emplace_back(std::move(vcblock));
  }

  return ProtobufToShardingStructure(result.sharding(),
                                     shardingStructureVersion, shards);
}

bool Messenger::SetNodeVCFinalBlock(zbytes& dst, const unsigned int offset,
                                    const uint64_t dsBlockNumber,
                                    const uint32_t consensusID,
                                    const TxBlock& txBlock,
                                    const zbytes& stateDelta,
                                    const std::vector<VCBlock>& vcBlocks) {
  NodeVCFinalBlock result;

  result.set_dsblocknumber(dsBlockNumber);
  result.set_consensusid(consensusID);
  io::TxBlockToProtobuf(txBlock, *result.mutable_txblock());
  result.set_statedelta(stateDelta.data(), stateDelta.size());

  for (const auto& vcblock : vcBlocks) {
    io::VCBlockToProtobuf(vcblock, *result.add_vcblocks());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCFinalBlock(const zbytes& src,
                                    const unsigned int offset,
                                    uint64_t& dsBlockNumber,
                                    uint32_t& consensusID, TxBlock& txBlock,
                                    zbytes& stateDelta,
                                    std::vector<VCBlock>& vcBlocks) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeVCFinalBlock result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeVCFinalBlock initialization failed");
    return false;
  }

  dsBlockNumber = result.dsblocknumber();
  consensusID = result.consensusid();
  if (!io::ProtobufToTxBlock(result.txblock(), txBlock)) {
    return false;
  }
  stateDelta.resize(result.statedelta().size());
  copy(result.statedelta().begin(), result.statedelta().end(),
       stateDelta.begin());

  for (const auto& proto_vcblock : result.vcblocks()) {
    VCBlock vcblock;
    if (!io::ProtobufToVCBlock(proto_vcblock, vcblock)) {
      LOG_GENERAL(WARNING, "ProtobufToVCBlock failed");
      return false;
    }
    vcBlocks.emplace_back(std::move(vcblock));
  }
  return true;
}

bool Messenger::SetNodeFinalBlock(zbytes& dst, const unsigned int offset,
                                  const uint64_t dsBlockNumber,
                                  const uint32_t consensusID,
                                  const TxBlock& txBlock,
                                  const zbytes& stateDelta) {
  NodeFinalBlock result;

  result.set_dsblocknumber(dsBlockNumber);
  result.set_consensusid(consensusID);
  io::TxBlockToProtobuf(txBlock, *result.mutable_txblock());
  result.set_statedelta(stateDelta.data(), stateDelta.size());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeFinalBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeFinalBlock(const zbytes& src, const unsigned int offset,
                                  uint64_t& dsBlockNumber,
                                  uint32_t& consensusID, TxBlock& txBlock,
                                  zbytes& stateDelta) {
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
  if (!io::ProtobufToTxBlock(result.txblock(), txBlock)) {
    return false;
  }
  stateDelta.resize(result.statedelta().size());
  copy(result.statedelta().begin(), result.statedelta().end(),
       stateDelta.begin());

  return true;
}

bool Messenger::SetNodeMBnForwardTransaction(
    zbytes& dst, const unsigned int offset, const MicroBlock& microBlock,
    const vector<TransactionWithReceipt>& txns) {
  NodeMBnForwardTransaction result;

  io::MicroBlockToProtobuf(microBlock, *result.mutable_microblock());

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

bool Messenger::SetNodePendingTxn(
    zbytes& dst, const unsigned offset, const uint64_t& epochnum,
    const unordered_map<TxnHash, TxnStatus>& hashCodeMap,
    const uint32_t shardId, const PairOfKey& key) {
  NodePendingTxn result;

  SerializableToProtobufByteArray(key.second,
                                  *result.mutable_data()->mutable_pubkey());
  result.mutable_data()->set_epochnumber(epochnum);
  result.mutable_data()->set_shardid(shardId);

  SHA256Calculator sha2;

  for (const auto& hashCodePair : hashCodeMap) {
    auto protoHashCodePair = result.mutable_data()->add_hashcodepair();
    protoHashCodePair->set_txnhash(hashCodePair.first.data(),
                                   hashCodePair.first.size);
    protoHashCodePair->set_code(hashCodePair.second);

    sha2.Update(hashCodePair.first.data(), hashCodePair.first.size);
    sha2.Update(to_string(hashCodePair.second));
  }

  const zbytes& txnlisthash = sha2.Finalize();
  result.mutable_data()->set_txnlisthash(txnlisthash.data(),
                                         txnlisthash.size());

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "NodePendingTxn.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, key.first, key.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign NodePendingTxn");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodePendingTxn initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodePendingTxn(
    const zbytes& src, const unsigned offset, uint64_t& epochnum,
    unordered_map<TxnHash, TxnStatus>& hashCodeMap, uint32_t& shardId,
    PubKey& pubKey, zbytes& txnListHash) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodePendingTxn result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "NodePendingTxn initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().pubkey(), pubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, pubKey)) {
    LOG_GENERAL(WARNING, "NodePendingTxn signature wrong");
    return false;
  }

  SHA256Calculator sha2;

  for (const auto& codeHashPair : result.data().hashcodepair()) {
    TxnHash txhash;
    unsigned int size = min((unsigned int)codeHashPair.txnhash().size(),
                            (unsigned int)txhash.size);
    copy(codeHashPair.txnhash().begin(), codeHashPair.txnhash().begin() + size,
         txhash.asArray().begin());
    hashCodeMap.emplace(txhash, static_cast<TxnStatus>(codeHashPair.code()));

    sha2.Update(txhash.data(), txhash.size);
  }

  txnListHash = sha2.Finalize();

  epochnum = result.data().epochnumber();
  shardId = result.data().shardid();

  return true;
}

bool Messenger::GetNodeMBnForwardTransaction(const zbytes& src,
                                             const unsigned int offset,
                                             MBnForwardedTxnEntry& entry) {
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

  io::ProtobufToMicroBlock(result.microblock(), entry.m_microBlock);

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

bool Messenger::SetNodeVCBlock(zbytes& dst, const unsigned int offset,
                               const VCBlock& vcBlock) {
  NodeVCBlock result;

  io::VCBlockToProtobuf(vcBlock, *result.mutable_vcblock());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeVCBlock initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeVCBlock(const zbytes& src, const unsigned int offset,
                               VCBlock& vcBlock) {
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

  return io::ProtobufToVCBlock(result.vcblock(), vcBlock);
}

bool Messenger::SetNodeForwardTxnBlock(zbytes& dst, const unsigned int offset,
                                       const uint64_t& epochNumber,
                                       const uint64_t& dsBlockNum,
                                       const uint32_t shardId,
                                       const PairOfKey& lookupKey,
                                       std::vector<Transaction>& transactions) {
  NodeForwardTxnBlock result;

  result.set_epochnumber(epochNumber);
  result.set_dsblocknum(dsBlockNum);
  result.set_shardid(shardId);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  unsigned int txnsCurrentCount = 0, msg_size = 0;

  for (auto txn = transactions.begin(); txn != transactions.end();) {
    if (msg_size >= PACKET_BYTESIZE_LIMIT) {
      break;
    }

    auto protoTxn = std::make_unique<ProtoTransaction>();
    TransactionToProtobuf(*txn, *protoTxn);
    unsigned txn_size = protoTxn->ByteSizeLong();
    if ((msg_size + txn_size) > PACKET_BYTESIZE_LIMIT &&
        txn_size >= SMALL_TXN_SIZE) {
      continue;
    }
    *result.add_transactions() = *protoTxn;
    txnsCurrentCount++;
    msg_size += protoTxn->ByteSizeLong();
    txn = transactions.erase(txn);
  }

  Signature signature;
  if (result.transactions().size() > 0) {
    zbytes tmp;
    tmp.reserve(Transaction::AVERAGE_TXN_SIZE_BYTES *
                result.transactions_size());
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }
    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
      LOG_GENERAL(WARNING, "Failed to sign transactions");
      return false;
    }
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeForwardTxnBlock initialization failed");
    return false;
  }

  LOG_GENERAL(
      INFO, "Epoch: " << epochNumber << " Current txns: " << txnsCurrentCount);

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetNodeForwardTxnBlock(zbytes& dst, const unsigned int offset,
                                       const uint64_t& epochNumber,
                                       const uint64_t& dsBlockNum,
                                       const uint32_t& shardId,
                                       const PubKey& lookupKey,
                                       std::vector<Transaction>& txns,
                                       const Signature& signature) {
  NodeForwardTxnBlock result;

  result.set_epochnumber(epochNumber);
  result.set_dsblocknum(dsBlockNum);
  result.set_shardid(shardId);
  SerializableToProtobufByteArray(lookupKey, *result.mutable_pubkey());

  unsigned int txnsCount = 0, msg_size = 0;

  for (const auto& txn : txns) {
    if (msg_size >= PACKET_BYTESIZE_LIMIT) {
      break;
    }

    auto protoTxn = std::make_unique<ProtoTransaction>();
    TransactionToProtobuf(txn, *protoTxn);
    const unsigned txn_size = protoTxn->ByteSizeLong();
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
    const zbytes& src, const unsigned int offset, uint64_t& epochNumber,
    uint64_t& dsBlockNum, uint32_t& shardId, PubKey& lookupPubKey,
    std::vector<Transaction>& txns, Signature& signature) {
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
    zbytes tmp;
    tmp.reserve(Transaction::AVERAGE_TXN_SIZE_BYTES *
                result.transactions_size());
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }
    PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

    if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions");
      return false;
    }
    txns.reserve(result.transactions_size());
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
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey,
    const MicroBlock& microBlock, zbytes& messageToCosign) {
  ConsensusAnnouncement announcement;

  // Set the MicroBlock announcement parameters

  NodeMicroBlockAnnouncement* microblock = announcement.mutable_microblock();
  io::MicroBlockToProtobuf(microBlock, *microblock->mutable_microblock());

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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PubKey& leaderKey, MicroBlock& microBlock,
    zbytes& messageToCosign) {
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

  if (!announcement.microblock().IsInitialized()) {
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
  io::ProtobufToMicroBlock(microblock.microblock(), microBlock);

  // Get the part of the announcement that should be co-signed during the first
  // round of consensus

  messageToCosign.clear();
  if (!microBlock.GetHeader().Serialize(messageToCosign, 0)) {
    LOG_GENERAL(WARNING, "MicroBlockHeader serialization failed");
    return false;
  }

  return true;
}

bool Messenger::ShardStructureToArray(zbytes& dst, const unsigned int offset,
                                      const uint32_t& version,
                                      const DequeOfShardMembers& shards) {
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

bool Messenger::ArrayToShardStructure(const zbytes& src,
                                      const unsigned int offset,
                                      uint32_t& version,
                                      DequeOfShardMembers& shards) {
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
    zbytes& dst, const unsigned int offset,
    const vector<TxnHash>& missingTxnHashes, const uint64_t epochNum,
    const uint32_t listenPort) {
  NodeMissingTxnsErrorMsg result;

  for (const auto& hash : missingTxnHashes) {
    result.add_txnhashes(hash.data(), hash.size);
  }

  if (result.txnhashes_size() > 0) {
    LOG_EPOCH(INFO, epochNum, "Missing txns: " << result.txnhashes_size());
  }

  result.set_epochnum(epochNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeMissingTxnsErrorMsg initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeMissingTxnsErrorMsg(const zbytes& src,
                                           const unsigned int offset,
                                           vector<TxnHash>& missingTxnHashes,
                                           uint64_t& epochNum,
                                           uint32_t& listenPort) {
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

bool Messenger::SetNodeGetVersion(zbytes& dst, const unsigned int offset,
                                  const uint32_t listenPort) {
  NodeGetVersion result;
  result.set_listenport(listenPort);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeGetVersion initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeGetVersion(const zbytes& src, const unsigned int offset,
                                  uint32_t& listenPort) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeGetVersion result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeGetVersion initialization failed");
    return false;
  }

  listenPort = result.listenport();

  return true;
}

bool Messenger::SetNodeSetVersion(zbytes& dst, const unsigned int offset,
                                  const std::string& version) {
  NodeSetVersion result;
  result.set_version(version);
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeSetVersion initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeSetVersion(const zbytes& src, const unsigned int offset,
                                  std::string& version) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeSetVersion result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeSetVersion initialization failed");
    return false;
  }

  version = result.version();

  return true;
}

// ============================================================================
// Lookup messages
// ============================================================================

bool Messenger::SetLookupGetSeedPeers(zbytes& dst, const unsigned int offset,
                                      const uint32_t listenPort) {
  LookupGetSeedPeers result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetSeedPeers initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetSeedPeers(const zbytes& src,
                                      const unsigned int offset,
                                      uint32_t& listenPort) {
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

bool Messenger::SetLookupSetSeedPeers(zbytes& dst, const unsigned int offset,
                                      const PairOfKey& lookupKey,
                                      const vector<Peer>& candidateSeeds) {
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
    zbytes tmp;
    if (!RepeatableToArray(result.candidateseeds(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds");
      return false;
    }
    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetSeedPeers(const zbytes& src,
                                      const unsigned int offset,
                                      PubKey& lookupPubKey,
                                      vector<Peer>& candidateSeeds) {
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
    zbytes tmp;
    if (!RepeatableToArray(result.candidateseeds(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize candidate seeds");
      return false;
    }

    if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in candidate seeds");
      return false;
    }
  }

  return true;
}

bool Messenger::SetLookupGetDSInfoFromSeed(zbytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort,
                                           const bool initialDS) {
  LookupGetDSInfoFromSeed result;

  result.set_listenport(listenPort);
  result.set_initialds(initialDS);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSInfoFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSInfoFromSeed(const zbytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort,
                                           bool& initialDS) {
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

bool Messenger::SetLookupSetDSInfoFromSeed(zbytes& dst,
                                           const unsigned int offset,
                                           const PairOfKey& senderKey,
                                           const uint32_t& dsCommitteeVersion,
                                           const DequeOfNode& dsNodes,
                                           const bool initialDS) {
  LookupSetDSInfoFromSeed result;

  DSCommitteeToProtobuf(dsCommitteeVersion, dsNodes,
                        *result.mutable_dscommittee());

  SerializableToProtobufByteArray(senderKey.second, *result.mutable_pubkey());

  zbytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee");
    return false;
  }

  Signature signature;
  if (!Schnorr::Sign(tmp, senderKey.first, senderKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, PubKey& senderPubKey,
    uint32_t& dsCommitteeVersion, DequeOfNode& dsNodes, bool& initialDS) {
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

  zbytes tmp;
  if (!SerializeToArray(result.dscommittee(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize DS committee");
    return false;
  }

  initialDS = result.initialds();

  if (!Schnorr::Verify(tmp, signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in DS nodes info");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetDSBlockFromSeed(zbytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort,
                                            const bool includeMinerInfo) {
  LookupGetDSBlockFromSeed result;

  result.set_lowblocknum(lowBlockNum);
  result.set_highblocknum(highBlockNum);
  result.set_listenport(listenPort);
  result.set_includeminerinfo(includeMinerInfo);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSBlockFromSeed(
    const zbytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, uint32_t& listenPort, bool& includeMinerInfo) {
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
  includeMinerInfo = result.includeminerinfo();

  return true;
}

bool Messenger::SetLookupSetDSBlockFromSeed(zbytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const PairOfKey& lookupKey,
                                            const vector<DSBlock>& dsBlocks) {
  LookupSetDSBlockFromSeed result;

  result.mutable_data()->set_lowblocknum(lowBlockNum);
  result.mutable_data()->set_highblocknum(highBlockNum);

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  for (const auto& dsblock : dsBlocks) {
    io::DSBlockToProtobuf(dsblock, *result.mutable_data()->add_dsblocks());
  }

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<DSBlock>& dsBlocks) {
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
    if (!io::ProtobufToDSBlock(proto_dsblock, dsblock)) {
      LOG_GENERAL(WARNING, "ProtobufToDSBlock failed");
      return false;
    }
    dsBlocks.emplace_back(dsblock);
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetDSBlockFromSeed");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetMinerInfoFromSeed(
    zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
    const map<uint64_t, pair<MinerInfoDSComm, MinerInfoShards>>&
        minerInfoPerDS) {
  LookupSetMinerInfoFromSeed result;

  for (const auto& dsBlockAndMinerInfo : minerInfoPerDS) {
    LookupSetMinerInfoFromSeed::MinerInfo tmpMinerInfo;

    // Set the MinerInfoDSComm member for the DS block number
    for (const auto& dsnode : dsBlockAndMinerInfo.second.first.m_dsNodes) {
      ProtoMinerInfoDSComm::Node* protodsnode =
          tmpMinerInfo.mutable_minerinfodscomm()->add_dsnodes();
      SerializableToProtobufByteArray(dsnode, *protodsnode->mutable_pubkey());
    }
    for (const auto& dsnode :
         dsBlockAndMinerInfo.second.first.m_dsNodesEjected) {
      ProtoMinerInfoDSComm::Node* protodsnode =
          tmpMinerInfo.mutable_minerinfodscomm()->add_dsnodesejected();
      SerializableToProtobufByteArray(dsnode, *protodsnode->mutable_pubkey());
    }
    if (!result.IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoMinerInfoDSComm initialization failed");
      return false;
    }

    // Set the MinerInfoShards member for the DS block number
    for (const auto& shard : dsBlockAndMinerInfo.second.second.m_shards) {
      ProtoMinerInfoShards::Shard* protoshard =
          tmpMinerInfo.mutable_minerinfoshards()->add_shards();
      protoshard->set_shardsize(shard.m_shardSize);
      for (const auto& shardnode : shard.m_shardNodes) {
        ProtoMinerInfoShards::Node* protoshardnode =
            protoshard->add_shardnodes();
        SerializableToProtobufByteArray(shardnode,
                                        *protoshardnode->mutable_pubkey());
      }
    }
    if (!result.IsInitialized()) {
      LOG_GENERAL(WARNING, "ProtoMinerInfoShards initialization failed");
      return false;
    }

    // Add both to result map
    auto protoMap = result.mutable_data()->mutable_minerinfoperds();
    (*protoMap)[dsBlockAndMinerInfo.first] = tmpMinerInfo;
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetMinerInfoFromSeed.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign miner info");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetDSBlockFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetMinerInfoFromSeed(
    const zbytes& src, const unsigned int offset, PubKey& lookupPubKey,
    map<uint64_t, pair<MinerInfoDSComm, MinerInfoShards>>& minerInfoPerDS) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetMinerInfoFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetMinerInfoFromSeed initialization failed");
    return false;
  }

  // Check signature first
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in LookupSetMinerInfoFromSeed");
    return false;
  }

  // Populate the map
  minerInfoPerDS.clear();
  for (const auto& dsBlockAndMinerInfo : result.data().minerinfoperds()) {
    // Get the MinerInfoDSComm member for the DS block number
    MinerInfoDSComm tmpDSComm;
    for (const auto& protodsnode :
         dsBlockAndMinerInfo.second.minerinfodscomm().dsnodes()) {
      PubKey pubkey;
      PROTOBUFBYTEARRAYTOSERIALIZABLE(protodsnode.pubkey(), pubkey);
      tmpDSComm.m_dsNodes.emplace_back(pubkey);
    }
    for (const auto& protodsnode :
         dsBlockAndMinerInfo.second.minerinfodscomm().dsnodesejected()) {
      PubKey pubkey;
      PROTOBUFBYTEARRAYTOSERIALIZABLE(protodsnode.pubkey(), pubkey);
      tmpDSComm.m_dsNodesEjected.emplace_back(pubkey);
    }

    // Get the MinerInfoShards member for the DS block number
    MinerInfoShards tmpShards;
    for (const auto& protoshard :
         dsBlockAndMinerInfo.second.minerinfoshards().shards()) {
      MinerInfoShards::MinerInfoShard shard;
      shard.m_shardSize = protoshard.shardsize();
      for (const auto& protoshardnode : protoshard.shardnodes()) {
        PubKey pubkey;
        PROTOBUFBYTEARRAYTOSERIALIZABLE(protoshardnode.pubkey(), pubkey);
        shard.m_shardNodes.emplace_back(pubkey);
      }
      tmpShards.m_shards.emplace_back(shard);
    }

    // Add both to minerInfoPerDS map
    minerInfoPerDS.emplace(dsBlockAndMinerInfo.first,
                           make_pair(tmpDSComm, tmpShards));
  }

  return true;
}

bool Messenger::SetLookupGetTxBlockFromSeed(zbytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const uint32_t listenPort) {
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

bool Messenger::SetLookupGetVCFinalBlockFromL2l(zbytes& dst,
                                                const unsigned int offset,
                                                const uint64_t& blockNum,
                                                const Peer& sender,
                                                const PairOfKey& seedKey) {
  LookupGetVCFinalBlockFromL2l result;

  result.mutable_data()->set_blocknum(blockNum);

  PeerToProtobuf(sender, *result.mutable_data()->mutable_sender());

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetVCFinalBlockFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign LookupGetVCFinalBlockFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetVCFinalBlockFromL2l initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetVCFinalBlockFromL2l(const zbytes& src,
                                                const unsigned int offset,
                                                uint64_t& blockNum, Peer& from,
                                                PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetVCFinalBlockFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "GetLookupGetVCFinalBlockFromL2l initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "GetLookupGetVCFinalBlockFromL2l signature wrong");
    return false;
  }

  blockNum = result.data().blocknum();
  ProtobufToPeer(result.data().sender(), from);

  return true;
}

bool Messenger::SetLookupGetDSBlockFromL2l(zbytes& dst,
                                           const unsigned int offset,
                                           const uint64_t& blockNum,
                                           const Peer& sender,
                                           const PairOfKey& seedKey) {
  LookupGetDSBlockFromL2l result;

  result.mutable_data()->set_blocknum(blockNum);

  PeerToProtobuf(sender, *result.mutable_data()->mutable_sender());

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign LookupGetDSBlockFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetDSBlockFromL2l initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSBlockFromL2l(const zbytes& src,
                                           const unsigned int offset,
                                           uint64_t& blockNum, Peer& from,
                                           PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetDSBlockFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "GetLookupGetDSBlockFromL2l initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "GetLookupGetDSBlockFromL2l signature wrong");
    return false;
  }

  blockNum = result.data().blocknum();
  ProtobufToPeer(result.data().sender(), from);

  return true;
}

bool Messenger::SetLookupGetMBnForwardTxnFromL2l(
    zbytes& dst, const unsigned int offset, const uint64_t& blockNum,
    const uint32_t& shardId, const Peer& sender, const PairOfKey& seedKey) {
  LookupGetMBnForwardTxnFromL2l result;

  result.mutable_data()->set_blocknum(blockNum);
  result.mutable_data()->set_shardid(shardId);

  PeerToProtobuf(sender, *result.mutable_data()->mutable_sender());

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetMBnForwardTxnFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign LookupGetMBnForwardTxnFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetMBnForwardTxnFromL2l initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetMBnForwardTxnFromL2l(const zbytes& src,
                                                 const unsigned int offset,
                                                 uint64_t& blockNum,
                                                 uint32_t& shardId, Peer& from,
                                                 PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetMBnForwardTxnFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetMBnForwardTxnFromL2l initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "LookupGetMBnForwardTxnFromL2l signature wrong");
    return false;
  }

  blockNum = result.data().blocknum();
  shardId = result.data().shardid();
  ProtobufToPeer(result.data().sender(), from);

  return true;
}

bool Messenger::SetLookupGetPendingTxnFromL2l(
    zbytes& dst, const unsigned int offset, const uint64_t& blockNum,
    const uint32_t& shardId, const Peer& sender, const PairOfKey& seedKey) {
  LookupGetPendingTxnFromL2l result;

  result.mutable_data()->set_blocknum(blockNum);
  result.mutable_data()->set_shardid(shardId);

  PeerToProtobuf(sender, *result.mutable_data()->mutable_sender());

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetPendingTxnFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign LookupGetPendingTxnFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetPendingTxnFromL2l initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetPendingTxnFromL2l(const zbytes& src,
                                              const unsigned int offset,
                                              uint64_t& blockNum,
                                              uint32_t& shardId, Peer& from,
                                              PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetPendingTxnFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetPendingTxnFromL2l initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "LookupGetPendingTxnFromL2l signature wrong");
    return false;
  }

  blockNum = result.data().blocknum();
  shardId = result.data().shardid();
  ProtobufToPeer(result.data().sender(), from);

  return true;
}

bool Messenger::GetLookupGetTxBlockFromSeed(const zbytes& src,
                                            const unsigned int offset,
                                            uint64_t& lowBlockNum,
                                            uint64_t& highBlockNum,
                                            uint32_t& listenPort) {
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

bool Messenger::SetLookupSetTxBlockFromSeed(zbytes& dst,
                                            const unsigned int offset,
                                            const uint64_t lowBlockNum,
                                            const uint64_t highBlockNum,
                                            const PairOfKey& lookupKey,
                                            const vector<TxBlock>& txBlocks) {
  LookupSetTxBlockFromSeed result;

  result.mutable_data()->set_lowblocknum(lowBlockNum);
  result.mutable_data()->set_highblocknum(highBlockNum);

  for (const auto& txblock : txBlocks) {
    io::TxBlockToProtobuf(txblock, *result.mutable_data()->add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<TxBlock>& txBlocks) {
  LookupSetTxBlockFromSeed result;

  google::protobuf::io::ArrayInputStream arrayIn(src.data() + offset,
                                                 src.size() - offset);
  google::protobuf::io::CodedInputStream codedIn(&arrayIn);

  codedIn.SetTotalBytesLimit(MAX_READ_WATERMARK_IN_BYTES);  // changed dec 2017

  if (!result.ParseFromCodedStream(&codedIn) ||
      !codedIn.ConsumedEntireMessage() || !result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetTxBlockFromSeed initialization failed");
    return false;
  }

  lowBlockNum = result.data().lowblocknum();
  highBlockNum = result.data().highblocknum();

  for (const auto& txblock : result.data().txblocks()) {
    TxBlock block;
    if (!io::ProtobufToTxBlock(txblock, block)) {
      LOG_GENERAL(WARNING, "ProtobufToTxBlock failed");
      return false;
    }
    txBlocks.emplace_back(block);
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetTxBlockFromSeed");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetStateDeltaFromSeed(zbytes& dst,
                                               const unsigned int offset,
                                               const uint64_t blockNum,
                                               const uint32_t listenPort) {
  LookupGetStateDeltaFromSeed result;

  result.set_blocknum(blockNum);
  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetStateDeltaFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::SetLookupGetStateDeltasFromSeed(zbytes& dst,
                                                const unsigned int offset,
                                                uint64_t& lowBlockNum,
                                                uint64_t& highBlockNum,
                                                const uint32_t listenPort) {
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

bool Messenger::GetLookupGetStateDeltaFromSeed(const zbytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               uint32_t& listenPort) {
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

bool Messenger::GetLookupGetStateDeltasFromSeed(const zbytes& src,
                                                const unsigned int offset,
                                                uint64_t& lowBlockNum,
                                                uint64_t& highBlockNum,
                                                uint32_t& listenPort) {
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

bool Messenger::SetLookupSetStateDeltaFromSeed(zbytes& dst,
                                               const unsigned int offset,
                                               const uint64_t blockNum,
                                               const PairOfKey& lookupKey,
                                               const zbytes& stateDelta) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    zbytes& dst, const unsigned int offset, const uint64_t lowBlockNum,
    const uint64_t highBlockNum, const PairOfKey& lookupKey,
    const vector<zbytes>& stateDeltas) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetStateDeltaFromSeed(const zbytes& src,
                                               const unsigned int offset,
                                               uint64_t& blockNum,
                                               PubKey& lookupPubKey,
                                               zbytes& stateDelta) {
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

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in state delta");
    return false;
  }

  return true;
}

bool Messenger::GetLookupSetStateDeltasFromSeed(
    const zbytes& src, const unsigned int offset, uint64_t& lowBlockNum,
    uint64_t& highBlockNum, PubKey& lookupPubKey, vector<zbytes>& stateDeltas) {
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
    zbytes tmp;
    tmp.resize(delta.size());
    std::copy(delta.begin(), delta.end(), tmp.begin());
    stateDeltas.emplace_back(tmp);
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in state deltas");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetLookupOffline(zbytes& dst,
                                          const unsigned int offset,
                                          const uint8_t msgType,
                                          const uint32_t listenPort,
                                          const PairOfKey& lookupKey) {
  LookupSetLookupOffline result;

  result.mutable_data()->set_msgtype(msgType);
  result.mutable_data()->set_listenport(listenPort);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOffline.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetLookupOffline(const zbytes& src,
                                          const unsigned int offset,
                                          uint8_t& msgType,
                                          uint32_t& listenPort,
                                          PubKey& lookupPubkey) {
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

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubkey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubkey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetLookupOffline");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetLookupOnline(zbytes& dst, const unsigned int offset,
                                         const uint8_t msgType,
                                         const uint32_t listenPort,
                                         const PairOfKey& lookupKey) {
  LookupSetLookupOnline result;

  result.mutable_data()->set_msgtype(msgType);
  result.mutable_data()->set_listenport(listenPort);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupSetLookupOnline.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetLookupOnline(const zbytes& src,
                                         const unsigned int offset,
                                         uint8_t& msgType, uint32_t& listenPort,
                                         PubKey& pubKey) {
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

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, pubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in GetLookupSetLookupOnline");
    return false;
  }
  return true;
}

bool Messenger::SetLookupGetOfflineLookups(zbytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LookupGetOfflineLookups result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetOfflineLookups initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetOfflineLookups(const zbytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
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

bool Messenger::SetLookupSetOfflineLookups(zbytes& dst,
                                           const unsigned int offset,
                                           const PairOfKey& lookupKey,
                                           const vector<Peer>& nodes) {
  LookupSetOfflineLookups result;

  for (const auto& node : nodes) {
    SerializableToProtobufByteArray(node, *result.add_nodes());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.nodes().size() > 0) {
    zbytes tmp;
    if (!RepeatableToArray(result.nodes(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes");
      return false;
    }

    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetOfflineLookups(const zbytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           vector<Peer>& nodes) {
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
    zbytes tmp;
    if (!RepeatableToArray(result.nodes(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize offline lookup nodes");
      return false;
    }

    if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in offline lookup nodes");
      return false;
    }
  }

  return true;
}

bool Messenger::SetForwardTxnBlockFromSeed(
    zbytes& dst, const unsigned int offset,
    const std::vector<Transaction>& dsTransactions) {
  LookupForwardTxnsFromSeed result;

  if (!dsTransactions.empty()) {
    TransactionArrayToProtobuf(dsTransactions, *result.mutable_transactions());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupForwardTxnsFromSeed initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetForwardTxnBlockFromSeed(const zbytes& src,
                                           const unsigned int offset,
                                           vector<Transaction>& txnsContainer) {
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
  if (!ProtobufToTransactionArray(result.transactions(), txnsContainer)) {
    LOG_GENERAL(WARNING, "ProtobufToTransactionArray failed");
    return false;
  }

  return true;
}

// UNUSED
bool Messenger::SetLookupGetShardsFromSeed(zbytes& dst,
                                           const unsigned int offset,
                                           const uint32_t listenPort) {
  LookupGetShardsFromSeed result;

  result.set_listenport(listenPort);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetShardsFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

// UNUSED
bool Messenger::GetLookupGetShardsFromSeed(const zbytes& src,
                                           const unsigned int offset,
                                           uint32_t& listenPort) {
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
    zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
    const uint32_t& shardingStructureVersion,
    const DequeOfShardMembers& shards) {
  LookupSetShardsFromSeed result;

  ShardingStructureToProtobuf(shardingStructureVersion, shards,
                              *result.mutable_sharding());

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  zbytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure");
    return false;
  }

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetShardsFromSeed(const zbytes& src,
                                           const unsigned int offset,
                                           PubKey& lookupPubKey,
                                           uint32_t& shardingStructureVersion,
                                           DequeOfShardMembers& shards) {
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

  zbytes tmp;
  if (!SerializeToArray(result.sharding(), tmp, 0)) {
    LOG_GENERAL(WARNING, "Failed to serialize sharding structure");
    return false;
  }

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in sharding structure");
    return false;
  }

  return true;
}

bool Messenger::SetLookupGetMicroBlockFromLookup(
    zbytes& dst, const unsigned int offset,
    const vector<BlockHash>& microBlockHashes, const uint32_t portNo) {
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

bool Messenger::SetLookupGetMicroBlockFromL2l(
    zbytes& dst, const unsigned int offset,
    const vector<BlockHash>& microBlockHashes, uint32_t portNo,
    const PairOfKey& seedKey) {
  LookupGetMicroBlockFromL2l result;

  result.mutable_data()->set_portno(portNo);

  for (const auto& hash : microBlockHashes) {
    result.mutable_data()->add_mbhashes(hash.data(), hash.size);
  }

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetMicroBlockFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING,
                "Failed to sign LookupGetMicroBlockFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetMicroBlockFromL2l initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetMicroBlockFromL2l(
    const zbytes& src, const unsigned int offset,
    vector<BlockHash>& microBlockHashes, uint32_t& portNo,
    PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetMicroBlockFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetMicroBlockFromL2l initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "GetLookupGetMicroBlockFromL2l signature wrong");
    return false;
  }

  portNo = result.data().portno();

  for (const auto& hash : result.data().mbhashes()) {
    microBlockHashes.emplace_back();
    unsigned int size = min((unsigned int)hash.size(),
                            (unsigned int)microBlockHashes.back().size);
    copy(hash.begin(), hash.begin() + size,
         microBlockHashes.back().asArray().begin());
  }

  return true;
}

// UNUSED
bool Messenger::GetLookupGetMicroBlockFromLookup(
    const zbytes& src, const unsigned int offset,
    vector<BlockHash>& microBlockHashes, uint32_t& portNo) {
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
    zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
    const vector<MicroBlock>& mbs) {
  LOG_MARKER();
  LookupSetMicroBlockFromLookup result;

  for (const auto& mb : mbs) {
    io::MicroBlockToProtobuf(mb, *result.add_microblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.microblocks().size() > 0) {
    zbytes tmp;
    if (!RepeatableToArray(result.microblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks");
      return false;
    }

    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetLookupSetMicroBlockFromLookup(const zbytes& src,
                                                 const unsigned int offset,
                                                 PubKey& lookupPubKey,
                                                 vector<MicroBlock>& mbs) {
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
    zbytes tmp;
    if (!RepeatableToArray(result.microblocks(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize micro blocks");
      return false;
    }

    if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in micro blocks");
      return false;
    }
  }

  for (const auto& res_mb : result.microblocks()) {
    MicroBlock mb;

    io::ProtobufToMicroBlock(res_mb, mb);

    mbs.emplace_back(mb);
  }

  return true;
}

// UNUSED
bool Messenger::SetLookupGetTxnsFromLookup(zbytes& dst,
                                           const unsigned int offset,
                                           const BlockHash& mbHash,
                                           const vector<TxnHash>& txnhashes,
                                           const uint32_t portNo) {
  LookupGetTxnsFromLookup result;

  result.set_portno(portNo);
  result.set_mbhash(mbHash.data(), mbHash.size);

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
bool Messenger::GetLookupGetTxnsFromLookup(const zbytes& src,
                                           const unsigned int offset,
                                           BlockHash& mbHash,
                                           vector<TxnHash>& txnhashes,
                                           uint32_t& portNo) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetTxnsFromLookup result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromLookup initialization failure");
    return false;
  }

  portNo = result.portno();
  auto hash = result.mbhash();
  unsigned int size = min((unsigned int)hash.size(), (unsigned int)mbHash.size);
  copy(hash.begin(), hash.begin() + size, mbHash.asArray().begin());

  for (const auto& hash : result.txnhashes()) {
    txnhashes.emplace_back();
    size = min((unsigned int)hash.size(), (unsigned int)txnhashes.back().size);
    copy(hash.begin(), hash.begin() + size, txnhashes.back().asArray().begin());
  }
  return true;
}

bool Messenger::SetLookupGetTxnsFromL2l(zbytes& dst, const unsigned int offset,
                                        const BlockHash& mbHash,
                                        const vector<TxnHash>& txnhashes,
                                        const uint32_t portNo,
                                        const PairOfKey& seedKey) {
  LookupGetTxnsFromL2l result;

  result.mutable_data()->set_portno(portNo);
  result.mutable_data()->set_mbhash(mbHash.data(), mbHash.size);

  for (const auto& txhash : txnhashes) {
    result.mutable_data()->add_txnhashes(txhash.data(), txhash.size);
  }

  SerializableToProtobufByteArray(seedKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromL2l.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, seedKey.first, seedKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign LookupGetTxnsFromL2l request message");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromL2l initialization failure");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

// UNUSED
bool Messenger::GetLookupGetTxnsFromL2l(
    const zbytes& src, const unsigned int offset, BlockHash& mbHash,
    vector<TxnHash>& txnhashes, uint32_t& portNo, PubKey& senderPubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetTxnsFromL2l result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetTxnsFromL2l initialization failure");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "GetLookupGetTxnsFromL2l signature wrong");
    return false;
  }

  portNo = result.data().portno();
  auto hash = result.data().mbhash();
  unsigned int size = min((unsigned int)hash.size(), (unsigned int)mbHash.size);
  copy(hash.begin(), hash.begin() + size, mbHash.asArray().begin());

  for (const auto& hash : result.data().txnhashes()) {
    txnhashes.emplace_back();
    size = min((unsigned int)hash.size(), (unsigned int)txnhashes.back().size);
    copy(hash.begin(), hash.begin() + size, txnhashes.back().asArray().begin());
  }
  return true;
}

// UNUSED
bool Messenger::SetLookupSetTxnsFromLookup(
    zbytes& dst, const unsigned int offset, const PairOfKey& lookupKey,
    const BlockHash& mbHash, const vector<TransactionWithReceipt>& txns) {
  LookupSetTxnsFromLookup result;

  result.set_mbhash(mbHash.data(), mbHash.size);

  for (auto const& txn : txns) {
    SerializableToProtobufByteArray(txn, *result.add_transactions());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());
  Signature signature;
  if (result.transactions().size() > 0) {
    zbytes tmp;
    tmp.reserve(Transaction::AVERAGE_TXN_SIZE_BYTES *
                result.transactions_size());
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }

    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, PubKey& lookupPubKey,
    BlockHash& mbHash, vector<TransactionWithReceipt>& txns) {
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

  auto hash = result.mbhash();
  unsigned int size = min((unsigned int)hash.size(), (unsigned int)mbHash.size);
  copy(hash.begin(), hash.begin() + size, mbHash.asArray().begin());

  if (result.transactions().size() > 0) {
    zbytes tmp;
    tmp.reserve(Transaction::AVERAGE_TXN_SIZE_BYTES *
                result.transactions_size());
    if (!RepeatableToArray(result.transactions(), tmp, 0)) {
      LOG_GENERAL(WARNING, "Failed to serialize transactions");
      return false;
    }

    if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in transactions");
      return false;
    }
  }
  txns.reserve(result.transactions_size());
  for (auto const& protoTxn : result.transactions()) {
    TransactionWithReceipt txn;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(protoTxn, txn);
    txns.emplace_back(txn);
  }

  return true;
}

bool Messenger::SetLookupGetDirectoryBlocksFromSeed(
    zbytes& dst, const unsigned int offset, const uint32_t portNo,
    const uint64_t& indexNum, const bool includeMinerInfo) {
  LookupGetDirectoryBlocksFromSeed result;

  result.set_portno(portNo);
  result.set_indexnum(indexNum);
  result.set_includeminerinfo(includeMinerInfo);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDirectoryBlocksFromSeed(const zbytes& src,
                                                    const unsigned int offset,
                                                    uint32_t& portNo,
                                                    uint64_t& indexNum,
                                                    bool& includeMinerInfo) {
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
  includeMinerInfo = result.includeminerinfo();

  return true;
}

bool Messenger::SetLookupSetDirectoryBlocksFromSeed(
    zbytes& dst, const unsigned int offset,
    const uint32_t& shardingStructureVersion,
    const vector<boost::variant<DSBlock, VCBlock>>& directoryBlocks,
    const uint64_t& indexNum, const PairOfKey& lookupKey) {
  LookupSetDirectoryBlocksFromSeed result;

  result.mutable_data()->set_indexnum(indexNum);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  for (const auto& dirblock : directoryBlocks) {
    ProtoSingleDirectoryBlock* proto_dir_blocks =
        result.mutable_data()->add_dirblocks();
    if (dirblock.type() == typeid(DSBlock)) {
      io::DSBlockToProtobuf(get<DSBlock>(dirblock),
                            *proto_dir_blocks->mutable_dsblock());
    } else if (dirblock.type() == typeid(VCBlock)) {
      io::VCBlockToProtobuf(get<VCBlock>(dirblock),
                            *proto_dir_blocks->mutable_vcblock());
    }
  }

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset,
    uint32_t& shardingStructureVersion,
    vector<boost::variant<DSBlock, VCBlock>>& directoryBlocks,
    uint64_t& indexNum, PubKey& pubKey) {
  LookupSetDirectoryBlocksFromSeed result;

  google::protobuf::io::ArrayInputStream arrayIn(src.data() + offset,
                                                 src.size() - offset);
  google::protobuf::io::CodedInputStream codedIn(&arrayIn);
  codedIn.SetTotalBytesLimit(MAX_READ_WATERMARK_IN_BYTES);

  if (!result.ParseFromCodedStream(&codedIn) ||
      !codedIn.ConsumedEntireMessage() || !result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetDirectoryBlocksFromSeed initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), pubKey);

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, pubKey)) {
    LOG_GENERAL(WARNING,
                "Invalid signature in LookupSetDirectoryBlocksFromSeed");
    return false;
  }

  indexNum = result.data().indexnum();

  for (const auto& dirblock : result.data().dirblocks()) {
    DSBlock dsblock;
    VCBlock vcblock;
    switch (dirblock.directoryblock_case()) {
      case ProtoSingleDirectoryBlock::DirectoryblockCase::kDsblock:
        if (!dirblock.dsblock().IsInitialized()) {
          LOG_GENERAL(WARNING, "DS block not initialized");
          return false;
        }
        if (!io::ProtobufToDSBlock(dirblock.dsblock(), dsblock)) {
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
        if (!io::ProtobufToVCBlock(dirblock.vcblock(), vcblock)) {
          LOG_GENERAL(WARNING, "ProtobufToVCBlock failed");
          return false;
        }
        directoryBlocks.emplace_back(vcblock);
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

bool Messenger::SetConsensusCommit(zbytes& dst, const unsigned int offset,
                                   const uint32_t consensusID,
                                   const uint64_t blockNumber,
                                   const zbytes& blockHash,
                                   const uint16_t backupID,
                                   const vector<CommitInfo>& commitInfo,
                                   const PairOfKey& backupKey) {
  ConsensusCommit result;

  result.mutable_consensusinfo()->set_consensusid(consensusID);
  result.mutable_consensusinfo()->set_blocknumber(blockNumber);
  result.mutable_consensusinfo()->set_blockhash(blockHash.data(),
                                                blockHash.size());
  result.mutable_consensusinfo()->set_backupid(backupID);

  for (const auto& info : commitInfo) {
    ConsensusCommit::CommitInfo* ci =
        result.mutable_consensusinfo()->add_commitinfo();

    SerializableToProtobufByteArray(info.commit, *ci->mutable_commitpoint());
    SerializableToProtobufByteArray(info.hash, *ci->mutable_commitpointhash());
  }

  if (!result.consensusinfo().IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCommit.Data initialization failed");
    return false;
  }

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, backupKey.first, backupKey.second, signature)) {
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

bool Messenger::GetConsensusCommit(const zbytes& src, const unsigned int offset,
                                   const uint32_t consensusID,
                                   const uint64_t blockNumber,
                                   const zbytes& blockHash, uint16_t& backupID,
                                   vector<CommitInfo>& commitInfo,
                                   const DequeOfNode& committeeKeys) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  for (const auto& proto_ci : result.consensusinfo().commitinfo()) {
    CommitInfo ci;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_ci.commitpoint(), ci.commit);
    PROTOBUFBYTEARRAYTOSERIALIZABLE(proto_ci.commitpointhash(), ci.hash);

    commitInfo.emplace_back(ci);
  }

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusChallenge(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const vector<ChallengeSubsetInfo>& subsetInfo,
    const PairOfKey& leaderKey) {
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, leaderKey.first, leaderKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, vector<ChallengeSubsetInfo>& subsetInfo,
    const PubKey& leaderKey) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in challenge");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusResponse(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t backupID, const vector<ResponseSubsetInfo>& subsetInfo,
    const PairOfKey& backupKey) {
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, backupKey.first, backupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash, uint16_t& backupID,
    vector<ResponseSubsetInfo>& subsetInfo, const DequeOfNode& committeeKeys) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in response");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusCollectiveSig(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const Signature& collectiveSig,
    const vector<bool>& bitmap, const PairOfKey& leaderKey,
    const zbytes& newAnnouncementMessage) {
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, leaderKey.first, leaderKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign collectivesig");
    return false;
  }

  SerializableToProtobufByteArray(leaderKey.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!newAnnouncementMessage.empty()) {
    result.set_newannouncement(newAnnouncementMessage.data(),
                               newAnnouncementMessage.size());

    Signature finalsignature;
    if (!Schnorr::Sign(newAnnouncementMessage, leaderKey.first,
                       leaderKey.second, finalsignature)) {
      LOG_GENERAL(WARNING, "Failed to sign new announcement");
      return false;
    }

    SerializableToProtobufByteArray(finalsignature,
                                    *result.mutable_finalsignature());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ConsensusCollectiveSig initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetConsensusCollectiveSig(
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, vector<bool>& bitmap, Signature& collectiveSig,
    const PubKey& leaderKey, zbytes& newAnnouncement) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in collectivesig");
    return false;
  }

  if (!result.newannouncement().empty()) {
    newAnnouncement.resize(result.newannouncement().size());
    std::copy(result.newannouncement().begin(), result.newannouncement().end(),
              newAnnouncement.begin());

    Signature finalsignature;

    PROTOBUFBYTEARRAYTOSERIALIZABLE(result.finalsignature(), finalsignature);

    if (!Schnorr::Verify(newAnnouncement, finalsignature, leaderKey)) {
      LOG_GENERAL(WARNING, "Invalid signature in new announcement. leaderID = "
                               << leaderID << " leaderKey = " << leaderKey);
      return false;
    }
  }

  return true;
}

bool Messenger::SetConsensusCommitFailure(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t backupID, const zbytes& errorMsg,
    const PairOfKey& backupKey) {
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, backupKey.first, backupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash, uint16_t& backupID,
    zbytes& errorMsg, const DequeOfNode& committeeKeys) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, committeeKeys.at(backupID).first)) {
    LOG_GENERAL(WARNING, "Invalid signature in commit failure");
    return false;
  }

  return true;
}

bool Messenger::SetConsensusConsensusFailure(
    zbytes& dst, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash,
    const uint16_t leaderID, const PairOfKey& leaderKey) {
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  if (!Schnorr::Sign(tmp, leaderKey.first, leaderKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, const uint32_t consensusID,
    const uint64_t blockNumber, const zbytes& blockHash, uint16_t& leaderID,
    const PubKey& leaderKey) {
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
    zbytes remoteBlockHash(tmpBlockHash.size());
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

  zbytes tmp(result.consensusinfo().ByteSizeLong());
  result.consensusinfo().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, leaderKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in ConsensusConsensusFailure");
    return false;
  }

  return true;
}

// ============================================================================
// View change pre check messages
// ============================================================================

bool Messenger::SetLookupGetDSTxBlockFromSeed(
    zbytes& dst, const unsigned int offset, const uint64_t dsLowBlockNum,
    const uint64_t dsHighBlockNum, const uint64_t txLowBlockNum,
    const uint64_t txHighBlockNum, const uint32_t listenPort) {
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
    const zbytes& src, const unsigned int offset, uint64_t& dsLowBlockNum,
    uint64_t& dsHighBlockNum, uint64_t& txLowBlockNum, uint64_t& txHighBlockNum,
    uint32_t& listenPort) {
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

bool Messenger::SetVCNodeSetDSTxBlockFromSeed(zbytes& dst,
                                              const unsigned int offset,
                                              const PairOfKey& lookupKey,
                                              const vector<DSBlock>& DSBlocks,
                                              const vector<TxBlock>& txBlocks) {
  VCNodeSetDSTxBlockFromSeed result;

  for (const auto& dsblock : DSBlocks) {
    io::DSBlockToProtobuf(dsblock, *result.mutable_data()->add_dsblocks());
  }

  for (const auto& txblock : txBlocks) {
    io::TxBlockToProtobuf(txblock, *result.mutable_data()->add_txblocks());
  }

  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "VCNodeSetDSTxBlockFromSeed.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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

bool Messenger::GetVCNodeSetDSTxBlockFromSeed(const zbytes& src,
                                              const unsigned int offset,
                                              vector<DSBlock>& dsBlocks,
                                              vector<TxBlock>& txBlocks,
                                              PubKey& lookupPubKey) {
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
    if (!io::ProtobufToDSBlock(proto_dsblock, dsblock)) {
      LOG_GENERAL(WARNING, "ProtobufToDSBlock failed");
      return false;
    }
    dsBlocks.emplace_back(dsblock);
  }

  for (const auto& txblock : result.data().txblocks()) {
    TxBlock block;
    if (!io::ProtobufToTxBlock(txblock, block)) {
      LOG_GENERAL(WARNING, "ProtobufToTxBlock failed");
      return false;
    }
    txBlocks.emplace_back(block);
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), lookupPubKey);

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  if (!Schnorr::Verify(tmp, signature, lookupPubKey)) {
    LOG_GENERAL(WARNING, "Invalid signature in VCNodeSetDSTxBlockFromSeed");
    return false;
  }

  return true;
}

bool Messenger::SetNodeNewShardNodeNetworkInfo(
    zbytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
    const Peer& shardNodeNewNetworkInfo, const uint64_t timestamp,
    const PairOfKey& shardNodeKey) {
  LOG_MARKER();
  NodeSetShardNodeNetworkInfoUpdate result;

  result.mutable_data()->set_dsepochnumber(dsEpochNumber);
  SerializableToProtobufByteArray(
      shardNodeKey.second, *result.mutable_data()->mutable_shardnodepubkey());
  PeerToProtobuf(shardNodeNewNetworkInfo,
                 *result.mutable_data()->mutable_shardnodenewnetworkinfo());
  result.mutable_data()->set_timestamp(timestamp);

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetShardNodeNetworkInfoUpdate.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, shardNodeKey.first, shardNodeKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign shard node identity update");
    return false;
  }

  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetShardNodeNetworkInfoUpdate initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeNewShardNodeNetworkInfo(const zbytes& src,
                                               const unsigned int offset,
                                               uint64_t& dsEpochNumber,
                                               Peer& shardNodeNewNetworkInfo,
                                               uint64_t& timestamp,
                                               PubKey& shardNodePubKey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeSetShardNodeNetworkInfoUpdate result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "NodeSetShardNodeNetworkInfoUpdate initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.data().shardnodepubkey(),
                                  shardNodePubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, shardNodePubKey)) {
    LOG_GENERAL(WARNING, "NodeSetShardNodeNetworkInfoUpdate signature wrong");
    return false;
  }

  // Deserialize the remaining fields
  dsEpochNumber = result.data().dsepochnumber();
  ProtobufToPeer(result.data().shardnodenewnetworkinfo(),
                 shardNodeNewNetworkInfo);
  timestamp = result.data().timestamp();

  return true;
}

bool Messenger::SetDSLookupNewDSGuardNetworkInfo(
    zbytes& dst, const unsigned int offset, const uint64_t dsEpochNumber,
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

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, dsguardkey.first, dsguardkey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, uint64_t& dsEpochNumber,
    Peer& dsGuardNewNetworkInfo, uint64_t& timestamp, PubKey& dsGuardPubkey) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, dsGuardPubkey)) {
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
    zbytes& dst, const unsigned int offset, const uint32_t portNo,
    const uint64_t dsEpochNumber, const PairOfKey& lookupKey) {
  NodeGetGuardNodeNetworkInfoUpdate result;
  result.mutable_data()->set_portno(portNo);
  result.mutable_data()->set_dsepochnumber(dsEpochNumber);
  SerializableToProtobufByteArray(lookupKey.second, *result.mutable_pubkey());

  if (result.data().IsInitialized()) {
    zbytes tmp(result.data().ByteSizeLong());
    result.data().SerializeToArray(tmp.data(), tmp.size());

    Signature signature;
    if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset, uint32_t& portNo,
    uint64_t& dsEpochNumber) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "DSMicroBlockSubmission signature wrong");
    return false;
  }

  portNo = result.data().portno();
  dsEpochNumber = result.data().dsepochnumber();

  return true;
}

bool Messenger::SetNodeSetNewDSGuardNetworkInfo(
    zbytes& dst, unsigned int offset,
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  Signature signature;
  if (!Schnorr::Sign(tmp, lookupKey.first, lookupKey.second, signature)) {
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
    const zbytes& src, const unsigned int offset,
    vector<DSGuardUpdateStruct>& vecOfDSGuardUpdateStruct,
    PubKey& lookupPubKey) {
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
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, lookupPubKey)) {
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

bool Messenger::SetNodeRemoveFromBlacklist(zbytes& dst,
                                           const unsigned int offset,
                                           const PairOfKey& myKey,
                                           const uint128_t& ipAddress,
                                           const uint64_t& dsEpochNumber) {
  NodeRemoveFromBlacklist result;
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      ipAddress, *result.mutable_data()->mutable_ipaddress());
  result.mutable_data()->set_dsepochnumber(dsEpochNumber);

  SerializableToProtobufByteArray(myKey.second, *result.mutable_pubkey());

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeRemoveFromBlacklist.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  Signature signature;
  if (!Schnorr::Sign(tmp, myKey.first, myKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign NodeRemoveFromBlacklist");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeRemoveFromBlacklist initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetNodeRemoveFromBlacklist(const zbytes& src,
                                           const unsigned int offset,
                                           PubKey& senderPubKey,
                                           uint128_t& ipAddress,
                                           uint64_t& dsEpochNumber) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  NodeRemoveFromBlacklist result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "NodeRemoveFromBlacklist initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "NodeRemoveFromBlacklist signature wrong");
    return false;
  }

  ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(result.data().ipaddress(),
                                                     ipAddress);
  dsEpochNumber = result.data().dsepochnumber();
  return true;
}

bool Messenger::SetLookupGetCosigsRewardsFromSeed(zbytes& dst,
                                                  const unsigned int offset,
                                                  const uint64_t txBlkNum,
                                                  const uint32_t listenPort,
                                                  const PairOfKey& keys) {
  LookupGetCosigsRewardsFromSeed result;

  result.mutable_data()->set_epochnumber(txBlkNum);
  result.mutable_data()->set_portno(listenPort);

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetCosigsRewardsFromSeed.Data initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, keys.first, keys.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign GetCosigsRewardsFromSeed message");
    return false;
  }

  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupGetCosigsRewardsFromSeed initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetCosigsRewardsFromSeed(const zbytes& src,
                                                  const unsigned int offset,
                                                  PubKey& senderPubKey,
                                                  uint64_t& txBlockNumber,
                                                  uint32_t& port) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupGetCosigsRewardsFromSeed result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "LookupGetCosigRewardsFromSeed initialization failed");
    return false;
  }

  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubKey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubKey)) {
    LOG_GENERAL(WARNING, "LookupGetCosigRewardsFromSeed signature wrong");
    return false;
  }

  txBlockNumber = result.data().epochnumber();
  port = result.data().portno();
  return true;
}

bool Messenger::SetLookupGetDSLeaderTxnPool(zbytes& dst, unsigned int offset,
                                            const PairOfKey& keys,
                                            uint32_t listenPort) {
  LookupGetDSLeaderTxnPool result;

  result.mutable_data()->set_portno(listenPort);

  Signature signature;
  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "SetLookupGetDSLeaderTxnPool initialization failed");
    return false;
  }
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());

  if (!Schnorr::Sign(tmp, keys.first, keys.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign SetLookupGetDSLeaderTxnPool message");
    return false;
  }

  SerializableToProtobufByteArray(keys.second, *result.mutable_pubkey());
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SetLookupGetDSLeaderTxnPool initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupGetDSLeaderTxnPool(const zbytes& src,
                                            unsigned int offset,
                                            PubKey& senderPubkey,
                                            uint32_t& listenPort) {
  LookupGetDSLeaderTxnPool result;

  result.ParseFromArray(src.data() + offset, src.size() - offset);
  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING, "GetLookupGetDSLeaderTxnPool initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubkey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubkey)) {
    LOG_GENERAL(WARNING, "GetLookupGetDSLeaderTxnPool signature wrong");
    return false;
  }

  listenPort = result.data().portno();
  return true;
}

bool Messenger::SetLookupSetDSLeaderTxnPool(
    zbytes& dst, unsigned int offset,
    const std::vector<Transaction>& transactions) {
  LookupSetDSLeaderTxnPool result;

  TransactionArrayToProtobuf(transactions, *result.mutable_dsleadertxnpool());
  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "SetLookupSetDSLeaderTxnPool initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetDSLeaderTxnPool(
    const zbytes& src, unsigned int offset,
    std::vector<Transaction>& transactions) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetDSLeaderTxnPool result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "GetLookupSetDSLeaderTxnPool initialization failed");
    return false;
  }

  if (!ProtobufToTransactionArray(result.dsleadertxnpool(), transactions)) {
    LOG_GENERAL(WARNING, "ProtobufToTransactionArray failed");
    return false;
  }

  return true;
}

bool Messenger::SetLookupSetCosigsRewardsFromSeed(
    zbytes& dst, const unsigned int offset, const PairOfKey& myKey,
    const uint64_t& txBlkNumber, const std::vector<MicroBlock>& microblocks,
    const TxBlock& txBlock, const uint32_t& numberOfShards) {
  LookupSetCosigsRewardsFromSeed result;

  // For Non-DS Shard
  for (const auto& mb : microblocks) {
    if (mb.GetHeader().GetShardId() == numberOfShards) {
      continue;  // use txBlk for ds shard
    }
    ProtoCosigsRewardsStructure* proto_CosigsRewardsStructure =
        result.mutable_data()->add_cosigsrewards();

    // txblock and shardid
    proto_CosigsRewardsStructure->set_epochnumber(txBlkNumber);
    proto_CosigsRewardsStructure->set_shardid(mb.GetHeader().GetShardId());

    ZilliqaMessage::ProtoBlockBase* protoBlockBase =
        proto_CosigsRewardsStructure->mutable_blockbase();

    // cosigs
    io::BlockBaseToProtobuf(mb, *protoBlockBase);

    // rewards
    NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
        mb.GetHeader().GetRewards(),
        *proto_CosigsRewardsStructure->mutable_rewards());
  }

  // For DS Shard
  ProtoCosigsRewardsStructure* proto_CosigsRewardsStructure =
      result.mutable_data()->add_cosigsrewards();

  // txblock and shardid
  proto_CosigsRewardsStructure->set_epochnumber(txBlkNumber + 1);
  proto_CosigsRewardsStructure->set_shardid(-1);

  ZilliqaMessage::ProtoBlockBase* protoBlockBase =
      proto_CosigsRewardsStructure->mutable_blockbase();

  // cosigs
  io::BlockBaseToProtobuf(txBlock, *protoBlockBase);

  // rewards
  NumberToProtobufByteArray<uint128_t, UINT128_SIZE>(
      txBlock.GetHeader().GetRewards(),
      *proto_CosigsRewardsStructure->mutable_rewards());

  SerializableToProtobufByteArray(myKey.second, *result.mutable_pubkey());

  if (!result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetCosigsRewardsFromSeed.Data initialization failed");
    return false;
  }

  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  Signature signature;
  if (!Schnorr::Sign(tmp, myKey.first, myKey.second, signature)) {
    LOG_GENERAL(WARNING, "Failed to sign LookupSetCosigsRewardsFromSeed");
    return false;
  }
  SerializableToProtobufByteArray(signature, *result.mutable_signature());

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetCosigsRewardsFromSeed initialization failed");
    return false;
  }
  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetLookupSetCosigsRewardsFromSeed(
    const zbytes& src, const unsigned int offset,
    vector<CoinbaseStruct>& cosigrewards, PubKey& senderPubkey) {
  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  LookupSetCosigsRewardsFromSeed result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized() || !result.data().IsInitialized()) {
    LOG_GENERAL(WARNING,
                "LookupSetCosigsRewardsFromSeed initialization failed");
    return false;
  }

  // First deserialize the fields needed just for signature check
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.pubkey(), senderPubkey);
  Signature signature;
  PROTOBUFBYTEARRAYTOSERIALIZABLE(result.signature(), signature);

  // Check signature
  zbytes tmp(result.data().ByteSizeLong());
  result.data().SerializeToArray(tmp.data(), tmp.size());
  if (!Schnorr::Verify(tmp, 0, tmp.size(), signature, senderPubkey)) {
    LOG_GENERAL(WARNING, "LookupSetCosigsRewardsFromSeed signature wrong");
    return false;
  }

  // Deserialize the remaining fields
  int32_t shardId;
  uint64_t txBlkNum;
  uint128_t rewards;
  cosigrewards.clear();

  for (const auto& proto_cosigrewards : result.data().cosigsrewards()) {
    txBlkNum = proto_cosigrewards.epochnumber();
    shardId = proto_cosigrewards.shardid();
    ProtobufByteArrayToNumber<uint128_t, UINT128_SIZE>(
        proto_cosigrewards.rewards(), rewards);

    auto blockBaseVars =
        io::ProtobufToBlockBase(proto_cosigrewards.blockbase());
    if (!blockBaseVars) {
      LOG_GENERAL(WARNING, "ProtobufToBlockBase failed");
      return false;
    }

    const auto& [blockHash, coSigs, timestamp] = *blockBaseVars;
    cosigrewards.emplace_back(
        CoinbaseStruct(txBlkNum, shardId, coSigs.m_B1, coSigs.m_B2, rewards));
    LOG_GENERAL(INFO, "Received cosig and rewards for epoch "
                          << txBlkNum << ", shard " << shardId);
  }

  return true;
}

bool Messenger::SetMinerInfoDSComm(zbytes& dst, const unsigned int offset,
                                   const MinerInfoDSComm& minerInfo) {
  ProtoMinerInfoDSComm result;

  for (const auto& dsnode : minerInfo.m_dsNodes) {
    ProtoMinerInfoDSComm::Node* protodsnode = result.add_dsnodes();
    SerializableToProtobufByteArray(dsnode, *protodsnode->mutable_pubkey());
  }

  for (const auto& dsnode : minerInfo.m_dsNodesEjected) {
    ProtoMinerInfoDSComm::Node* protodsnode = result.add_dsnodesejected();
    SerializableToProtobufByteArray(dsnode, *protodsnode->mutable_pubkey());
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMinerInfoDSComm initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMinerInfoDSComm(const zbytes& src, const unsigned int offset,
                                   MinerInfoDSComm& minerInfo) {
  minerInfo.m_dsNodes.clear();
  minerInfo.m_dsNodesEjected.clear();

  if (src.size() == 0) {
    LOG_GENERAL(INFO, "Empty MinerInfoDSComm");
    return true;
  }

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoMinerInfoDSComm result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMinerInfoDSComm initialization failed");
    return false;
  }

  for (const auto& protodsnode : result.dsnodes()) {
    PubKey pubkey;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(protodsnode.pubkey(), pubkey);
    minerInfo.m_dsNodes.emplace_back(pubkey);
  }

  for (const auto& protodsnode : result.dsnodesejected()) {
    PubKey pubkey;
    PROTOBUFBYTEARRAYTOSERIALIZABLE(protodsnode.pubkey(), pubkey);
    minerInfo.m_dsNodesEjected.emplace_back(pubkey);
  }

  return true;
}

bool Messenger::SetMinerInfoShards(zbytes& dst, const unsigned int offset,
                                   const MinerInfoShards& minerInfo) {
  ProtoMinerInfoShards result;

  for (const auto& shard : minerInfo.m_shards) {
    ProtoMinerInfoShards::Shard* protoshard = result.add_shards();
    protoshard->set_shardsize(shard.m_shardSize);
    for (const auto& shardnode : shard.m_shardNodes) {
      ProtoMinerInfoShards::Node* protoshardnode = protoshard->add_shardnodes();
      SerializableToProtobufByteArray(shardnode,
                                      *protoshardnode->mutable_pubkey());
    }
  }

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMinerInfoShards initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMinerInfoShards(const zbytes& src, const unsigned int offset,
                                   MinerInfoShards& minerInfo) {
  minerInfo.m_shards.clear();

  if (src.size() == 0) {
    LOG_GENERAL(INFO, "Empty MinerInfoShards");
    return true;
  }

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoMinerInfoShards result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMinerInfoShards initialization failed");
    return false;
  }

  for (const auto& protoshard : result.shards()) {
    MinerInfoShards::MinerInfoShard shard;
    shard.m_shardSize = protoshard.shardsize();
    for (const auto& protoshardnode : protoshard.shardnodes()) {
      PubKey pubkey;
      PROTOBUFBYTEARRAYTOSERIALIZABLE(protoshardnode.pubkey(), pubkey);
      shard.m_shardNodes.emplace_back(pubkey);
    }
    minerInfo.m_shards.emplace_back(shard);
  }

  return true;
}

bool Messenger::SetMicroBlockKey(zbytes& dst, const unsigned int offset,
                                 const uint64_t& epochNum,
                                 const uint32_t& shardID) {
  ProtoMicroBlockKey result;
  result.set_epochnum(epochNum);
  result.set_shardid(shardID);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlockKey initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetMicroBlockKey(const zbytes& src, const unsigned int offset,
                                 uint64_t& epochNum, uint32_t& shardID) {
  if (src.size() == 0) {
    LOG_GENERAL(INFO, "Empty ProtoMicroBlockKey");
    return false;
  }

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoMicroBlockKey result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoMicroBlockKey initialization failed");
    return false;
  }

  epochNum = result.epochnum();
  shardID = result.shardid();

  return true;
}

bool Messenger::SetTxEpoch(zbytes& dst, const unsigned int offset,
                           const uint64_t& epochNum) {
  ProtoTxEpoch result;
  result.set_epochnum(epochNum);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxEpoch initialization failed");
    return false;
  }

  return SerializeToArray(result, dst, offset);
}

bool Messenger::GetTxEpoch(const zbytes& src, const unsigned int offset,
                           uint64_t& epochNum) {
  if (src.size() == 0) {
    LOG_GENERAL(INFO, "Empty TxEpoch");
    return true;
  }

  if (offset >= src.size()) {
    LOG_GENERAL(WARNING, "Invalid data and offset, data size "
                             << src.size() << ", offset " << offset);
    return false;
  }

  ProtoTxEpoch result;
  result.ParseFromArray(src.data() + offset, src.size() - offset);

  if (!result.IsInitialized()) {
    LOG_GENERAL(WARNING, "ProtoTxEpoch initialization failed");
    return false;
  }

  epochNum = result.epochnum();

  return true;
}
