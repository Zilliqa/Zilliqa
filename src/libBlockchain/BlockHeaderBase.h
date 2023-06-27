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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADERBASE_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADERBASE_H_

#include "common/Constants.h"
#include "common/Hashes.h"
#include "common/Serializable.h"

constexpr const uint64_t INIT_BLOCK_NUMBER = (uint64_t)-1;

/// [TODO] Base class for all supported block header types
class BlockHeaderBase : public SerializableDataBlock {
 protected:
  // TODO: pull out all common code from ds, micro and tx block header
  uint32_t m_version;
  CommitteeHash m_committeeHash;
  BlockHash m_prevHash;

 public:
  // Constructors
  BlockHeaderBase(uint32_t version = 0,
                  const CommitteeHash& committeeHash = CommitteeHash{},
                  const BlockHash& prevHash = BlockHash{})
      : m_version(version),
        m_committeeHash(committeeHash),
        m_prevHash(prevHash) {
    std::cout << "argh" << std::endl;
  }

  /// Calculate my hash
  BlockHash GetMyHash() const;

  /// Returns the current version of this block.
  uint32_t GetVersion() const { return m_version; }

  /// Returns the hash of the committee where the block was generated
  const CommitteeHash& GetCommitteeHash() const { return m_committeeHash; }

  const BlockHash& GetPrevHash() const { return m_prevHash; }

 protected:
  // Operators
  bool operator==(const BlockHeaderBase& header) const;
};

std::ostream& operator<<(std::ostream& os, const BlockHeaderBase& t);

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_BLOCKHEADERBASE_H_
