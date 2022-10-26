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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_H_

#include <functional>
#include <memory>

#include <json/json.h>

namespace evmproj {

class WebsocketServer;

namespace filters {

using TxnHash = std::string;
using BlockHash = std::string;
using FilterId = std::string;

/// Result of filter changes API calls
struct PollResult {
  bool success = false;

  Json::Value result;

  std::string error;
};

class FilterAPIBackend {
 public:
  virtual ~FilterAPIBackend() = default;

  /// Result of InstallNew*Filter calls
  struct InstallResult {
    /// Set to true if filter has been installed
    bool success = false;

    /// Depending on success, either filter ID or error message
    std::string result;
  };

  /// Called from TxMeta backend on init and on epoch switching
  virtual void SetEpochRange(uint64_t earliest, uint64_t latest) = 0;

  /// Backend entry for eth_newFilter
  virtual InstallResult InstallNewEventFilter(const Json::Value &params) = 0;

  /// Backend entry for eth_newBlockFilter
  virtual InstallResult InstallNewBlockFilter() = 0;

  /// Backend entry for eth_newPendingTransactionsFilter
  virtual InstallResult InstallNewPendingTxnFilter() = 0;

  /// Backend entry for eth_uninstallFilter
  /// Uninstalls filter and returns: true on success, false if filter_id was
  /// not installed (or expired and not found)
  virtual bool UninstallFilter(const FilterId &filter_id) = 0;

  /// Backend entry for eth_getFilterChanges
  /// Returns changes since the last poll
  virtual PollResult GetFilterChanges(const FilterId &filter_id) = 0;

  /// Backend entry for eth_getFilterLogs
  /// Almost the same as GetFilterChanges, but returns all items subject to the
  /// filter, ignoring 'last seen' internal cursor
  virtual PollResult GetFilterLogs(const FilterId &filter_id) = 0;

  /// Backend entry for eth_getLogs
  /// Stateless version of event filters polling
  virtual PollResult GetLogs(const Json::Value &params) = 0;
};

class APICacheUpdate {
 public:
  virtual ~APICacheUpdate() = default;

  virtual void AddPendingTransaction(const TxnHash &hash, uint64_t epoch) = 0;

  virtual void StartEpoch(uint64_t epoch, const BlockHash &block_hash,
                          uint32_t num_shards, uint32_t num_txns) = 0;

  virtual void AddCommittedTransaction(uint64_t epoch, uint32_t shard,
                                       const TxnHash &hash,
                                       const Json::Value &receipt) = 0;
};

class APICache {
 public:
  /// Injected function that creates block json response by hash
  using BlockByHash = std::function<Json::Value(const BlockHash &)>;

  /// Creates an instance of default TxMetadata implementation
  static std::shared_ptr<APICache> Create();

  virtual ~APICache() = default;
  virtual FilterAPIBackend &GetFilterAPI() = 0;
  virtual APICacheUpdate &GetUpdate() = 0;
  virtual void EnableWebsocketAPI(std::shared_ptr<WebsocketServer> ws,
                                  BlockByHash blockByHash) = 0;
};

}  // namespace filters
}  // namespace evmproj

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_H_
