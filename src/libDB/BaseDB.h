/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
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
