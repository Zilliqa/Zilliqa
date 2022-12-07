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

#include "RemoteStorageDB.h"
#include <boost/filesystem/operations.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include "libServer/JSONConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/HashUtils.h"
#include "libUtils/TimeUtils.h"

using namespace std;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_document;

ModificationState RemoteStorageDB::GetModificationState(
    const TxnStatus status) const {
  if (status == TxnStatus::DISPATCHED) {
    return ModificationState::DISPATCHED;
  } else if (IsTxnDropped(status) || status == TxnStatus::CONFIRMED) {
    return ModificationState::CONFIRMED_OR_DROPPED;
  } else {
    return ModificationState::IN_PROCESS;
  }
}

pair<string, string> getCreds() {
  string username, password;
  if (const char* env_p = getenv("ZIL_DB_USERNAME")) {
    username = env_p;
  }
  if (const char* env_p = getenv("ZIL_DB_PASSWORD")) {
    password = env_p;
  }

  return make_pair(username, password);
}

void RemoteStorageDB::Init(bool reset) {
  try {
    if (!reset) {
      auto instance = bsoncxx::stdx::make_unique<mongocxx::instance>();
      m_inst = move(instance);
    }

    auto creds = getCreds();
    string uri;
    if (creds.first.empty() || creds.second.empty()) {
      uri = "mongodb://" + REMOTESTORAGE_DB_HOST + ":" +
            to_string(REMOTESTORAGE_DB_PORT) + "/" + REMOTESTORAGE_DB_NAME;
    } else {
      uri = "mongodb://" + creds.first + ":" + creds.second + "@" +
            REMOTESTORAGE_DB_HOST + ":" + to_string(REMOTESTORAGE_DB_PORT) +
            "/" + REMOTESTORAGE_DB_NAME;

      LOG_GENERAL(INFO, "Authenticating.. found env variables");
    }
    uri += "?serverSelectionTimeoutMS=" +
           to_string(REMOTESTORAGE_DB_SERVER_SELECTION_TIMEOUT_MS);
    if (!REMOTESTORAGE_DB_TLS_FILE.empty() &&
        boost::filesystem::exists(REMOTESTORAGE_DB_TLS_FILE)) {
      uri += "&tls=true&tlsAllowInvalidHostnames=true&tlsCAFile=" +
             REMOTESTORAGE_DB_TLS_FILE;
    }

    uri += "&socketTimeoutMS=" + to_string(REMOTESTORAGE_DB_SOCKET_TIMEOUT_MS);

    mongocxx::uri URI(uri);
    if (URI.tls()) {
      LOG_GENERAL(INFO, "Connecting using TLS");
    }
    if (URI.server_selection_timeout_ms()) {
      LOG_GENERAL(INFO, "ServerSelectionTimeoutInMS: "
                            << URI.server_selection_timeout_ms().value());
    }
    if (URI.socket_timeout_ms()) {
      LOG_GENERAL(INFO,
                  "SockeTimeoutInMS: " << URI.socket_timeout_ms().value());
    }
    m_pool = bsoncxx::stdx::make_unique<mongocxx::pool>(move(URI));
    mongocxx::options::bulk_write bulk_opts;
    bulk_opts.ordered(false);
    const auto& conn = GetConnection();
    auto txnCollection = conn->database(m_dbName)[m_txnCollectionName];
    auto bulk = bsoncxx::stdx::make_unique<mongocxx::bulk_write>(
        txnCollection.create_bulk_write(bulk_opts));
    {
      lock_guard<mutex> g(m_mutexBulkWrite);
      m_bulkWrite = move(bulk);
    }
    m_initialized = true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to initialize DB: " << e.what());
    m_initialized = false;
  }
}

inline mongoConnection RemoteStorageDB::GetConnection() {
  return m_pool->acquire();
}

bool RemoteStorageDB::IsInitialized() const { return m_initialized; }

