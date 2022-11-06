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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_COMMON_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_COMMON_H_

#include "../Filters.h"

namespace evmproj {
namespace filters {

enum class FilterType {
  INVALID,
  EVENT_FILTER,
  TXN_FILTER,
  BLK_FILTER,
};

using Quantity = std::string;
using Address = std::string;
using TxnHash = std::string;
using BlockHash = std::string;
using FilterId = std::string;
using EpochNumber = int64_t;

// special values for LastSeen fields
static const EpochNumber SEEN_NOTHING = -1;
static const EpochNumber EARLIEST_EPOCH = -4;
static const EpochNumber LATEST_EPOCH = -3;
static const EpochNumber PENDING_EPOCH = -2;

struct EventFilterParams {
  /// Earliest epoch number to which this filter applies
  EpochNumber fromBlock = SEEN_NOTHING;

  /// Latest epoch number to which this filter applies
  EpochNumber toBlock = SEEN_NOTHING;

  /// Filter events emitted from this address. Empty address means that
  /// every address matches this filter
  Address address;

  /// **OR** logic. Empty vector means that any value matches
  using TopicMatchVariants = std::vector<Quantity>;

  /// *AND* logic. Up to 4 topics. Empty topic at position i (0..3) matches
  /// any value
  std::vector<TopicMatchVariants> topicMatches;
};

class TxCache {
 public:
  virtual ~TxCache() = default;

  virtual EpochNumber GetEventFilterChanges(EpochNumber after_epoch,
                                            const EventFilterParams &filter,
                                            PollResult &result) = 0;

  virtual EpochNumber GetBlockFilterChanges(EpochNumber after_epoch,
                                            PollResult &result) = 0;

  virtual EpochNumber GetPendingTxnsFilterChanges(EpochNumber after_counter,
                                                  PollResult &result) = 0;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_COMMON_H_
