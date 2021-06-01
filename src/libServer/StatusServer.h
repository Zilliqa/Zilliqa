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

#ifndef ZILLIQA_SRC_LIBSERVER_STATUSSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_STATUSSERVER_H_

#include "Server.h"

class StatusServer : public Server,
                     public jsonrpc::AbstractServer<StatusServer> {
 public:
  StatusServer(Mediator& mediator, jsonrpc::AbstractServerConnector& server);
  inline virtual void GetNodeStateI(const Json::Value& request,
                                    Json::Value& response) {
    (void)request;
    response = this->GetNodeState();
  }

  inline virtual void IsTxnInMemPoolI(const Json::Value& request,
                                      Json::Value& response) {
    response = this->IsTxnInMemPool(request[0u].asString());
  }
  inline virtual void AddToBlacklistExclusionI(const Json::Value& request,
                                               Json::Value& response) {
    response = this->AddToBlacklistExclusion(request[0u].asString());
  }
  inline virtual void AddToExtSeedWhitelistI(const Json::Value& request,
                                             Json::Value& response) {
    response = this->AddToExtSeedWhitelist(request[0u].asString());
  }
  inline virtual void RemoveFromExtSeedWhitelistI(const Json::Value& request,
                                                  Json::Value& response) {
    response = this->RemoveFromExtSeedWhitelist(request[0u].asString());
  }
  inline virtual void GetWhitelistedExtSeedI(const Json::Value& request,
                                             Json::Value& response) {
    (void)request;
    response = this->GetWhitelistedExtSeed();
  }
  inline virtual void RemoveFromBlacklistExclusionI(const Json::Value& request,
                                                    Json::Value& response) {
    response = this->RemoveFromBlacklistExclusion(request[0u].asString());
  }
  inline virtual void AddToSeedsWhitelistI(const Json::Value& request,
                                           Json::Value& response) {
    response = this->AddToSeedsWhitelist(request[0u].asString());
  }
  inline virtual void RemoveFromSeedsWhitelistI(const Json::Value& request,
                                                Json::Value& response) {
    response = this->RemoveFromSeedsWhitelist(request[0u].asString());
  }
  inline virtual void IsIPInBlacklistI(const Json::Value& request,
                                       Json::Value& response) {
    response = this->IsIPInBlacklist(request[0u].asString());
  }
  inline virtual void RemoveIPFromBlacklistI(const Json::Value& request,
                                             Json::Value& response) {
    response = this->RemoveIPFromBlacklist(request[0u].asString());
  }
  inline virtual void GetLatestEpochStatesUpdatedI(const Json::Value& request,
                                                   Json::Value& response) {
    (void)request;
    response = this->GetLatestEpochStatesUpdated();
  }
  inline virtual void GetEpochFinI(const Json::Value& request,
                                   Json::Value& response) {
    (void)request;
    response = this->GetEpochFin();
  }
  inline virtual void GetDSCommitteeI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetDSCommittee();
  }
  inline virtual void GetHealthI(const Json::Value& request,
                                 Json::Value& response) {
    (void)request;
    response = this->GetHealth();
  }
  inline virtual void ToggleSendSCCallsToDSI(const Json::Value& request,
                                             Json::Value& response) {
    (void)request;
    response = this->ToggleSendSCCallsToDS();
  }
  inline virtual void GetSendSCCallsToDSI(const Json::Value& request,
                                          Json::Value& response) {
    (void)request;
    response = this->GetSendSCCallsToDS();
  }
  inline virtual void DisablePoWI(const Json::Value& request,
                                  Json::Value& response) {
    (void)request;
    response = this->DisablePoW();
  }
  inline virtual void GetSendAllToDSI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetSendAllToDS();
  }
  inline virtual void ToggleSendAllToDSI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->ToggleSendAllToDS();
  }
  inline virtual void ToggleDisableTxnsI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->ToggleDisableTxns();
  }
  inline virtual void SetValidateDBI(const Json::Value& request,
                                     Json::Value& response) {
    (void)request;
    response = this->SetValidateDB();
  }
  inline virtual void GetValidateDBI(const Json::Value& request,
                                     Json::Value& response) {
    (void)request;
    response = this->GetValidateDB();
  }
  inline virtual void SetVoteInPowI(const Json::Value& request,
                                    Json::Value& response) {
    (void)request;
    response = this->SetVoteInPow(
        request[0u].asString(), request[1u].asString(), request[2u].asString(),
        request[3u].asString(), request[4u].asString());
  }
  inline virtual void ToggleRemoteStorageI(const Json::Value& request,
                                           Json::Value& response) {
    (void)request;
    response = this->ToggleRemoteStorage();
  }
  inline virtual void GetRemoteStorageI(const Json::Value& request,
                                        Json::Value& response) {
    (void)request;
    response = this->GetRemoteStorage();
  }
  inline virtual void InitRemoteStorageI(const Json::Value& request,
                                         Json::Value& response) {
    (void)request;
    response = this->InitRemoteStorage();
  }
  inline virtual void GetAverageBlockTimeI(const Json::Value& request,
                                           Json::Value& response) {
    (void)request;
    response = this->AverageBlockTime();
  }
  inline virtual void ToggleGetSmartContractStateI(const Json::Value& request,
                                                   Json::Value& response) {
    (void)request;
    response = this->ToggleGetSmartContractState();
  }
  inline virtual void AuditShardI(const Json::Value& request,
                                  Json::Value& response) {
    (void)request;
    response = this->AuditShard(request[0u].asString());
  }
  inline virtual void ToggleGetPendingTxnsI(const Json::Value& request,
                                            Json::Value& response) {
    (void)request;
    response = this->ToggleGetPendingTxns();
  }

  Json::Value IsTxnInMemPool(const std::string& tranID);
  bool AddToBlacklistExclusion(const std::string& ipAddr);
  bool AddToExtSeedWhitelist(const std::string& pubKeyStr);
  bool RemoveFromExtSeedWhitelist(const std::string& pubKeyStr);
  std::string GetWhitelistedExtSeed();
  bool RemoveFromBlacklistExclusion(const std::string& ipAddr);
  bool AddToSeedsWhitelist(const std::string& ipAddr);
  bool RemoveFromSeedsWhitelist(const std::string& ipAddr);
  bool IsIPInBlacklist(const std::string& ipAddr);
  bool RemoveIPFromBlacklist(const std::string& ipAddr);
  std::string GetNodeState();
  std::string GetLatestEpochStatesUpdated();
  std::string GetEpochFin();
  Json::Value GetDSCommittee();
  std::string GetHealth();
  bool ToggleSendSCCallsToDS();
  bool GetSendSCCallsToDS();
  bool ToggleSendAllToDS();
  bool GetSendAllToDS();
  bool DisablePoW();
  bool ToggleDisableTxns();
  std::string SetValidateDB();
  std::string GetValidateDB();
  bool SetVoteInPow(const std::string& proposalId, const std::string& voteValue,
                    const std::string& remainingVoteCount,
                    const std::string& startDSEpoch,
                    const std::string& endDSEpoch);
  bool ToggleRemoteStorage();
  bool GetRemoteStorage();
  bool InitRemoteStorage();
  std::string AverageBlockTime();
  bool ToggleGetSmartContractState();
  bool AuditShard(const std::string& shardIDStr);
  bool ToggleGetPendingTxns();
};

#endif  // ZILLIQA_SRC_LIBSERVER_STATUSSERVER_H_
