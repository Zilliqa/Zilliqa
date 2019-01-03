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

#include <json/json.h>
#include <vector>
#include "BaseDB.h"

class ExplorerDB : public BaseDB {
 public:
  ExplorerDB(std::string dbname, std::string txn, std::string txBlock,
             std::string dsBlock, std::string accountState)
      : BaseDB(dbname, txn, txBlock, dsBlock, accountState) {}
  bool InsertTxn(const TransactionWithReceipt& txn) override;
  bool InsertTxBlock(const TxBlock& txblock) override;
  bool InsertDSBlock(const DSBlock& dsblock) override;
  bool InsertJson(const Json::Value& _json, const std::string& collectionName);
  bool InsertAccount(const Address& addr, const Account& acc) override;
  void Init(unsigned int port = 27017) override;
};
