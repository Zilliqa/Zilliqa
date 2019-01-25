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

#ifndef __BLOCKHASHSET_H__
#define __BLOCKHASHSET_H__

#include "common/BaseType.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"

// Hashes for DSBlockHashSet
using ShardingHash = dev::h256;
using TxSharingHash = dev::h256;

struct DSBlockHashSet {
  ShardingHash m_shardingHash;  // Hash of sharding structure
  std::array<unsigned char, RESERVED_FIELD_SIZE>
      m_reservedField;  // Reserved storage for extra hashes

  bool operator==(const DSBlockHashSet& hashSet) const {
    return std::tie(m_shardingHash) == std::tie(hashSet.m_shardingHash);
  }
  bool operator<(const DSBlockHashSet& hashSet) const {
    return std::tie(hashSet.m_shardingHash) > std::tie(m_shardingHash);
  }
  bool operator>(const DSBlockHashSet& hashSet) const {
    return hashSet < *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const DSBlockHashSet& t);
};

inline std::ostream& operator<<(std::ostream& os, const DSBlockHashSet& t) {
  std::string reservedFieldStr;
  if (!DataConversion::charArrToHexStr(t.m_reservedField, reservedFieldStr)) {
    os << "";
    return os;
  }
  os << "<DSBlockHashSet>" << std::endl
     << " m_shardingHash  = " << t.m_shardingHash.hex() << std::endl
     << " m_reservedField = " << reservedFieldStr;
  return os;
}

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<DSBlockHashSet> {
  size_t operator()(DSBlockHashSet const& hashSet) const noexcept {
    std::size_t seed = 0;
    std::string reservedFieldStr;
    if (!DataConversion::charArrToHexStr(hashSet.m_reservedField,
                                         reservedFieldStr)) {
      return seed;
    }
    boost::hash_combine(seed, hashSet.m_shardingHash.hex());
    boost::hash_combine(seed, reservedFieldStr);
    return seed;
  }
};
}  // namespace std

struct MicroBlockHashSet {
  TxnHash m_txRootHash;        // Tx merkle tree root hash
  StateHash m_stateDeltaHash;  // State Delta hash
  TxnHash m_tranReceiptHash;   // Tx Receipt hash

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const {
    copy(m_txRootHash.asArray().begin(), m_txRootHash.asArray().end(),
         dst.begin() + offset);
    offset += TRAN_HASH_SIZE;
    copy(m_stateDeltaHash.asArray().begin(), m_stateDeltaHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;
    copy(m_tranReceiptHash.asArray().begin(), m_tranReceiptHash.asArray().end(),
         dst.begin() + offset);
    offset += TRAN_HASH_SIZE;

    return offset;
  }

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset) {
    try {
      copy(src.begin() + offset, src.begin() + offset + TRAN_HASH_SIZE,
           m_txRootHash.asArray().begin());
      offset += TRAN_HASH_SIZE;
      copy(src.begin() + offset, src.begin() + offset + STATE_HASH_SIZE,
           m_stateDeltaHash.asArray().begin());
      offset += STATE_HASH_SIZE;
      copy(src.begin() + offset, src.begin() + offset + TRAN_HASH_SIZE,
           m_tranReceiptHash.asArray().begin());
      offset += TRAN_HASH_SIZE;
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Error with MicroBlockHashSet::Deserialize."
                               << ' ' << e.what());
      return -1;
    }

    return 0;
  }

  static constexpr unsigned int size() {
    return TRAN_HASH_SIZE + STATE_HASH_SIZE + TRAN_HASH_SIZE;
  }

  bool operator==(const MicroBlockHashSet& hashSet) const {
    return std::tie(m_txRootHash, m_stateDeltaHash, m_tranReceiptHash) ==
           std::tie(hashSet.m_txRootHash, hashSet.m_stateDeltaHash,
                    hashSet.m_tranReceiptHash);
  }
  bool operator<(const MicroBlockHashSet& hashSet) const {
    return std::tie(hashSet.m_txRootHash, hashSet.m_stateDeltaHash,
                    hashSet.m_tranReceiptHash) >
           std::tie(m_txRootHash, m_stateDeltaHash, m_tranReceiptHash);
  }
  bool operator>(const MicroBlockHashSet& hashSet) const {
    return hashSet < *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const MicroBlockHashSet& t);
};

inline std::ostream& operator<<(std::ostream& os, const MicroBlockHashSet& t) {
  os << "<MicroBlockHashSet>" << std::endl
     << " m_txRootHash      = " << t.m_txRootHash.hex() << std::endl
     << " m_stateDeltaHash  = " << t.m_stateDeltaHash.hex() << std::endl
     << " m_tranReceiptHash = " << t.m_tranReceiptHash.hex();
  return os;
}

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<MicroBlockHashSet> {
  size_t operator()(MicroBlockHashSet const& hashSet) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, hashSet.m_txRootHash.hex());
    boost::hash_combine(seed, hashSet.m_stateDeltaHash.hex());
    boost::hash_combine(seed, hashSet.m_tranReceiptHash.hex());

    return seed;
  }
};
}  // namespace std

