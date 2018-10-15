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

#include "ExplorerDB.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include "libServer/JSONConversion.h"
#include "libUtils/HashUtils.h"

using namespace std;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

bool ExplorerDB::InsertTxn(const TransactionWithReceipt& txn) {
  Json::Value tx_json = JSONConversion::convertTxtoJson(txn);
  return InsertJson(tx_json, m_txCollectionName);
}

bool ExplorerDB::InsertTxBlock(const TxBlock& txblock) {
  Json::Value txblock_json = JSONConversion::convertTxBlocktoJson(txblock);
  txblock_json["hash"] = txblock.GetBlockHash().hex();
  return InsertJson(txblock_json, m_txBlockCollectionName);
}

bool ExplorerDB::InsertDSBlock(const DSBlock& dsblock) {
  Json::Value dsblock_json = JSONConversion::convertDSblocktoJson(dsblock);
  dsblock_json["hash"] = dsblock.GetBlockHash().hex();
  return InsertJson(dsblock_json, m_dsBlockCollectionName);
}

bool ExplorerDB::InsertAccount([[gnu::unused]] const Address& addr,
                               [[gnu::unused]] const Account& acc) {
  return true;
}

void ExplorerDB::Init(unsigned int port) {
  BaseDB::Init(port);
  mongocxx::options::index index_options;
  index_options.unique(true);
  auto MongoClient = m_pool->acquire();
  auto mongoDB = MongoClient->database(m_dbname);
  // ID is unique in txn and from is also an index but not unique
  mongoDB[m_txCollectionName].create_index(make_document(kvp("ID", 1)),
                                           index_options);
  mongoDB[m_txCollectionName].create_index(make_document(kvp("toAddr", 1)), {});
  // blockNum is unique in txBlock and DSBlock
  mongoDB[m_txBlockCollectionName].create_index(
      make_document(kvp("header.blockNum", 1)), index_options);
  mongoDB[m_dsBlockCollectionName].create_index(
      make_document(kvp("header.blockNum", 1)), index_options);
}

bool ExplorerDB::InsertJson(const Json::Value& _json,
                            const string& collectionName) {
  try {
    auto MongoClient = m_pool->acquire();
    auto mongoDB = MongoClient->database(m_dbname);
    bsoncxx::document::value doc_val =
        bsoncxx::from_json(_json.toStyledString());
    auto res = mongoDB[collectionName].insert_one(move(doc_val));
    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to Insert " << _json.toStyledString());
    return false;
  }
}