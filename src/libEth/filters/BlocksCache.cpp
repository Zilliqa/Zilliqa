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

#include "BlocksCache.h"

#include "FiltersUtils.h"
#include "libEth/Eth.h"
#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

using UniqueLock = std::unique_lock<std::shared_timed_mutex>;
using SharedLock = std::shared_lock<std::shared_timed_mutex>;

BlocksCache::BlocksCache(size_t depth, OnEpochFinalized epochFinalizedCallback)
    : m_depth(depth),
      m_epochFinalizedCallback(std::move(epochFinalizedCallback)) {
  assert(m_depth > 0);
  assert(m_epochFinalizedCallback);
}

EpochNumber BlocksCache::GetLastEpoch() {
  if (m_finalizedEpochs.empty()) {
    return SEEN_NOTHING;
  }
  return m_finalizedEpochs.back().epoch;
}

void BlocksCache::StartEpoch(uint64_t epoch, BlockHash block_hash,
                             uint32_t num_shards, uint32_t num_txns) {
  LOG_MARKER();
  EpochNumber n = static_cast<EpochNumber>(epoch);

  UniqueLock lock(m_mutex);

  if (n <= GetLastEpoch()) {
    LOG_GENERAL(WARNING, "Ignoring unexpected epoch number " << n);
    return;
  }

  if (m_epochsInProcess.count(epoch) != 0) {
    LOG_GENERAL(WARNING, "Ignoring existing epoch number " << n);
    return;
  }

  LOG_GENERAL(INFO, "m_finalizedEpochs size = "
                        << m_finalizedEpochs.size() << " m_depth = " << m_depth
                        << " epoch Num = " << epoch << " num_shards = "
                        << num_shards << " num txns = " << num_txns);

  if (num_txns == 0) {
    if (m_finalizedEpochs.size() >= m_depth) {
      LOG_GENERAL(INFO, "m_finalizedEpochs front = "
                            << m_finalizedEpochs.front().epoch << " hash = "
                            << m_finalizedEpochs.front().blockHash);
      m_finalizedEpochs.pop_front();
    }
    m_finalizedEpochs.emplace_back();
    auto &ctx = m_finalizedEpochs.back();
    ctx.epoch = n;
    ctx.blockHash = std::move(block_hash);

    m_epochFinalizedCallback(ctx);
  } else {
    auto &ctx = m_epochsInProcess[n];
    ctx.blockHash = std::move(block_hash);
    ctx.totalTxns = num_txns;
    ctx.shardsInProcess.resize(num_shards + 1);
  }
}

void BlocksCache::AddCommittedTransaction(uint64_t epoch, uint32_t shard,
                                          const TxnHash &hash,
                                          const Json::Value &receipt) {
  LOG_MARKER();
  LOG_GENERAL(INFO, "epoch = " << epoch << " shard = " << shard << " hash = "
                               << hash << " receipt = " << receipt);
  EpochNumber n = static_cast<EpochNumber>(epoch);

  UniqueLock lock(m_mutex);

  auto it = m_epochsInProcess.find(n);
  if (it == m_epochsInProcess.end()) {
    LOG_GENERAL(WARNING, "Unexpected epoch number " << n);
    return;
  }

  auto &ctx = it->second;
  if (shard >= ctx.shardsInProcess.size()) {
    LOG_GENERAL(WARNING, "Unexpected shard number " << shard);
    return;
  }

  auto &txn_list = ctx.shardsInProcess[shard];

  txn_list.emplace_back();
  ++ctx.currentTxns;
  auto &item = txn_list.back();

  item.hash = hash;

  std::string error;
  bool found = false;

  auto logs = ExtractArrayFromJsonObj(receipt, "event_logs", error);
  if (!error.empty()) {
    LOG_GENERAL(WARNING, "Error extracting event logs: " << error);
  }

  logs = Eth::ConvertScillaEventsToEvm(logs);
  for (const auto &event : logs) {
    ++ctx.totalLogs;

    item.events.emplace_back();
    auto &log = item.events.back();

    log.address = ExtractStringFromJsonObj(event, ADDRESS_STR, error, found);
    if (log.address.empty()) {
      LOG_GENERAL(WARNING, "Error extracting address of event log: " << error);
    }

    auto json_topics = ExtractArrayFromJsonObj(event, TOPICS_STR, error);
    if (!error.empty()) {
      LOG_GENERAL(WARNING, "Error extracting event log topics: " << error);
    }

    log.topics.reserve(json_topics.size());
    for (const auto &t : json_topics) {
      if (!t.isString()) {
        LOG_GENERAL(WARNING, "Event log topic is of wrong type");
        log.topics.clear();
        break;
      }
      log.topics.emplace_back(t.asString());
    }

    auto data = ExtractStringFromJsonObj(event, DATA_STR, error, found);
    if (data.empty()) {
      LOG_GENERAL(WARNING, "Error extracting event log data: " << error);
    }

    log.response =
        CreateEventResponseItem(n, hash, log.address, log.topics, data);
  }
  LOG_GENERAL(INFO, "ctx currentTxns = " << ctx.currentTxns
                                         << " totalTxns = " << ctx.totalTxns)
  if (ctx.currentTxns >= ctx.totalTxns) {
    TryFinalizeEpochs();
  }
}

