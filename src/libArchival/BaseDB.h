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

#ifndef __BASEDB_H__
#define __BASEDB_H__

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>
#include <string>
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockData/Block/DSBlock.h"
#include "libData/BlockData/Block/TxBlock.h"

class BaseDB {
 protected:
  std::unique_ptr<mongocxx::pool> m_pool;
  std::unique_ptr<mongocxx::instance> m_inst;
  bool m_isInitialized;
  const std::string m_dbname;
  const std::string m_txCollectionName;
  const std::string m_txBlockCollectionName;
  const std::string m_dsBlockCollectionName;
  const std::string m_accountStateCollectionName;

 public:
  BaseDB(std::string dbname, std::string txn, std::string txBlock,
         std::string dsBlock, std::string accountState)
      : m_isInitialized(false),
        m_dbname(dbname),
        m_txCollectionName(txn),
        m_txBlockCollectionName(txBlock),
        m_dsBlockCollectionName(dsBlock),
        m_accountStateCollectionName(accountState)

  {}
  virtual void Init(unsigned int port = 27017);
  virtual bool InsertTxn(const TransactionWithReceipt& txn) = 0;
  virtual bool InsertTxBlock(const TxBlock& txblock) = 0;
  virtual bool InsertDSBlock(const DSBlock& dsblock) = 0;
  virtual bool InsertAccount(const Address& addr, const Account& acc) = 0;
};

#endif  //__BASEDB_H__
