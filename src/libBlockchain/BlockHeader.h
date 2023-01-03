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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADER_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADER_H_

#include "common/Hashes.h"
#include "libCrypto/CoSignatures.h"

namespace zil {

template <typename DataT>
class BlockHeader {
 public:
  const CommitteeHash& GetCommitteeHash() const noexcept {
    return m_committeeHash;
  }
  const BlockHash& GetPrevHash() const noexcept { return m_prevHash; }

 private:
  uint32_t m_version = 0;
  CommitteeHash m_committeeHash;
  BlockHash m_prevHash;
};

}  // namespace zil

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADER_H_
