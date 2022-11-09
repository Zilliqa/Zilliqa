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

#include "BlocksCache.h"
#include "FiltersImpl.h"
#include "FiltersUtils.h"
#include "PendingTxnCache.h"
#include "SubscriptionsImpl.h"
#include "libUtils/Logger.h"

namespace evmproj {
namespace filters {

namespace {

// TODO this is to be a parameter from constants.xml
const size_t TXMETADATADEPTH = 100;

}  // namespace

class APICacheImpl : public APICache, public APICacheUpdate, public TxCache {
 public:
  APICacheImpl()
      : m_filterAPI(*this),
        m_pendingTxnCache(TXMETADATADEPTH),
        m_blocksCache(TXMETADATADEPTH,
                      [this](const BlocksCache::EpochMetadata& meta) {
                        EpochFinalized(meta);
                      }) {}

 private:
  FilterAPIBackend& GetFilterAPI() override { return m_filterAPI; }

  APICacheUpdate& GetUpdate() override { return *this; }

  void EnableWebsocketAPI(std::shared_ptr<WebsocketServer> ws,
                          BlockByHash blockByHash) override {
    m_subscriptions.Start(std::move(ws), std::move(blockByHash));
  }

  void AddPendingTransaction(const TxnHash& hash, uint64_t epoch) override {
    auto hash_normalized = NormalizeHexString(hash);
    m_pendingTxnCache.Append(hash_normalized, epoch);
    m_subscriptions.OnPendingTransaction(hash_normalized);
  }

  void StartEpoch(uint64_t epoch, const BlockHash& block_hash,
                  uint32_t num_shards, uint32_t num_txns) override {
    m_blocksCache.StartEpoch(epoch, NormalizeHexString(block_hash), num_shards,
                             num_txns);
  }

  void AddCommittedTransaction(uint64_t epoch, uint32_t shard,
                               const TxnHash& hash,
                               const Json::Value& receipt) override {
    auto hash_normalized = NormalizeHexString(hash);
    m_blocksCache.AddCommittedTransaction(epoch, shard, hash_normalized,
                                          receipt);
    m_pendingTxnCache.TransactionCommitted(std::move(hash_normalized));
  }

  EpochNumber GetEventFilterChanges(EpochNumber after_epoch,
                                    const EventFilterParams& filter,
                                    PollResult& result) override {
    return m_blocksCache.GetEventFilterChanges(after_epoch, filter, result);
  }

  EpochNumber GetBlockFilterChanges(EpochNumber after_epoch,
                                    PollResult& result) override {
    return m_blocksCache.GetBlockFilterChanges(after_epoch, result);
  }

  EpochNumber GetPendingTxnsFilterChanges(EpochNumber after_counter,
                                          PollResult& result) override {
    return m_pendingTxnCache.GetPendingTxnsFilterChanges(after_counter, result);
  }

  void EpochFinalized(const BlocksCache::EpochMetadata& meta) {
    uint64_t epoch = meta.epoch;
    LOG_GENERAL(INFO, "Finalized epoch " << epoch);

    m_subscriptions.OnNewHead(meta.blockHash);
    for (const auto& event : meta.meta) {
      m_subscriptions.OnEventLog(event.address, event.topics, event.response);
    }

    auto earliest = epoch <= TXMETADATADEPTH ? 0 : epoch - TXMETADATADEPTH;
    m_filterAPI.SetEpochRange(earliest, epoch);
  }

  FilterAPIBackendImpl m_filterAPI;
  SubscriptionsImpl m_subscriptions;
  PendingTxnCache m_pendingTxnCache;
  BlocksCache m_blocksCache;
};

std::shared_ptr<APICache> APICache::Create() {
  return std::make_shared<APICacheImpl>();
}

}  // namespace filters
}  // namespace evmproj
