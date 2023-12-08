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
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/stdx/make_unique.hpp>
#include "libServer/JSONConversion.h"
#include "libUtils/DetachedFunction.h"
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

namespace {

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

tuple<string, unsigned int, string> getConnDetails() {
  string host, dbName;
  unsigned int port = 27017;
  if (const char* env_p = getenv("REMOTESTORAGE_DB_NAME")) {
    dbName = env_p;
  } else {
    dbName = REMOTESTORAGE_DB_NAME;
  }

  if (const char* env_p = getenv("REMOTESTORAGE_DB_HOST")) {
    host = env_p;
  } else {
    host = REMOTESTORAGE_DB_HOST;
  }

  if (const char* env_p = getenv("REMOTESTORAGE_DB_PORT")) {
    // bad conversation will throw an exception that will be caught in Init
    port = stoul(env_p);
  } else {
    port = REMOTESTORAGE_DB_PORT;
  }

  return make_tuple(host, port, dbName);
}

optional<string> getConnStr() {
  optional<string> result;
  if (const char* env_p = getenv("REMOTESTORAGE_DB_CONN_STRING")) {
    result = env_p;
  } else if (!REMOTESTORAGE_DB_CONN_STRING.empty()) {
    result = REMOTESTORAGE_DB_CONN_STRING;
  }

  return result;
}

optional<string_view> extractDbName(const string& connStr) {
  const constexpr auto scheme = "mongodb://";
  if (connStr.find(scheme) != 0) {
    return nullopt;
  }

  auto dbNamePos = connStr.find('/', string_view{scheme}.size());
  if (dbNamePos == string::npos) {
    return nullopt;
  }

  // could be string::npos if there are no args
  auto argsPos = connStr.find('?', dbNamePos + 1);

  return std::string_view{
      connStr.data() + dbNamePos + 1,
      (argsPos == string::npos ? connStr.size() : argsPos) - dbNamePos - 1};
}

}  // namespace

void RemoteStorageDB::Init(bool reset) {
  try {
    if (!reset) {
      auto instance = bsoncxx::stdx::make_unique<mongocxx::instance>();
      m_inst = std::move(instance);
    }

    string uri;
    auto uriConnStr = getConnStr();
    if (uriConnStr) {
      auto dbName = extractDbName(*uriConnStr);
      if (!dbName) {
        // caught below
        throw std::runtime_error{"Couldn't extract MongoDB database name"};
      }

      LOG_GENERAL(INFO,
                  "Using the configured MongoDB connection string (database = "
                      << *dbName << ")");
      m_dbName = *dbName;
      uri = *uriConnStr;
    } else {
      auto [host, port, dbName] = getConnDetails();
      auto [username, password] = getCreds();
      uri = "mongodb://" +
            ((username.empty() || password.empty())
                 ? ""
                 : username + ":" + password + "@") +
            host + ":" + to_string(port) + "/" + dbName;
      m_dbName = dbName;
      uri += "?serverSelectionTimeoutMS=" +
             to_string(REMOTESTORAGE_DB_SERVER_SELECTION_TIMEOUT_MS);
      if (!REMOTESTORAGE_DB_TLS_FILE.empty() &&
          std::filesystem::exists(REMOTESTORAGE_DB_TLS_FILE)) {
        uri += "&tls=true&tlsAllowInvalidHostnames=true&tlsCAFile=" +
               REMOTESTORAGE_DB_TLS_FILE;
      }

      uri +=
          "&socketTimeoutMS=" + to_string(REMOTESTORAGE_DB_SOCKET_TIMEOUT_MS);
    }

    LOG_GENERAL(INFO, "Connecting to MongoDB...");
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
    m_pool = bsoncxx::stdx::make_unique<mongocxx::pool>(std::move(URI));
    mongocxx::options::bulk_write bulk_opts;
    bulk_opts.ordered(false);
    const auto& conn = GetConnection();
    auto txnCollection = conn->database(m_dbName)[m_txnCollectionName];
    auto bulk = bsoncxx::stdx::make_unique<mongocxx::bulk_write>(
        txnCollection.create_bulk_write(bulk_opts));
    {
      lock_guard<mutex> g(m_mutexBulkWrite);
      m_bulkWrite = std::move(bulk);
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
  constexpr auto USE_HEX_ENCODING_FOR_CODE_AND_DATA = true;
  Json::Value tx_json =
      JSONConversion::convertTxtoJson(txn, USE_HEX_ENCODING_FOR_CODE_AND_DATA);
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
    LOG_GENERAL(DEBUG, "Failed to InsertTxn " << e.what());
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
    m_bulkWrite = std::move(bulk);
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
  LOG_MARKER();
  if (!m_initialized) {
    LOG_GENERAL(WARNING, "DB not initialized");
    return false;
  }

  {
    lock_guard<mutex> g(m_mutexHashMapUpdateTxn);
    const auto& result_emplace = m_hashMapUpdateTxn.emplace(txnhash, status);
    if (!result_emplace.second) {
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

Json::Value RemoteStorageDB::QueryTxnHash(const dev::h256& txnhash) {
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
    auto cursor =
        txnCollection.find_one(make_document(kvp("ID", txnhash.hex())));
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
      tmpJson["TxnHash"] = std::string(doc["ID"].get_string());
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
