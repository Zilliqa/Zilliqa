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

#include "ArchiveDB.h"
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include <bsoncxx/stdx/optional.hpp>
#include <bsoncxx/types.hpp>
#include <cstdint>
#include <iostream>
#include <mongocxx/client.hpp>
#include <mongocxx/logger.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <vector>
#include "libServer/JSONConversion.h"
#include "libUtils/HashUtils.h"

using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_document;

using namespace std;

bool ArchiveDB::InsertTxn(const TransactionWithReceipt& txn) {
  string index = txn.GetTransaction().GetTranID().hex();
  return InsertSerializable(txn, index, m_txCollectionName);
}

bool ArchiveDB::InsertTxBlock(const TxBlock& txblock) {
  string index = to_string(txblock.GetHeader().GetBlockNum());
  return InsertSerializable(txblock, index, m_txBlockCollectionName);
}

bool ArchiveDB::InsertDSBlock(const DSBlock& dsblock) {
  string index = to_string(dsblock.GetHeader().GetBlockNum());
  return InsertSerializable(dsblock, index, m_dsBlockCollectionName);
}

bool ArchiveDB::InsertAccount(const Address& addr, const Account& acc) {
  string index = addr.hex();
  return InsertSerializable(acc, index, m_accountStateCollectionName);
}

bool ArchiveDB::InsertSerializable(const Serializable& sz, const string& index,
                                   const string& collectionName) {
  if (!m_isInitialized) {
    return false;
  }
  bytes vec;
  sz.Serialize(vec, 0);
  try {
    auto MongoClient = (m_pool->acquire());
    bsoncxx::types::b_binary bin_data;
    bin_data.size = vec.size();
    bin_data.bytes = vec.data();
    bsoncxx::document::value doc_val =
        make_document(kvp("_id", index), kvp("Value", bin_data));

    auto res = MongoClient->database(m_dbname)[collectionName].insert_one(
        move(doc_val));
    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING,
                "Failed to insert in DB " << collectionName << " " << e.what());
    return false;
  }
}

// Temporary function for use by data blocks
bool ArchiveDB::InsertSerializable(const SerializableDataBlock& sz,
                                   const string& index,
                                   const string& collectionName) {
  if (!m_isInitialized) {
    return false;
  }
  bytes vec;
  sz.Serialize(vec, 0);
  try {
    auto MongoClient = (m_pool->acquire());
    bsoncxx::types::b_binary bin_data;
    bin_data.size = vec.size();
    bin_data.bytes = vec.data();
    bsoncxx::document::value doc_val =
        make_document(kvp("_id", index), kvp("Value", bin_data));

    auto res = MongoClient->database(m_dbname)[collectionName].insert_one(
        move(doc_val));
    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING,
                "Failed to insert in DB " << collectionName << " " << e.what());
    return false;
  }
}

bool ArchiveDB::GetSerializable(bytes& retVec, const string& index,
                                const string& collectionName) {
  if (!m_isInitialized) {
    return false;
  }
  auto MongoClient = (m_pool->acquire());
  auto cursor = MongoClient->database(m_dbname)[collectionName].find(
      make_document(kvp("_id", index)));

  try {
    bsoncxx::document::element ele;
    int counter = 0;
    for (auto&& doc : cursor) {
      counter++;
      ele = doc["Value"];
    }
    if (counter == 0) {
      return false;
    } else if (counter > 1) {
      LOG_GENERAL(WARNING, "More than one txn found, Investigate ?" << index);
    }

    if (ele.type() == bsoncxx::type::k_binary) {
      bsoncxx::types::b_binary bin_data;
      bin_data = ele.get_binary();
      copy(bin_data.bytes, bin_data.bytes + bin_data.size,
           back_inserter(retVec));
      return true;
    } else {
      LOG_GENERAL(WARNING, "Element type mismatch");
      return false;
    }
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to Find in DB " << e.what());
    return false;
  }
}
