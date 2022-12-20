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

#include "StatusServer.h"
#include "JSONConversion.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Blacklist.h"
#include "libPersistence/BlockStorage.h"
#include "libRemoteStorageDB/RemoteStorageDB.h"

using namespace jsonrpc;
using namespace std;

StatusServer::StatusServer(Mediator& mediator,
                           jsonrpc::AbstractServerConnector& server)
    : Server(mediator),
      jsonrpc::AbstractServer<StatusServer>(server,
                                            jsonrpc::JSONRPC_SERVER_V2) {
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetCurrentMiniEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetCurrentMiniEpochI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("GetCurrentDSEpoch", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetCurrentDSEpochI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNodeType", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &Server::GetNodeTypeI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("GetNodeState", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetNodeStateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("IsTxnInMemPool", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::IsTxnInMemPoolI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("AddToBlacklistExclusion", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::AddToBlacklistExclusionI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("RemoveFromBlacklistExclusion",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_BOOLEAN,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &StatusServer::RemoveFromBlacklistExclusionI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("AddToExtSeedWhitelist", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::AddToExtSeedWhitelistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("RemoveFromExtSeedWhitelist",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_BOOLEAN,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &StatusServer::RemoveFromExtSeedWhitelistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetWhitelistedExtSeed", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetWhitelistedExtSeedI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("AddToSeedsWhitelist", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::AddToSeedsWhitelistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("RemoveFromSeedsWhitelist",
                         jsonrpc::PARAMS_BY_POSITION, jsonrpc::JSON_BOOLEAN,
                         "param01", jsonrpc::JSON_STRING, NULL),
      &StatusServer::RemoveFromSeedsWhitelistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("IsIPInBlacklist", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::IsIPInBlacklistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("RemoveIPFromBlacklist", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::RemoveIPFromBlacklistI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetDSCommittee", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::GetDSCommitteeI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("healthcheck", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetHealthI);
  this->bindAndAddMethod(jsonrpc::Procedure("GetLatestEpochStatesUpdated",
                                            jsonrpc::PARAMS_BY_POSITION,
                                            jsonrpc::JSON_STRING, NULL),
                         &StatusServer::GetLatestEpochStatesUpdatedI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetEpochFin", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetEpochFinI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetPrevDSDifficulty", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &Server::GetPrevDSDifficultyI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetPrevDifficulty", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_INTEGER, NULL),
      &Server::GetPrevDifficultyI);

  this->bindAndAddMethod(
      jsonrpc::Procedure("ToggleSendSCCallsToDS", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::ToggleSendSCCallsToDSI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSendSCCallsToDS", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetSendSCCallsToDSI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("ToggleSendAllToDS", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::ToggleSendAllToDSI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetSendAllToDS", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetSendAllToDSI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("DisablePoW", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::DisablePoWI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("ToggleDisableTxns", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::ToggleDisableTxnsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("SetValidateDB", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::SetValidateDBI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetValidateDB", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetValidateDBI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("SetVoteInPow", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_BOOLEAN, NULL),
      &StatusServer::SetVoteInPowI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("ToggleRemoteStorage", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::ToggleRemoteStorageI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetRemoteStorage", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::GetRemoteStorageI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("InitRemoteStorage", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::InitRemoteStorageI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("GetAverageBlockTime", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_STRING, NULL),
      &StatusServer::GetAverageBlockTimeI);
  this->bindAndAddMethod(jsonrpc::Procedure("ToggleGetSmartContractState",
                                            jsonrpc::PARAMS_BY_POSITION,
                                            jsonrpc::JSON_OBJECT, NULL),
                         &StatusServer::ToggleGetSmartContractStateI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("AuditShard", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_STRING,
                         NULL),
      &StatusServer::AuditShardI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("ToggleGetPendingTxns", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::ToggleGetPendingTxnsI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("EnableJsonRpcPort", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::EnableJsonRpcPortI);
  this->bindAndAddMethod(
      jsonrpc::Procedure("DisableJsonRpcPort", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::DisableJsonRpcPortI);
}

string StatusServer::GetLatestEpochStatesUpdated() {
  LOG_MARKER();
  uint64_t epochNum;
  if (!BlockStorage::GetBlockStorage().GetLatestEpochStatesUpdated(epochNum)) {
    return "";
  }
  return to_string(epochNum);
}

string StatusServer::GetEpochFin() {
  // LOG_MARKER();
  uint64_t epochNum;
  if (!BlockStorage::GetBlockStorage().GetEpochFin(epochNum)) {
    return "";
  }
  return to_string(epochNum);
}

Json::Value StatusServer::GetDSCommittee() {
  if (m_mediator.m_DSCommittee == NULL) {
    throw JsonRpcException(RPC_INTERNAL_ERROR, "DS Committee empty");
  }

  lock_guard<mutex> g(m_mediator.m_mutexDSCommittee);

  const DequeOfNode& dq = *m_mediator.m_DSCommittee;

  return JSONConversion::convertDequeOfNode(dq);
}

string StatusServer::GetHealth() { return "ok"; }

bool StatusServer::AddToBlacklistExclusion(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().Whitelist(numIP)) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Could not add IP Address in exclusion list, already present");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::AddToExtSeedWhitelist(const string& pubKeyStr) {
  try {
    PubKey pubKey = PubKey::GetPubKeyFromString(pubKeyStr);

    if (!m_mediator.m_lookup->AddToWhitelistExtSeed(pubKey)) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Could not add pub key in extseed whitelist, already present");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::RemoveFromExtSeedWhitelist(const string& pubKeyStr) {
  try {
    PubKey pubKey = PubKey::GetPubKeyFromString(pubKeyStr);

    if (!m_mediator.m_lookup->RemoveFromWhitelistExtSeed(pubKey)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Could not remove pub key in extseed whitelist, "
                             "already not present");
    }
    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

string StatusServer::GetWhitelistedExtSeed() {
  try {
    std::unordered_set<PubKey> extSeedsWhitelisted;
    if (!BlockStorage::GetBlockStorage().GetAllExtSeedPubKeys(
            extSeedsWhitelisted)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Could not get pub key in extseed whitelist");
    }
    string result;
    for (const auto& pubk : extSeedsWhitelisted) {
      result += string(pubk);
      result += ", ";
    }
    result.erase(result.find_last_of(','));
    return result;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::AddToSeedsWhitelist(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().IsEnabled()) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Whitelisting is disabled. Node might not be synced yet!");
    }

    if (!Blacklist::GetInstance().WhitelistSeed(numIP)) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Could not add IP Address in whitelisted seed list, already present");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::RemoveFromSeedsWhitelist(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().RemoveFromWhitelistedSeeds(numIP)) {
      throw JsonRpcException(
          RPC_INVALID_PARAMETER,
          "Could not remove IP Address from whitelisted seed list");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::IsIPInBlacklist(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    return Blacklist::GetInstance().Exist(numIP);
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::RemoveIPFromBlacklist(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    Blacklist::GetInstance().Remove(numIP);
    return true;
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

bool StatusServer::RemoveFromBlacklistExclusion(const string& ipAddr) {
  try {
    uint128_t numIP;

    if (!IPConverter::ToNumericalIPFromStr(ipAddr, numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "IP Address provided not valid");
    }

    if (!Blacklist::GetInstance().RemoveFromWhitelist(numIP)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Could not remove IP Address from exclusion list");
    }

    return true;

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
}

string StatusServer::GetNodeState() {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  if (m_mediator.m_ds->m_mode == DirectoryService::Mode::IDLE) {
    return m_mediator.m_node->GetStateString();
  } else {
    return m_mediator.m_ds->GetStateString();
  }
}

Json::Value StatusServer::IsTxnInMemPool(const string& tranID) {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  try {
    if (tranID.size() != TRAN_HASH_SIZE * 2) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Txn Hash size not appropriate");
    }

    TxnHash tranHash(tranID);
    Json::Value _json;

    const auto& code = m_mediator.m_node->IsTxnInMemPool(tranHash);

    if (!IsTxnDropped(code)) {
      switch (code) {
        case TxnStatus::NOT_PRESENT:
          _json["present"] = false;
          _json["pending"] = false;
          _json["code"] = TxnStatus::NOT_PRESENT;
          return _json;
        case TxnStatus::PRESENT_NONCE_HIGH:
          _json["present"] = true;
          _json["pending"] = true;
          _json["code"] = TxnStatus::PRESENT_NONCE_HIGH;
          return _json;
        case TxnStatus::PRESENT_GAS_EXCEEDED:
          _json["present"] = true;
          _json["pending"] = true;
          _json["code"] = TxnStatus::PRESENT_GAS_EXCEEDED;
          return _json;
        case TxnStatus::ERROR:
          throw JsonRpcException(RPC_INTERNAL_ERROR, "Processing transactions");
        default:
          throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
      }
    } else {
      _json["present"] = true;
      _json["pending"] = false;
      _json["code"] = code;
      return _json;
    }

  } catch (const JsonRpcException& je) {
    throw je;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "[Error]" << e.what() << " Input " << tranID);
    throw JsonRpcException(RPC_MISC_ERROR,
                           string("Unable To Process: ") + e.what());
  }
}

bool StatusServer::ToggleSendSCCallsToDS() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  m_mediator.m_lookup->m_sendSCCallsToDS =
      !(m_mediator.m_lookup->m_sendSCCallsToDS);
  return m_mediator.m_lookup->m_sendSCCallsToDS;
}

bool StatusServer::GetSendSCCallsToDS() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  return m_mediator.m_lookup->m_sendSCCallsToDS;
}

bool StatusServer::ToggleSendAllToDS() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  m_mediator.m_lookup->m_sendAllToDS = !(m_mediator.m_lookup->m_sendAllToDS);
  return m_mediator.m_lookup->m_sendAllToDS;
}

bool StatusServer::GetSendAllToDS() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  return m_mediator.m_lookup->m_sendAllToDS;
}

bool StatusServer::DisablePoW() {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  m_mediator.m_disablePoW = true;
  return true;
}

bool StatusServer::DisableJsonRpcPort() {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on other than lookups");
  }
  return m_mediator.m_lookup->StopJsonRpcPort();
}

bool StatusServer::EnableJsonRpcPort() {
  LOG_MARKER();
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on other than lookups");
  }
  return m_mediator.m_lookup->StartJsonRpcPort();
}

bool StatusServer::ToggleDisableTxns() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  m_mediator.m_disableTxns = !m_mediator.m_disableTxns;
  return m_mediator.m_disableTxns;
}

bool StatusServer::ToggleRemoteStorage() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  REMOTESTORAGE_DB_ENABLE = !REMOTESTORAGE_DB_ENABLE;

  return REMOTESTORAGE_DB_ENABLE;
}

bool StatusServer::GetRemoteStorage() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  return REMOTESTORAGE_DB_ENABLE;
}

string StatusServer::SetValidateDB() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  string result = "";
  try {
    switch (m_mediator.m_validateState) {
      case ValidateState::IDLE:
      case ValidateState::DONE:
      case ValidateState::ERROR:
        if (m_mediator.m_lookup->GetSyncType() != SyncType::NO_SYNC) {
          result = "Validation aborted - node not synced";
        } else {
          m_mediator.m_node->ValidateDB();
          result = "Validation started";
        }
        break;
      case ValidateState::INPROGRESS:
      default:
        result = "Validation in progress";
        break;
    }
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }

  return result;
}

string StatusServer::GetValidateDB() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  string result = "";
  try {
    switch (m_mediator.m_validateState) {
      case ValidateState::IDLE:
        result = "Validation idle";
        break;
      case ValidateState::INPROGRESS:
        result = "Validation in progress";
        break;
      case ValidateState::DONE:
        result = "Validation completed successfully";
        break;
      case ValidateState::ERROR:
      default:
        result = "Validation completed with errors";
        break;
    }
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }

  return result;
}

bool StatusServer::SetVoteInPow(const std::string& proposalId,
                                const std::string& voteValue,
                                const std::string& remainingVoteCount,
                                const std::string& startDSEpoch,
                                const std::string& endDSEpoch) {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  if (proposalId.empty() || voteValue.empty() || remainingVoteCount.empty() ||
      startDSEpoch.empty() || endDSEpoch.empty()) {
    return false;
  }
  try {
    if (!m_mediator.m_node->StoreVoteUntilPow(proposalId, voteValue,
                                              remainingVoteCount, startDSEpoch,
                                              endDSEpoch)) {
      throw JsonRpcException(RPC_INVALID_PARAMETER,
                             "Invalid request parameters");
    }
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error]: " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }
  return true;
}
bool StatusServer::InitRemoteStorage() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  RemoteStorageDB::GetInstance().Init(true);

  if (!RemoteStorageDB::GetInstance().IsInitialized()) {
    throw JsonRpcException(RPC_MISC_ERROR, "Failed to initialize");
  }

  return true;
}

string StatusServer::AverageBlockTime() {
  return to_string(
      static_cast<unsigned int>(m_mediator.m_aveBlockTimeInSeconds));
}

bool StatusServer::ToggleGetSmartContractState() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  m_mediator.m_disableGetSmartContractState =
      !m_mediator.m_disableGetSmartContractState;
  return m_mediator.m_disableGetSmartContractState;
}

bool StatusServer::AuditShard(const std::string& shardIDStr) {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  const uint32_t shardID = stoul(shardIDStr);
  LOG_GENERAL(INFO, "Auditing shard " << shardID);

  try {
    const auto shards = m_mediator.m_lookup->GetShardPeers();
    if (shards.size() <= shardID) {
      throw JsonRpcException(RPC_INVALID_PARAMETER, "Invalid shardID");
    }

    const auto& shard = shards.at(shardID);
    vector<Peer> peersVec;
    for (const auto& peer : shard) {
      LOG_GENERAL(INFO,
                  "Checking " << std::get<1>(peer).GetPrintableIPAddress());
      peersVec.emplace_back(std::get<1>(peer));
    }

    m_mediator.m_node->CheckPeers(peersVec);
  } catch (const JsonRpcException& je) {
    throw je;
  } catch (const exception& e) {
    LOG_GENERAL(WARNING, "[Error] " << e.what());
    throw JsonRpcException(RPC_MISC_ERROR, "Unable to process");
  }

  return true;
}

bool StatusServer::ToggleGetPendingTxns() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }

  m_mediator.m_disableGetPendingTxns = !m_mediator.m_disableGetPendingTxns;
  return m_mediator.m_disableGetPendingTxns;
}
