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

#ifndef ZILLIQA_SRC_LIBREMOTESTORAGEDB_REMOTESTORAGEDB_H_
#define ZILLIQA_SRC_LIBREMOTESTORAGEDB_REMOTESTORAGEDB_H_

#include "common/Singleton.h"
#include "common/TxnStatus.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Transaction.h"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/pool.hpp>

using mongoConnection = mongocxx::pool::entry;

enum class ModificationState {
  DISPATCHED = 0,
  IN_PROCESS = 1,
  CONFIRMED_OR_DROPPED = 2
};

struct PendingTxnStatus {
  std::string m_txnhash;
  TxnStatus m_status;
  uint64_t m_epoch;

  PendingTxnStatus(const std::string& txnhash, const TxnStatus& status,
                   const uint64_t& epoch)
      : m_txnhash(txnhash), m_status(status), m_epoch(epoch) {}

  bool operator==(const PendingTxnStatus& pts) const {
    return ((pts.m_txnhash == m_txnhash) && (pts.m_status == m_status) &&
            (pts.m_epoch == m_epoch));
  }
};

struct PendingTxnStatusHash {
  std::size_t operator()(const PendingTxnStatus& pts) const {
    return std::hash<std::string>()(pts.m_txnhash) ^
           std::hash<std::uint8_t>()(static_cast<uint8_t>(pts.m_status)) ^
           std::hash<std::uint64_t>()(pts.m_epoch);
  }
};

class RemoteStorageDB : public Singleton<RemoteStorageDB> {
  std::unique_ptr<mongocxx::pool> m_pool;
  std::unique_ptr<mongocxx::instance> m_inst;
  bool m_initialized;
  bool m_bulkWriteEmpty;
  const std::string m_dbName;
  const std::string m_txnCollectionName;
  std::unique_ptr<mongocxx::bulk_write> m_bulkWrite;
  std::mutex m_mutexBulkWrite;
  std::mutex m_mutexHashMapUpdateTxn;
  std::unordered_set<PendingTxnStatus, PendingTxnStatusHash> m_hashMapUpdateTxn;

 public:
  RemoteStorageDB(std::string txnCollectionName = "TransactionStatus")
      : m_initialized(false),
        m_bulkWriteEmpty(true),
        m_dbName(REMOTESTORAGE_DB_NAME),
        m_txnCollectionName(std::move(txnCollectionName)) {}

  void Init(bool reset = false);
  bool InsertJson(const Json::Value& _json, const std::string& collectionName);
  bool InsertTxn(const Transaction& txn, const TxnStatus status,
                 const uint64_t& epoch, const bool success = false);
  bool UpdateTxn(const std::string& txnhash, const TxnStatus status,
                 const uint64_t& epoch, const bool success);
  Json::Value QueryTxnHash(const std::string& txnhash);
  Json::Value QueryPendingTxns(const unsigned int txEpochFirstExclusive,
                               const unsigned int txEpochLastInclusive);
  ModificationState GetModificationState(const TxnStatus status) const;
  bool ExecuteWrite();
  void ExecuteWriteDetached();
  bool IsInitialized() const;
  void ClearHashMapForUpdates();

  static RemoteStorageDB& GetInstance() {
    static RemoteStorageDB rsDB;
    return rsDB;
  }

 private:
  mongoConnection GetConnection();
  bsoncxx::stdx::optional<mongoConnection> TryGetConnection();
};

#endif  // ZILLIQA_SRC_LIBREMOTESTORAGEDB_REMOTESTORAGEDB_H_