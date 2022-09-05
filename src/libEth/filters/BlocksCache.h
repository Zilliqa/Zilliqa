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

#include <mutex>
#include <shared_mutex>

#include "Common.h"

namespace evmproj {
namespace filters {

class BlocksCache {
 public:
  void StartEpoch(EpochNumber epoch);

  void AddCommittedTransaction(uint32_t shard, const TxnHash &hash,
                               const Json::Value &receipt);

  void FinalizeEpoch(BlockHash blockHash, EpochNumber cleanup_before);

  EpochNumber GetEventFilterChanges(EpochNumber after_epoch,
                                    const EventFilterParams &filter,
                                    PollResult &result);

  EpochNumber GetBlockFilterChanges(EpochNumber after_epoch,
                                    PollResult &result);

 private:
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

  struct TransactionAndEvents {
    TxnHash hash;
    std::vector<EventLog> events;
  };

  using FinalizedEpochs = std::deque<EpochMetadata>;

  void CleanupOldEpochs(EpochNumber cleanup_before);

  FinalizedEpochs::iterator FindNext(EpochNumber after_epoch);

  EpochNumber m_currentEpoch = SEEN_NOTHING;
  size_t m_numLogsInEpoch = 0;
  std::vector<std::vector<TransactionAndEvents>> m_shardsInProcess;
  FinalizedEpochs m_finalizedEpochs;
  std::shared_timed_mutex m_mutex;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_BLOCKSCACHE_H_
