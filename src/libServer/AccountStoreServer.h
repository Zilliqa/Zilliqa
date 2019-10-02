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
#ifndef ZILLIQA_SRC_LIBSERVER_ACCOUNTSTORESERVER_H_
#define ZILLIQA_SRC_LIBSERVER_ACCOUNTSTORESERVER_H_

#include <mutex>
#include <random>

#include "JSONConversion.h"
#include "LookupServer.h"
#include "Server.h"
#include "jsonrpccpp/server.h"

#include "depends/common/FixedHash.h"

#include "libData/AccountData/Address.h"

/// Assume we have three shards, and letting the txn sender to determine the
/// block num

class AccountStoreServer : public jsonrpc::AbstractServer<AccountStoreServer>,
                           public ServerBase {
  CreateTransactionTargetFunc m_createTransactionTarget =
      [this](const Transaction& tx, uint32_t shardId) -> bool {
    (void)tx;
    (void)shardId;
    return true;  // TODO
  };

 public:
  AccountStoreServer(jsonrpc::AbstractServerConnector& conn);
  ~AccountStoreServer() = default;

  inline virtual void CreateTransactionI(const Json::Value& request,
                                         Json::Value& response) {
    response = LookupServer::CreateTransaction(
        request[0u], m_shardSize, m_gasPrice, m_createTransactionTarget);
  }
  // inline virtual void AddAccountI(const Json::Value& request,
  //                                 Json::Value& response) {
  //   bool found = true;
  //   if (!AddAccount(request[0u])) {
  //     found = false;
  //     throw jsonrpc::JsonRpcException("Add Account failed");
  //   }

  //   response.clear();
  //   response.append(Json::Value(found));
  // }
  // inline virtual void GetAccountI(const Json::Value& request,
  //                                 Json::Value& response) {
  //   response = this->GetAccount(request[0u]);
  // }

  // virtual bool AddAccount(const Json::Value& _json);
  // virtual Json::Value GetAccount(const Json::Value& _json);

 private:
  unsigned int m_shardSize;
  uint128_t m_gasPrice;
};

#endif  // ZILLIQA_SRC_LIBSERVER_ACCOUNTSTORESERVER_H_