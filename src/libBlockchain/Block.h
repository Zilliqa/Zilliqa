/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_

#include "common/Hashes.h"
#include "libCrypto/CoSignatures.h"

template <typename HeaderT, typename DataT>
class Block {
 public:
  const BlockHash& GetHash() const noexcept { return m_blockHash; }

  const HeaderT& GetHeader() const noexcept { return m_header; }

  std::chrono::system_clock::time_point GetTimestamp() const noexcept {
    return m_timestamp;
  }

 private:
  BlockHash m_blockHash;
  CoSignatures m_cosigs;
  // uint64_t m_timestamp = 0;
  std::chrono::system_clock::time_point m_timestamp =
      std::chrono::system_clock::now();
  HeaderT m_header;
};

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCK_H_