using MBInfoHash = dev::h256;

struct TxBlockHashSet {
  StateHash m_stateRootHash;   // State merkle tree root hash only valid in
                               // vacuous epoch
  StateHash m_stateDeltaHash;  // State Delta Hash on DS
  MBInfoHash m_mbInfoHash;     // Hash concatenated from all microblock infos

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const {
    copy(m_stateRootHash.asArray().begin(), m_stateRootHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;
    copy(m_stateDeltaHash.asArray().begin(), m_stateDeltaHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;
    copy(m_mbInfoHash.asArray().begin(), m_mbInfoHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;
    return offset;
  }

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset) {
    try {
      copy(src.begin() + offset, src.begin() + offset + STATE_HASH_SIZE,
           m_stateRootHash.asArray().begin());
      offset += STATE_HASH_SIZE;
      copy(src.begin() + offset, src.begin() + offset + STATE_HASH_SIZE,
           m_stateDeltaHash.asArray().begin());
      offset += STATE_HASH_SIZE;
      copy(src.begin() + offset, src.begin() + offset + STATE_HASH_SIZE,
           m_mbInfoHash.asArray().begin());
      offset += STATE_HASH_SIZE;
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING,
                  "Error with TxBlockHashSet::Deserialize." << ' ' << e.what());
      return -1;
    }

    return 0;
  }

  bool operator==(const TxBlockHashSet& hashSet) const {
    return std::tie(m_stateRootHash, m_stateDeltaHash, m_mbInfoHash) ==
           std::tie(hashSet.m_stateRootHash, hashSet.m_stateDeltaHash,
                    hashSet.m_mbInfoHash);
  }
  bool operator<(const TxBlockHashSet& hashSet) const {
    return std::tie(hashSet.m_stateRootHash, hashSet.m_stateDeltaHash,
                    hashSet.m_mbInfoHash) >
           std::tie(m_stateRootHash, m_stateDeltaHash, m_mbInfoHash);
  }
  bool operator>(const TxBlockHashSet& hashSet) const {
    return hashSet < *this;
  }

  static constexpr unsigned int size() {
    return STATE_HASH_SIZE + STATE_HASH_SIZE + STATE_HASH_SIZE;
  }

  friend std::ostream& operator<<(std::ostream& os, const TxBlockHashSet& t);
};

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<TxBlockHashSet> {
  size_t operator()(TxBlockHashSet const& hashSet) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, hashSet.m_stateRootHash.hex());
    boost::hash_combine(seed, hashSet.m_stateDeltaHash.hex());
    boost::hash_combine(seed, hashSet.m_mbInfoHash.hex());

    return seed;
  }
};
}  // namespace std

inline std::ostream& operator<<(std::ostream& os, const TxBlockHashSet& t) {
  os << "<TxBlockHashSet>" << std::endl
     << " m_stateRootHash  = " << t.m_stateRootHash.hex() << std::endl
     << " m_stateDeltaHash = " << t.m_stateDeltaHash.hex() << std::endl
     << " m_mbInfoHash     = " << t.m_mbInfoHash.hex();
  return os;
}

struct FallbackBlockHashSet {
  StateHash m_stateRootHash;

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(bytes& dst, unsigned int offset) const {
    copy(m_stateRootHash.asArray().begin(), m_stateRootHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;

    return offset;
  }

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const bytes& src, unsigned int offset) {
    try {
      copy(src.begin() + offset, src.begin() + offset + STATE_HASH_SIZE,
           m_stateRootHash.asArray().begin());
      offset += STATE_HASH_SIZE;
    } catch (const std::exception& e) {
      LOG_GENERAL(WARNING, "Error with FallbackBlockHashSet::Deserialize."
                               << ' ' << e.what());
      return -1;
    }

    return 0;
  }

  bool operator==(const FallbackBlockHashSet& hashSet) const {
    return std::tie(m_stateRootHash) == std::tie(hashSet.m_stateRootHash);
  }
  bool operator<(const FallbackBlockHashSet& hashSet) const {
    return std::tie(hashSet.m_stateRootHash) > std::tie(m_stateRootHash);
  }
  bool operator>(const FallbackBlockHashSet& hashSet) const {
    return hashSet < *this;
  }

  static constexpr unsigned int size() { return STATE_HASH_SIZE; }

  friend std::ostream& operator<<(std::ostream& os,
                                  const FallbackBlockHashSet& t);
};

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<FallbackBlockHashSet> {
  size_t operator()(FallbackBlockHashSet const& hashSet) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, hashSet.m_stateRootHash.hex());

    return seed;
  }
};
}  // namespace std

inline std::ostream& operator<<(std::ostream& os,
                                const FallbackBlockHashSet& t) {
  os << "m_stateRootHash : " << t.m_stateRootHash.hex() << std::endl;
  return os;
}

#endif  // __BLOCKHASHSET_H__