bool RemoteStorageDB::InsertTxn(const Transaction& txn, const TxnStatus status,
                                const uint64_t& epoch, const bool success) {
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    return false;
  }
  Json::Value tx_json = JSONConversion::convertTxtoJson(txn);
  tx_json["status"] = static_cast<int>(status);
  tx_json["success"] = success;
  tx_json["epochInserted"] = to_string(epoch);
  tx_json["epochUpdated"] = to_string(epoch);
  tx_json["lastModified"] = to_string(get_time_as_int());
  tx_json["modificationState"] = static_cast<int>(GetModificationState(status));

  try {
    const bsoncxx::document::value& doc_val =
        bsoncxx::from_json(tx_json.toStyledString());

    mongocxx::model::insert_one insert_op{doc_val.view()};
    {
      lock_guard<mutex> g(m_mutexBulkWrite);
      m_bulkWrite->append(insert_op);
      m_bulkWriteEmpty = false;
    }
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to InsertTxn " << e.what());
    return false;
  }

  return true;
}

bool RemoteStorageDB::ExecuteWrite() {
  LOG_MARKER();
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    return false;
  }

  auto resetBulkWrite = [this]() mutable {
    mongocxx::options::bulk_write bulk_opts;
    m_bulkWrite.reset(nullptr);
    bulk_opts.ordered(false);
    const auto& conn = GetConnection();
    auto txnCollection = conn->database(m_dbName)[m_txnCollectionName];
    auto bulk = bsoncxx::stdx::make_unique<mongocxx::bulk_write>(
        txnCollection.create_bulk_write(bulk_opts));
    m_bulkWrite = move(bulk);
    m_bulkWriteEmpty = true;
  };
  lock_guard<mutex> g(m_mutexBulkWrite);
  if (m_bulkWriteEmpty) {
    LOG_GENERAL(INFO, "No txns for RemoteStorageDB");
    return true;
  }
  try {
    const auto& res = m_bulkWrite->execute();

    if (!res) {
      LOG_GENERAL(WARNING, "Failed to ExecuteWrite");
      resetBulkWrite();
      return false;
    }

    LOG_GENERAL(INFO, "Inserted " << res.value().inserted_count()
                                  << " & Updated "
                                  << res.value().modified_count());

    resetBulkWrite();
    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to write bulk " << e.what());
    resetBulkWrite();
    return false;
  }
}

inline bsoncxx::stdx::optional<mongoConnection>
RemoteStorageDB::TryGetConnection() {
  return m_pool->try_acquire();
}

bool RemoteStorageDB::UpdateTxn(const string& txnhash, const TxnStatus status,
                                const uint64_t& epoch, const bool success) {
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    return false;
  }

  {
    lock_guard<mutex> g(m_mutexHashMapUpdateTxn);
    const auto& result_emplace =
        m_hashMapUpdateTxn.emplace(txnhash, status, epoch);
    if (!result_emplace.second) {
      LOG_GENERAL(INFO, "TxnHash already present: " << txnhash);
      return false;
    }
  }
  const auto& modifState = static_cast<int>(GetModificationState(status));
  try {
    const auto& currentTime = to_string(get_time_as_int());
    const auto& query_doc = make_document(
        kvp("ID", txnhash),
        kvp("modificationState", make_document(kvp("$lte", modifState))));
    const auto& doc = make_document(
        kvp("$set", make_document(kvp("status", static_cast<int>(status)),
                                  kvp("success", success),
                                  kvp("epochUpdated", to_string(epoch)),
                                  kvp("lastModified", currentTime),
                                  kvp("modificationState", modifState))));

    mongocxx::model::update_one update_op{query_doc.view(), doc.view()};
    {
      lock_guard<mutex> g(m_mutexBulkWrite);
      m_bulkWrite->append(update_op);
      m_bulkWriteEmpty = false;
    }
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to UpdateTxn " << txnhash << " " << e.what());
    return false;
  }
  return true;
}

Json::Value RemoteStorageDB::QueryTxnHash(const std::string& txnhash) {
  LOG_MARKER();
  Json::Value _json{Json::Value::null};
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    _json["error"] = true;
    return _json;
  }

  try {
    const auto& conn = TryGetConnection();  // Use try here as this function is
                                            // exposed via RPC API
    if (!conn) {
      LOG_GENERAL(WARNING, "Failed to establish connection");
      _json["error"] = true;
      return _json;
    }
    auto txnCollection = conn.value()->database(m_dbName)[m_txnCollectionName];
    auto cursor = txnCollection.find_one(make_document(kvp("ID", txnhash)));
    if (cursor) {
      const auto& json_string = bsoncxx::to_json(cursor.value());
      Json::Reader reader;
      reader.parse(json_string, _json);
    }

  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Failed to query" << txnhash << " " << e.what());
    _json["error"] = true;
  }

  return _json;
}

