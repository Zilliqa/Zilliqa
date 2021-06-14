/*
 * Copyright (C) 2021 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_DATASTRUCTURES_TRACEABLEDB_H_
#define ZILLIQA_SRC_LIBDATA_DATASTRUCTURES_TRACEABLEDB_H_

#include "depends/libDatabase/OverlayDB.h"

class TraceableDB : public dev::OverlayDB {
 public:
  explicit TraceableDB(const std::string& dbName)
      : dev::OverlayDB(dbName), m_purgeDB(dbName + "_purge") {}
  ~TraceableDB() = default;
  bool commit(const uint64_t& dsBlockNum);

 private:
  LevelDB m_purgeDB;
  std::atomic<bool> m_stopSignal{false};
  std::atomic<bool> m_purgeRunning{false};

  bool AddPendingPurge(const uint64_t& dsBlockNum,
                       const std::vector<dev::h256>& toPurge);
  bool ExecutePurge(const uint64_t& dsBlockNum, bool purgeAll = false);

 public:
  void DetachedExecutePurge();

  void SetStopSignal() { m_stopSignal = true; }

  bool IsPurgeRunning() { return m_purgeRunning; }

  bool RefreshDB();
};

#endif  // ZILLIQA_SRC_LIBDATA_DATASTRUCTURES_TRACEABLEDB_H_