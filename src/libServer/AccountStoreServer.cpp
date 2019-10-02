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

#include "libData/AccountData/AccountStore.h"
#include "libUtils/DataConversion.h"

#include "AccountStoreServer.h"

using namespace std;
using namespace jsonrpc;

AccountStoreServer::AccountStoreServer(AbstractServerConnector& conn)
    : AbstractServer<AccountStoreServer>(conn, JSONRPC_SERVER_V2) {
  // These JSON signatures match that of the actual functions below.
  bindAndAddMethod(
      Procedure("CreateTransaction", jsonrpc::PARAMS_BY_POSITION,
                jsonrpc::JSON_OBJECT, "param01", jsonrpc::JSON_OBJECT, NULL),
      &AccountStoreServer::CreateTransactionI);
}

// bool AccountStoreServer::AddAccount(const Json::Value& _json) {}

// Json::Value AccountStoreServer::GetAccount(const Json::Value& _json) {}