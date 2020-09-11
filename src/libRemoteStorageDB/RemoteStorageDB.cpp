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
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include "libServer/JSONConversion.h"
#include "libUtils/HashUtils.h"

using namespace std;
using bsoncxx::builder::basic::kvp;
using bsoncxx::builder::basic::make_array;
using bsoncxx::builder::basic::make_document;

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
    if (!REMOTESTORAGE_DB_TLS_FILE.empty() &&
        boost::filesystem::exists(REMOTESTORAGE_DB_TLS_FILE)) {
      uri += "?tls=true&tlsAllowInvalidHostnames=true&tlsCAFile=" +
             REMOTESTORAGE_DB_TLS_FILE;
    }
    mongocxx::uri URI(uri);
    if (URI.tls()) {
      LOG_GENERAL(INFO, "Connecting using TLS");
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
    LOG_GENERAL(WARNING, "Failed to initialized DB " << e.what());
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
  };
  lock_guard<mutex> g(m_mutexBulkWrite);
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
    }
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to UpdateTxn " << txnhash << " " << e.what());
    return false;
  }
  return true;
}

bool RemoteStorageDB::InsertJson(const Json::Value& _json,
                                 const string& collectionName) {
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    return false;
  }
  try {
    const auto& conn = GetConnection();
    auto txnCollection = conn->database(m_dbName)[collectionName];
    bsoncxx::document::value doc_val =
        bsoncxx::from_json(_json.toStyledString());
    const auto& res = txnCollection.insert_one(move(doc_val));
    return true;
  } catch (exception& e) {
    LOG_GENERAL(WARNING, "Failed to Insert " << _json.toStyledString() << endl
                                             << e.what());
    return false;
  }
}

Json::Value RemoteStorageDB::QueryTxnHash(const std::string& txnhash) {
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
