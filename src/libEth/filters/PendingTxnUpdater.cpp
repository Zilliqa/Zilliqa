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

#include "PendingTxnUpdater.h"

#include <cassert>

#include "libEth/Filters.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"

namespace evmproj::filters {

PendingTxnUpdater::PendingTxnUpdater(Mediator& mediator)
    : m_mediator(mediator), m_thread([this] { WorkerThread(); }) {
  assert(m_mediator.m_lookup);
  assert(m_mediator.m_filtersAPICache);
}

PendingTxnUpdater::~PendingTxnUpdater() {
  {
    std::lock_guard lk(m_mutex);
    m_stopped = true;
  }
  m_cond.notify_all();
  m_thread.join();
}

bool PendingTxnUpdater::Wait() {
  std::unique_lock lk(m_mutex);
  if (!m_stopped) {
    m_cond.wait_for(lk, UPDATE_INTERVAL);
  }
  return !m_stopped;
}

void PendingTxnUpdater::WorkerThread() {
  auto& update = m_mediator.m_filtersAPICache->GetUpdate();

  while (Wait()) {
    auto txns = m_mediator.m_lookup->GetDSLeaderTxnPool();
    if (txns.has_value()) {
      for (const auto& t : txns.value()) {
        update.AddPendingTransaction(t.GetTranID().hex(), 0);
      }
    }
  }
}

}  // namespace evmproj::filters
