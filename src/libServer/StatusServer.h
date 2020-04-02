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
  inline virtual void RemoveFromBlacklistExclusionI(const Json::Value& request,
                                                    Json::Value& response) {
    response = this->RemoveFromBlacklistExclusion(request[0u].asString());
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

  Json::Value IsTxnInMemPool(const std::string& tranID);
  bool AddToBlacklistExclusion(const std::string& ipAddr);
  bool RemoveFromBlacklistExclusion(const std::string& ipAddr);
  std::string GetNodeState();
  std::string GetLatestEpochStatesUpdated();
  std::string GetEpochFin();
  Json::Value GetDSCommittee();
  bool ToggleSendSCCallsToDS();
  bool GetSendSCCallsToDS();
  bool DisablePoW();
};

#endif  // ZILLIQA_SRC_LIBSERVER_STATUSSERVER_H_
