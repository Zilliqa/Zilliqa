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
