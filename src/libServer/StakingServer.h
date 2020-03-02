/*
 * Copyright (C) 2020 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBSERVER_STAKINGSERVER_H_
#define ZILLIQA_SRC_LIBSERVER_STAKINGSERVER_H_

#include "Server.h"

class Mediator;

class StakingServer : public Server,
                      public jsonrpc::AbstractServer<StakingServer> {
  inline virtual void GetRawDSBlockI(const Json::Value& request,
                                     Json::Value& response) {
    response = this->GetRawDSBlock(request[0u].asString());
  }
  Json::Value GetRawDSBlock(const std::string& blockNum);
  inline virtual void GetRawTxBlockI(const Json::Value& request,
                                     Json::Value& response) {
    response = this->GetRawTxBlock(request[0u].asString());
  }
  Json::Value GetRawTxBlock(const std::string& blockNum);

 public:
  StakingServer(Mediator& mediator, jsonrpc::AbstractServerConnector& server);
  ~StakingServer() = default;
};

#endif  // ZILLIQA_SRC_LIBSERVER_STAKINGSERVER_H_
