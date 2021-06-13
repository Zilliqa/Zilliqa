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
  bool commit(const uint64_t& dsBlockNum) {
    std::vector<dev::h256> toPurge;
    if (!OverlayDB::commit(KEEP_HISTORICAL_STATE, toPurge)) {
      LOG_GENERAL(WARNING, "OverlayDB::commit failed");
      return false;
    }

    if (!KEEP_HISTORICAL_STATE || !dsBlockNum) {
      return true;
    }

    // adding into purge pool
    if (!AddPendingPurge(dsBlockNum, toPurge)) {
      LOG_GENERAL(WARNING, "AddToPurge failed");
      return false;
    }

    // execute purge for expired keys
    if (!ExecutePurge(dsBlockNum)) {
      LOG_GENERAL(WARNING, "ExecutePurging failed");
      return false;
    }

    return true;
  }

 private:
  LevelDB m_purgeDB;

  bool AddPendingPurge(const uint64_t& dsBlockNum,
                       const std::vector<dev::h256>& toPurge) {
    LOG_MARKER();
    if (toPurge.empty()) {
      return true;
    }

    dev::RLPStream s(toPurge.size());
    for (const auto& it : toPurge) {
      LOG_GENERAL(INFO, "toPurge: " << it.hex());
      s.append(it);
    }

    std::stringstream keystream;
    keystream.fill('0');
    keystream.width(BLOCK_NUMERIC_DIGITS);
    keystream << std::to_string(dsBlockNum);

    return m_purgeDB.Insert(keystream.str(), s.out()) == 0;
  }
  bool ExecutePurge(const uint64_t& dsBlockNum) {
    LOG_MARKER();
    leveldb::Iterator* iter =
        m_purgeDB.GetDB()->NewIterator(leveldb::ReadOptions());
    iter->SeekToFirst();
    for (; iter->Valid(); iter->Next()) {
      uint64_t t_dsBlockNum;
      try {
        t_dsBlockNum = stoull(iter->key().ToString());
      } catch (...) {
        LOG_GENERAL(INFO, "key is not numeric: " << iter->key().ToString());
        return false;
      }

      std::vector<dev::h256> toPurge;
      if (t_dsBlockNum + NUM_DS_EPOCHS_STATE_HISTORY < dsBlockNum) {
        dev::RLP rlp(iter->value());
        std::vector<dev::h256> toPurge(rlp);

        for (const auto& t : toPurge) {
          LOG_GENERAL(INFO, "purging: " << t.hex());
        }

        m_levelDB.BatchDelete(toPurge);
        m_purgeDB.DeleteKey(iter->key().ToString());
      } else {
        break;
      }
    }

    return true;
  }
};

#endif  // ZILLIQA_SRC_LIBDATA_DATASTRUCTURES_TRACEABLEDB_H_