void BlocksCache::TryFinalizeEpochs() {
  LOG_MARKER();
  if (!m_epochsInProcess.empty()) {
    LOG_GENERAL(INFO, "TryFinalizeEpochs start  m_epochsInProgress size = "
                          << m_epochsInProcess.size() << " first = "
                          << m_epochsInProcess.begin()->first << " last = "
                          << std::prev(m_epochsInProcess.end())->first);
  }
  while (!m_epochsInProcess.empty()) {
    auto it = m_epochsInProcess.begin();
    auto &ctx = it->second;
    if (ctx.currentTxns < ctx.totalTxns) {
      LOG_GENERAL(INFO, "bug m_epochsInProcess first = "<< it -> first<<" currentTxns = "<< ctx.currentTxns << " total txns = "<<ctx.totalTxns);
      break;
    }
    FinalizeOneEpoch(it->first, ctx);
    m_epochsInProcess.erase(it);
  }
  if (!m_epochsInProcess.empty()) {
    LOG_GENERAL(INFO, "TryFinalizeEpochs end  m_epochsInProgress size = "
                          << m_epochsInProcess.size() << " first = "
                          << m_epochsInProcess.begin()->first << " last = "
                          << std::prev(m_epochsInProcess.end())->first);
  }
}

void BlocksCache::FinalizeOneEpoch(EpochNumber n, EpochInProcess &data) {
  LOG_MARKER();
  if (m_finalizedEpochs.size() >= m_depth) {
    LOG_GENERAL(INFO, "Removing = m_finalizedEpochs  ="
                          << m_finalizedEpochs.front().epoch);
    m_finalizedEpochs.pop_front();
  }
  m_finalizedEpochs.emplace_back();
  auto &item = m_finalizedEpochs.back();

  item.epoch = n;
  item.blockHash = std::move(data.blockHash);

  if (data.totalLogs != 0) {
    item.meta.reserve(data.totalLogs);
    size_t txn_index = 0;
    size_t event_idx = 0;

    for (auto &shard : data.shardsInProcess) {
      for (auto &txn : shard) {
        for (auto &e : txn.events) {
          item.meta.emplace_back(std::move(e));
          auto &event = item.meta.back();
          event.response[LOGINDEX_STR] = NumberAsString(event_idx);
          event.response[BLOCKHASH_STR] = item.blockHash;
          event.response[TRANSACTIONINDEX_STR] = NumberAsString(txn_index);
          ++event_idx;
        }
        ++txn_index;
      }
    }
  }
  m_epochFinalizedCallback(item);
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
  LOG_MARKER();
  result.result = Json::Value(Json::arrayValue);
  result.success = true;

  SharedLock lock(m_mutex);

  auto last_epoch = GetLastEpoch();
  LOG_GENERAL(INFO,
              "last_epoch = " << last_epoch << " after_epoch = " << after_epoch
                              << " filter.fromBlock = " << filter.fromBlock
                              << " LATEST_EPOCH = " << LATEST_EPOCH);

  if (last_epoch <= after_epoch || filter.fromBlock == PENDING_EPOCH) {
    LOG_GENERAL(INFO, "Returning after_epoch = " << after_epoch);
    return after_epoch;
  }

  EpochNumber begin_epoch = after_epoch;
  if (filter.fromBlock > after_epoch + 1) {
    begin_epoch = filter.fromBlock - 1;
  } else if (filter.fromBlock == LATEST_EPOCH) {
    begin_epoch = last_epoch - 1;
  }

  EpochNumber end_epoch = std::numeric_limits<EpochNumber>::max();
  if (filter.toBlock >= 0) {
    end_epoch = filter.toBlock;
  }
  LOG_GENERAL(
      INFO, " from block = " << filter.fromBlock << " to block = " << end_epoch
                             << " it epoch = " << FindNext(begin_epoch)->epoch);
  for (auto it = FindNext(begin_epoch); it != m_finalizedEpochs.end(); ++it) {
    if (it->epoch > end_epoch) {
      break;
    }

    for (const auto &log : it->meta) {
      LOG_GENERAL(INFO, "here log.address = " << log.address);
      if (Match(filter, log.address, log.topics)) {
        LOG_GENERAL(INFO, "here");
        result.result.append(log.response);
      }
    }
  }
  LOG_GENERAL(INFO, "result = " << result.result.toStyledString());
  LOG_GENERAL(INFO, "last_epoch = " << last_epoch);

  return last_epoch;
}

EpochNumber BlocksCache::GetBlockFilterChanges(EpochNumber after_epoch,
                                               PollResult &result) {
  result.result = Json::Value(Json::arrayValue);
  result.success = true;

  SharedLock lock(m_mutex);

  auto last_epoch = GetLastEpoch();

  if (last_epoch <= after_epoch) {
    return after_epoch;
  }

  for (auto it = FindNext(after_epoch); it != m_finalizedEpochs.end(); ++it) {
    result.result.append(it->blockHash);
  }

  return last_epoch;
}

}  // namespace filters
}  // namespace evmproj
