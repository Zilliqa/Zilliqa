/*
 * Copyright (C) 2023 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNUPDATER_H_
#define ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNUPDATER_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

class Mediator;

namespace evmproj::filters {

class APICacheUpdate;

/// A workaround which periodically pulls txn pool from DS leader from a
/// separate tread and updates Filters/Subscriptions cache
class PendingTxnUpdater {
 public:
  static constexpr std::chrono::seconds UPDATE_INTERVAL{5};

  /// Starts updating
  explicit PendingTxnUpdater(Mediator& mediator);

  /// Stops and joins the worker thread
  ~PendingTxnUpdater();

 private:
  bool Wait();

  void WorkerThread();

  Mediator& m_mediator;
  std::thread m_thread;
  bool m_stopped = false;
  std::condition_variable m_cond;
  std::mutex m_mutex;
};

}  // namespace evmproj::filters

#endif  // ZILLIQA_SRC_LIBETH_FILTERS_PENDINGTXNUPDATER_H_
