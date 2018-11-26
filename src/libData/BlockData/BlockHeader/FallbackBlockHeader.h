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

#ifndef __FALLBACKBLOCKHEADER_H__
#define __FALLBACKBLOCKHEADER_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libNetwork/PeerStore.h"

/// Stores information on the header part of the fallback block.
class FallbackBlockHeader : public BlockHeaderBase {
  uint64_t m_fallbackDSEpochNo;
  uint64_t m_fallbackEpochNo;
  unsigned char m_fallbackState;
  FallbackBlockHashSet m_hashset;
  uint32_t m_leaderConsensusId;
  Peer m_leaderNetworkInfo;
  PubKey m_leaderPubKey;
  uint32_t m_shardId;
  BlockHash m_prevHash;

 public:
  /// Default constructor.
  FallbackBlockHeader();  // creates a dummy invalid placeholder BlockHeader

  /// Constructor for loading fallback block header information from a byte
  /// stream.
  FallbackBlockHeader(const std::vector<unsigned char>& src,
                      unsigned int offset);

  /// Constructor with specified fallback block header parameters.
  FallbackBlockHeader(
      const uint64_t& fallbackDSEpochNo, const uint64_t& fallbackEpochNo,
      const unsigned char fallbackState, const FallbackBlockHashSet& hashset,
      const uint32_t leaderConsensusId, const Peer& leaderNetworkInfo,
      const PubKey& leaderPubKey, const uint32_t shardId,
      const CommitteeHash& committeeHash, const BlockHash& prevHash);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

  /// Returns the hash of prev dir block
  const BlockHash& GetPrevHash() const { return m_prevHash; }

  /// Returns the DS Epoch number where view change happen
  const uint64_t& GetFallbackDSEpochNo() const;

  /// Returns the Epoch number where view change happen
  const uint64_t& GetFallbackEpochNo() const;

  /// Return the candidate leader ds state when view change happen
  unsigned char GetFallbackState() const;

  /// Returns the digest that represents the root of the Merkle tree that stores
  /// all state uptil this block.
  const StateHash& GetStateRootHash() const;

  /// Return the consensus Id of the leader
  uint32_t GetLeaderConsensusId() const;

  /// Return the IP and port of leader (at the point where fall back happen)
  const Peer& GetLeaderNetworkInfo() const;

  /// Return pub key of leader
  const PubKey& GetLeaderPubKey() const;

  /// Return the shard id where fallback happens
  uint32_t GetShardId() const;

  /// Equality operator.
  bool operator==(const FallbackBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const FallbackBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const FallbackBlockHeader& header) const;
};

#endif  // __FALLBACKBLOCKHEADER_H__
