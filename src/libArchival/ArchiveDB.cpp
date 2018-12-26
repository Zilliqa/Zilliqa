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