Json::Value RemoteStorageDB::QueryPendingTxns(
    const unsigned int txEpochFirstExclusive,
    const unsigned int txEpochLastInclusive) {
  LOG_MARKER();

  Json::Value _json{Json::Value::null};

  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    _json["error"] = true;
    return _json;
  }

  try {
    const auto& conn = TryGetConnection();  // Use try here as this function is
                                            // exposed via RPC API
    if (!conn) {
      LOG_GENERAL(WARNING, "Failed to establish connection");
      _json["error"] = true;
      return _json;
    }

    auto txnCollection = conn.value()->database(m_dbName)[m_txnCollectionName];

    // Query: epochUpdated in (txEpochFirstExclusive, txEpochLastInclusive] and
    // modificationState < 2 Note that epochUpdated is of type string - the
    // query for gt and lte work as long as length of epoch number expressed as
    // string is consistent
    auto doc = document{};
    doc << bsoncxx::builder::concatenate(
        document{} << "epochUpdated" << open_document << "$gt"
                   << to_string(txEpochFirstExclusive) << close_document
                   << finalize);
    doc << bsoncxx::builder::concatenate(
        document{} << "epochUpdated" << open_document << "$lte"
                   << to_string(txEpochLastInclusive) << close_document
                   << finalize);
    doc << bsoncxx::builder::concatenate(
        document{} << "modificationState" << open_document << "$lt"
                   << static_cast<int>(ModificationState::CONFIRMED_OR_DROPPED)
                   << close_document << finalize);

    LOG_GENERAL(DEBUG, "Query = " << bsoncxx::to_json(doc));

    mongocxx::options::find opts;
    opts.limit(PENDING_TXN_QUERY_MAX_RESULTS);  // Limit number of results
    opts.sort(document{} << "epochUpdated" << -1
                         << finalize);  // Sort by epochUpdated descending
    if (DEBUG_LEVEL == 4) {
      opts.projection(
          document{}
          << "ID" << 1 << "status" << 1 << "epochUpdated" << 1
          << finalize);  // Retrieve only ID, status, and epochUpdated fields
    } else {
      opts.projection(document{}
                      << "ID" << 1 << "status" << 1
                      << finalize);  // Retrieve only ID and status fields
    }

    auto cursor = txnCollection.find(doc.view(), opts);

    _json["Txns"] = Json::Value(Json::arrayValue);

    auto putTxn = [&_json](const bsoncxx::document::view& doc) -> void {
      Json::Value tmpJson;
      tmpJson["TxnHash"] = doc["ID"].get_utf8().value.to_string();
      tmpJson["code"] = doc["status"].get_int32().value;
      _json["Txns"].append(tmpJson);
    };

    if (DEBUG_LEVEL == 4) {
      unsigned int count = 0;
      for (const auto& doc : cursor) {
        LOG_GENERAL(DEBUG, bsoncxx::to_json(doc));
        putTxn(doc);
        count++;
      }
      LOG_GENERAL(DEBUG, "Num results = " << count);
    } else {
      for (const auto& doc : cursor) {
        putTxn(doc);
      }
    }

  } catch (std::exception& e) {
    LOG_GENERAL(WARNING, "Failed to query: " << e.what());
    _json["error"] = true;
  }

  return _json;
}

void RemoteStorageDB::ClearHashMapForUpdates() {
  lock_guard<mutex> g(m_mutexHashMapUpdateTxn);
  m_hashMapUpdateTxn.clear();
}

void RemoteStorageDB::ExecuteWriteDetached() {
  auto func_detached = [this]() -> void {
    if (!ExecuteWrite()) {
      LOG_GENERAL(INFO, "Execute Write failed");
    }
  };
  DetachedFunction(1, func_detached);
}
