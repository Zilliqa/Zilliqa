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

#include <algorithm>
#include <cassert>

#include "BlocksCache.h"

#include "FiltersUtils.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;

void BlocksCache::StartEpoch(EpochNumber epoch) {
  UniqueLock lock(m_mutex);

  assert(epoch > m_currentEpoch);

  if (epoch <= m_currentEpoch) {
    // TODO fatal exit
    return;
  }

  m_currentEpoch = epoch;
}

void BlocksCache::AddCommittedTransaction(uint32_t shard, const TxnHash &hash,
                                          const Json::Value &receipt) {
  // TODO get it from parameters or constants or original design document
  static const uint32_t MAX_SHARDS = 1024;

  if (shard > MAX_SHARDS) {
    // TODO fatal exit
    return;
  }

  UniqueLock lock(m_mutex);

  if (m_currentEpoch < 0) {
    // TODO fatal exit
    return;
  }

  if (m_shardsInProcess.size() < shard + 1) {
    m_shardsInProcess.resize(shard + 1);
  }

  auto &txn_list = m_shardsInProcess[shard];

  txn_list.emplace_back();
  auto &item = txn_list.back();

  item.hash = hash;

  std::string error;
  bool found = false;

  auto logs = ExtractArrayFromJsonObj(receipt, "event_logs", error);
  if (!error.empty()) {
    // log...
    // TODO handle errors
  }

  for (const auto &event : logs) {
    ++m_numLogsInEpoch;
    item.events.emplace_back();
    auto &log = item.events.back();

    log.address = ExtractStringFromJsonObj(event, ADDRESS_STR, error, found);
    // TODO handle errors

    auto json_topics = ExtractArrayFromJsonObj(event, TOPICS_STR, error);
    // TODO handle errors

    log.topics.reserve(json_topics.size());
    for (const auto &t : json_topics) {
      if (!t.isString()) {
        // TODO handle errors
        return;
      }
      log.topics.emplace_back(t.asString());
    }

    auto data = ExtractArrayFromJsonObj(event, DATA_STR, error);
    // TODO handle errors

    log.response = CreateEventResponseItem(m_currentEpoch, hash, log.address,
                                           log.topics, data);
  }
}

void BlocksCache::FinalizeEpoch(BlockHash blockHash,
                                EpochNumber cleanup_before) {
  UniqueLock lock(m_mutex);

  CleanupOldEpochs(cleanup_before);

  m_finalizedEpochs.emplace_back();
  auto &item = m_finalizedEpochs.back();
  item.epoch = m_currentEpoch;
  item.blockHash = blockHash;

  if (m_numLogsInEpoch != 0) {
    item.meta.reserve(m_numLogsInEpoch);
    size_t txn_index = 0;
    size_t event_idx = 0;

    for (auto &shard : m_shardsInProcess) {
      for (auto &txn : shard) {
        for (auto &e : txn.events) {
          item.meta.emplace_back(std::move(e));
          auto &event = item.meta.back();
          event.response[LOGINDEX_STR] = NumberAsString(event_idx);
          event.response[BLOCKHASH_STR] = blockHash;
          event.response[TRANSACTIONINDEX_STR] = NumberAsString(txn_index);
          ++event_idx;
        }
        ++txn_index;
      }
    }
  }

  m_numLogsInEpoch = 0;
  m_shardsInProcess.clear();
}

void BlocksCache::CleanupOldEpochs(EpochNumber cleanup_before) {
  auto begin = m_finalizedEpochs.begin();
  auto it = begin;
  for (; it != m_finalizedEpochs.end(); ++it) {
    if (it->epoch >= cleanup_before) {
      break;
    }
    // TODO log here
  }
  if (it != begin) {
    m_finalizedEpochs.erase(begin, it);
  }
}

BlocksCache::FinalizedEpochs::iterator BlocksCache::FindNext(
    EpochNumber after_epoch) {
  EpochMetadata item;
  item.epoch = after_epoch;

  return std::upper_bound(m_finalizedEpochs.begin(), m_finalizedEpochs.end(),
                          item,
                          [](const EpochMetadata &a, const EpochMetadata &b) {
                            return a.epoch < b.epoch;
                          });
}

EpochNumber BlocksCache::GetEventFilterChanges(EpochNumber after_epoch,
                                               const EventFilterParams &filter,
                                               PollResult &result) {
  result.result = Json::Value(Json::arrayValue);
  result.success = true;

  SharedLock lock(m_mutex);

  // TODO fromBlock and toBlock proper handling - set them properly in
  // FilterAPIBackend

  if (after_epoch >= m_currentEpoch - 1) {
    return after_epoch;
  }

  auto last_seen = 0;

  for (auto it = FindNext(after_epoch); it != m_finalizedEpochs.end(); ++it) {
    // TODO break if toBlock < height

    for (const auto &log : it->meta) {
      if (Match(filter, log.address, log.topics)) {
        result.result.append(log.response);
      }
    }
    last_seen = it->epoch;
  }

  return last_seen;
}

EpochNumber BlocksCache::GetBlockFilterChanges(EpochNumber after_epoch,
                                               PollResult &result) {
  result.result = Json::Value(Json::arrayValue);
  result.success = true;

  SharedLock lock(m_mutex);

  if (after_epoch >= m_currentEpoch - 1) {
    return after_epoch;
  }

  auto last_seen = 0;

  for (auto it = FindNext(after_epoch); it != m_finalizedEpochs.end(); ++it) {
    result.result.append(it->blockHash);
    last_seen = it->epoch;
  }

  return last_seen;
}

}  // namespace filters
}  // namespace evmproj
