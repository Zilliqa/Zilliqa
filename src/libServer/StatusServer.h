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

#ifndef __STATUS_SERVER_H__
#define __STATUS_SERVER_H__

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
  inline virtual void GetDSCommitteeI(const Json::Value& request,
                                      Json::Value& response) {
    (void)request;
    response = this->GetDSCommittee();
  }
  Json::Value IsTxnInMemPool(const std::string& tranID);
  bool AddToBlacklistExclusion(const std::string& ipAddr);
  bool RemoveFromBlacklistExclusion(const std::string& ipAddr);
  std::string GetNodeState();
  std::string GetLatestEpochStatesUpdated();
  Json::Value GetDSCommittee();
};

#endif  //__STATUS_SERVER_H__