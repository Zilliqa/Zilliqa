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
#include "PendingTxnCache.h"

namespace evmproj {
namespace filters {

namespace {

// TODO this is to be a parameter from constants.xml
const EpochNumber TXMETADATADEPTH = 100;

// TODO stub
class SubscriptionAPIBackendImpl : public SubscriptionAPIBackend {};

}  // namespace

class CacheImpl : public CacheUpdate, public TxCache {
 public:
  CacheImpl() : m_filterAPI(*this) {}

  FilterAPIBackend& GetFilterAPI() { return m_filterAPI; }

  SubscriptionAPIBackend& GetSubscriptionAPI() { return m_subscriptionAPI; }

 private:
  void AddPendingTransaction(const TxnHash& hash, uint64_t epoch) override {
    m_pendingTxnCache.Append(hash, epoch);
    // TODO subscriptions
  }

  void StartEpoch(uint64_t epoch) override {
    auto native_epoch = static_cast<EpochNumber>(epoch);
    m_currentEpoch = native_epoch;
    if (m_earliestEpoch < 0) {
      m_earliestEpoch = m_currentEpoch;
    }
  }

  void AddCommittedTransaction(uint32_t shard, const TxnHash& hash,
                               const Json::Value& receipt) override {
    m_blocksCache.AddCommittedTransaction(shard, hash, receipt);
  }

  void FinalizeEpoch(BlockHash blockHash) override {
    if (m_currentEpoch - m_earliestEpoch > TXMETADATADEPTH) {
      m_earliestEpoch = m_currentEpoch - TXMETADATADEPTH;
      m_pendingTxnCache.CleanupBefore(m_earliestEpoch);
    }
    m_filterAPI.SetEpochRange(m_earliestEpoch, m_currentEpoch);
    m_blocksCache.FinalizeEpoch(blockHash, m_earliestEpoch);
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

  // XXX seems no need to sync these values
  EpochNumber m_earliestEpoch = SEEN_NOTHING;
  EpochNumber m_currentEpoch = SEEN_NOTHING;

  FilterAPIBackendImpl m_filterAPI;
  SubscriptionAPIBackendImpl m_subscriptionAPI;
  PendingTxnCache m_pendingTxnCache;
  BlocksCache m_blocksCache;
};

namespace {

CacheImpl& GetCacheInstance() {
  static CacheImpl cache;
  return cache;
}

}  // namespace

FilterAPIBackend& TxMetadata::GetFilterAPI() {
  return GetCacheInstance().GetFilterAPI();
}

SubscriptionAPIBackend& TxMetadata::GetSubscriptionAPI() {
  return GetCacheInstance().GetSubscriptionAPI();
}

CacheUpdate& TxMetadata::GetCacheUpdate() { return GetCacheInstance(); }

}  // namespace filters
}  // namespace evmproj
