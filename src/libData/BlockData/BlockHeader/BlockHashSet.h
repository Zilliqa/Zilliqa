/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#ifndef __BLOCKHASHSET_H__
#define __BLOCKHASHSET_H__

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"

// Hashes for DSBlockHashSet
using ShardingHash = dev::h256;
using TxSharingHash = dev::h256;

struct DSBlockHashSet {
  ShardingHash m_shardingHash;    // Hash of sharding structure
  TxSharingHash m_txSharingHash;  // Hash of transaction sharing assignments
  std::array<unsigned char, RESERVED_FIELD_SIZE>
      m_reservedField;  // Reserved storage for extra hashes

  bool operator==(const DSBlockHashSet& hashSet) const {
    return std::tie(m_shardingHash, m_txSharingHash) ==
           std::tie(hashSet.m_shardingHash, hashSet.m_txSharingHash);
  }
  bool operator<(const DSBlockHashSet& hashSet) const {
    return std::tie(hashSet.m_shardingHash, hashSet.m_txSharingHash) >
           std::tie(m_shardingHash, m_txSharingHash);
  }
  bool operator>(const DSBlockHashSet& hashSet) const {
    return hashSet < *this;
  }

  friend std::ostream& operator<<(std::ostream& os, const DSBlockHashSet& t);
};

inline std::ostream& operator<<(std::ostream& os, const DSBlockHashSet& t) {
  os << "<DSBlockHashSet>" << std::endl
     << "m_shardingHash : " << t.m_shardingHash.hex() << std::endl
     << "m_txSharingHash : " << t.m_txSharingHash.hex() << std::endl
     << "m_reservedField : "
     << DataConversion::charArrToHexStr(t.m_reservedField);
  return os;
}

// define its hash function in order to used as key in map
namespace std {
template <>
struct hash<DSBlockHashSet> {
  size_t operator()(DSBlockHashSet const& hashSet) const noexcept {
    std::size_t seed = 0;
    boost::hash_combine(seed, hashSet.m_shardingHash.hex());
    boost::hash_combine(seed, hashSet.m_txSharingHash.hex());
    boost::hash_combine(
        seed, DataConversion::charArrToHexStr(hashSet.m_reservedField));

    return seed;
  }
};
}  // namespace std

struct MicroBlockHashSet {
  TxnHash m_txRootHash;        // Tx merkle tree root hash
  StateHash m_stateDeltaHash;  // State Delta hash
  TxnHash m_tranReceiptHash;   // Tx Receipt hash

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(std::vector<unsigned char>& dst,
                         unsigned int offset) const {
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
  int Deserialize(const std::vector<unsigned char>& src, unsigned int offset) {
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
     << "m_txRootHash : " << t.m_txRootHash.hex() << std::endl
     << "m_stateDeltaHash : " << t.m_stateDeltaHash.hex() << std::endl
     << "m_tranReceiptHash : " << t.m_tranReceiptHash.hex();
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
  unsigned int Serialize(std::vector<unsigned char>& dst,
                         unsigned int offset) const {
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
  int Deserialize(const std::vector<unsigned char>& src, unsigned int offset) {
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
  os << "<TxBlockHashSet> " << std::endl
     << "m_stateRootHash : " << t.m_stateRootHash.hex() << std::endl
     << "m_stateDeltaHash : " << t.m_stateDeltaHash.hex() << std::endl
     << "m_mbInfoHash : " << t.m_mbInfoHash.hex();
  return os;
}

struct FallbackBlockHashSet {
  StateHash m_stateRootHash;

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(std::vector<unsigned char>& dst,
                         unsigned int offset) const {
    copy(m_stateRootHash.asArray().begin(), m_stateRootHash.asArray().end(),
         dst.begin() + offset);
    offset += STATE_HASH_SIZE;

    return offset;
  }

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const std::vector<unsigned char>& src, unsigned int offset) {
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
