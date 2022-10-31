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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_BLOCKSCACHE_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_BLOCKSCACHE_H_

#include <functional>
#include <mutex>
#include <shared_mutex>

#include "Common.h"

namespace evmproj {
namespace filters {

class BlocksCache {
 public:
  struct EventLog {
    Address address;
    std::vector<Quantity> topics;
    Json::Value response;
  };

  struct EpochMetadata {
    EpochNumber epoch = SEEN_NOTHING;
    BlockHash blockHash;
    std::vector<EventLog> meta;
  };

  using OnEpochFinalized = std::function<void(const EpochMetadata &)>;

  BlocksCache(size_t depth, OnEpochFinalized epochFinalizedCallback);

  ///
  /// \param epoch
  /// \param block_hash
  /// \param num_shards
  /// \param num_txns
  /// \return true if the meta for this epoch is finalized
  void StartEpoch(uint64_t epoch, BlockHash block_hash, uint32_t num_shards,
                  uint32_t num_txns);

  void AddCommittedTransaction(uint64_t epoch, uint32_t shard,
                               const TxnHash &hash, const Json::Value &receipt);

  EpochNumber GetEventFilterChanges(EpochNumber after_epoch,
                                    const EventFilterParams &filter,
                                    PollResult &result);

  EpochNumber GetBlockFilterChanges(EpochNumber after_epoch,
                                    PollResult &result);

 private:
  struct TransactionAndEvents {
    TxnHash hash;
    std::vector<EventLog> events;
  };

  struct EpochInProcess {
    /// TX block hash
    BlockHash blockHash;

    /// Total # of transactions in this TX epoch
    uint32_t totalTxns = 0;

    uint32_t currentTxns = 0;

    size_t totalLogs = 0;

    /// Transactions metadata per shards
    std::vector<std::vector<TransactionAndEvents>> shardsInProcess;
  };

  using FinalizedEpochs = std::deque<EpochMetadata>;

  // void CleanupOldEpochs(EpochNumber cleanup_before);

  FinalizedEpochs::iterator FindNext(EpochNumber after_epoch);

  /// Tries to finalize unfinished meta, returns true if current epoch advanced
  void TryFinalizeEpochs();

  void FinalizeOneEpoch(EpochNumber n, EpochInProcess &data);

  EpochNumber GetLastEpoch();

  const size_t m_depth;
  OnEpochFinalized m_epochFinalizedCallback;
  std::map<EpochNumber, EpochInProcess> m_epochsInProcess;
  FinalizedEpochs m_finalizedEpochs;
  std::shared_timed_mutex m_mutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_BLOCKSCACHE_H_
