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

#ifndef ZILLIQA_SRC_LIBUTILS_MEMORYPOOL_H_
#define ZILLIQA_SRC_LIBUTILS_MEMORYPOOL_H_

#include <libData/AccountData/Transaction.h>

#include "common/BaseType.h"

#include <memory>
#include <mutex>

class MemoryPool {
 public:
  static MemoryPool& GetInstance() {
    static MemoryPool instance;
    return instance;
  }

  std::shared_ptr<zbytes> GetZbytesFromPool() {
    std::unique_lock<std::mutex> lock{m_mutex};

    if (!std::empty(m_bytesPool)) {
      auto buffer = m_bytesPool.back();
      m_bytesPool.pop_back();
      return buffer;
    }

    // Arbitrary multiplier
    constexpr auto MULTIPLIER = 64;
    const auto size =
        Transaction::AVERAGE_TXN_SIZE_BYTES * TXN_STORAGE_LIMIT * MULTIPLIER;
    auto buffer = std::make_shared<zbytes>(size);
    return buffer;
  }
  void PutZbytesToPool(std::shared_ptr<zbytes> ptr) {
    std::unique_lock<std::mutex> lock{m_mutex};
    m_bytesPool.push_back(std::move(ptr));
  }

 private:
  MemoryPool(){};

 private:
  std::mutex m_mutex;
  std::vector<std::shared_ptr<zbytes>> m_bytesPool;
};

#endif  // ZILLIQA_SRC_LIBUTILS_MEMORYPOOL_H_
