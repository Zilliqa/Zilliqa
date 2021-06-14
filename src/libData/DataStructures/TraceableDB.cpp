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

#include "libData/DataStructures/TraceableDB.h"
#include "libUtils/DetachedFunction.h"

using namespace std;

bool TraceableDB::commit(const uint64_t& dsBlockNum) {
  std::vector<dev::h256> toPurge;
  if (!OverlayDB::commit(KEEP_HISTORICAL_STATE && LOOKUP_NODE_MODE, toPurge)) {
    LOG_GENERAL(WARNING, "OverlayDB::commit failed");
    return false;
  }

  if (!(KEEP_HISTORICAL_STATE && LOOKUP_NODE_MODE) || !dsBlockNum) {
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

bool TraceableDB::AddPendingPurge(const uint64_t& dsBlockNum,
                                  const std::vector<dev::h256>& toPurge) {
  LOG_MARKER();
  if (toPurge.empty()) {
    return true;
  }

  dev::RLPStream s(toPurge.size());
  for (const auto& it : toPurge) {
    if (LOG_SC) {
      LOG_GENERAL(INFO,
                  "toPurge: " << it.hex() << " dsBlockNum: " << dsBlockNum);
    }
    s.append(it);
  }

  std::stringstream keystream;
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << std::to_string(dsBlockNum);

  return m_purgeDB.Insert(keystream.str(), s.out()) == 0;
}

bool TraceableDB::ExecutePurge(const uint64_t& dsBlockNum, bool purgeAll) {
  LOG_MARKER();

  leveldb::Iterator* iter =
      m_purgeDB.GetDB()->NewIterator(leveldb::ReadOptions());
  iter->SeekToFirst();
  for (; iter->Valid(); iter->Next()) {
    if (purgeAll && m_stopSignal) {
      LOG_GENERAL(WARNING, "m_stopSignal = true");
      break;
    }
    uint64_t t_dsBlockNum;
    try {
      t_dsBlockNum = stoull(iter->key().ToString());
    } catch (...) {
      LOG_GENERAL(INFO, "key is not numeric: " << iter->key().ToString());
      return false;
    }

    // If purgeAll = true, dsBlockNum is inconsequential
    if ((t_dsBlockNum + NUM_DS_EPOCHS_STATE_HISTORY < dsBlockNum) || purgeAll) {
      dev::RLP rlp(iter->value());
      std::vector<dev::h256> toPurge(rlp);

      if (LOG_SC) {
        for (const auto& t : toPurge) {
          LOG_GENERAL(INFO, "purging: " << t.hex()
                                        << " t_dsBlockNum: " << t_dsBlockNum);
        }
      }

      m_levelDB.BatchDelete(toPurge);
      m_purgeDB.DeleteKey(iter->key().ToString());
    }
  }

  return true;
}

bool TraceableDB::RefreshDB() {
  return m_levelDB.RefreshDB() && m_purgeDB.RefreshDB();
}

void TraceableDB::DetachedExecutePurge() {
  LOG_MARKER();
  auto detached_func = [this]() -> void {
    if (!m_purgeRunning) {
      m_stopSignal = false;
      m_purgeRunning = true;
      ExecutePurge(0, true);
      m_purgeRunning = false;
      m_stopSignal = false;
    } else {
      LOG_GENERAL(INFO, "DetachedExecutePurge already running");
    }
  };
  DetachedFunction(1, detached_func);
}