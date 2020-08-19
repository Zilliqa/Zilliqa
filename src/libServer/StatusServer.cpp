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
#include "libNetwork/Blacklist.h"

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
      jsonrpc::Procedure("GetDSCommittee", jsonrpc::PARAMS_BY_POSITION,
                         jsonrpc::JSON_OBJECT, NULL),
      &StatusServer::GetDSCommitteeI);
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
        case ErrTxnStatus::NOT_PRESENT:
          _json["present"] = false;
          _json["pending"] = false;
          _json["code"] = ErrTxnStatus::NOT_PRESENT;
          return _json;
        case ErrTxnStatus::PRESENT_NONCE_HIGH:
          _json["present"] = true;
          _json["pending"] = true;
          _json["code"] = ErrTxnStatus::PRESENT_NONCE_HIGH;
          return _json;
        case ErrTxnStatus::PRESENT_GAS_EXCEEDED:
          _json["present"] = true;
          _json["pending"] = true;
          _json["code"] = ErrTxnStatus::PRESENT_GAS_EXCEEDED;
          return _json;
        case ErrTxnStatus::ERROR:
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

bool StatusServer::DisablePoW() {
  if (LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST, "Not to be queried on lookup");
  }
  m_mediator.m_disablePoW = true;
  return true;
}

bool StatusServer::ToggleDisableTxns() {
  if (!LOOKUP_NODE_MODE) {
    throw JsonRpcException(RPC_INVALID_REQUEST,
                           "Not to be queried on non-lookup");
  }
  m_mediator.m_disableTxns = !m_mediator.m_disableTxns;
  return m_mediator.m_disableTxns;
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