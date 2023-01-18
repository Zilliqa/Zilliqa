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

#ifndef ZILLIQA_SRC_LIBBLOCKCHAIN_DSBLOCKHEADER_H_
#define ZILLIQA_SRC_LIBBLOCKCHAIN_DSBLOCKHEADER_H_

#include <Schnorr.h>
#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "libNetwork/Peer.h"
#include "libUtils/SWInfo.h"

/// Stores information on the header part of the DS block.
class DSBlockHeader final : public BlockHeaderBase {
  uint8_t m_dsDifficulty{};  // Number of PoW leading zeros
  uint8_t m_difficulty{};    // Number of PoW leading zeros
  PubKey m_leaderPubKey;     // The one who proposed this DS block
  uint64_t m_blockNum{};  // Block index, starting from 0 in the genesis block
  uint64_t m_epochNum{};  // Tx Epoch Num when the DS block was generated
  uint128_t m_gasPrice;
  SWInfo m_swInfo;
  std::map<PubKey, Peer> m_PoWDSWinners;
  std::vector<PubKey> m_removeDSNodePubkeys;
  DSBlockHashSet m_hashset;
  // map of proposal id , ds votes map, shard votes map
  GovDSShardVotesMap m_govProposalMap;

 public:
  /// Default constructor.
  DSBlockHeader(uint8_t dsDifficulty = 0, uint8_t difficulty = 0,
                const PubKey& leaderPubKey = {},
                uint64_t blockNum = INIT_BLOCK_NUMBER,
                uint64_t epochNum = (uint64_t)-1, const uint128_t& gasPrice = 0,
                const SWInfo& swInfo = {},
                const std::map<PubKey, Peer>& powDSWinners = {},
                const std::vector<PubKey>& removeDSNodePubkeys = {},
                const DSBlockHashSet& hashset = {},
                const GovDSShardVotesMap& m_govProposalMap = {},
                uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(zbytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const zbytes& src, unsigned int offset) override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::string& src, unsigned int offset) override;

  /// Implements the GetHash function for serializing based on concrete vars
  /// only, primarily used for generating randomness seed
  BlockHash GetHashForRandom() const;

  /// Returns the difficulty of the PoW puzzle.
  uint8_t GetDSDifficulty() const noexcept { return m_dsDifficulty; }

  /// Returns the difficulty of the PoW puzzle.
  uint8_t GetDifficulty() const noexcept { return m_difficulty; }

  /// Returns the total difficulty of the chain until this block. (not
  /// supported)
  uint8_t GetTotalDifficulty() const noexcept { return 0; }

  /// Returns the public key of the leader of the DS committee that composed
  /// this block.
  const PubKey& GetLeaderPubKey() const noexcept { return m_leaderPubKey; }

  /// Returns the number of ancestor blocks.
  uint64_t GetBlockNum() const noexcept { return m_blockNum; }

  /// Returns the number of tx epoch when block is mined
  uint64_t GetEpochNum() const noexcept { return m_epochNum; }

  /// Returns the number of global minimum gas price acceptable for the coming
  /// epoch
  const uint128_t& GetGasPrice() const noexcept { return m_gasPrice; }

  /// Returns the software version information used during creation of this
  /// block.
  const SWInfo& GetSWInfo() const noexcept { return m_swInfo; }

  // Returns the DS PoW Winners.
  const std::map<PubKey, Peer>& GetDSPoWWinners() const noexcept {
    return m_PoWDSWinners;
  }

  // Returns Governance proposals and corresponding votes values count.
  const GovDSShardVotesMap& GetGovProposalMap() const noexcept {
    return m_govProposalMap;
  }

  // Returns the DS members to remove for non-performance.
  const std::vector<PubKey>& GetDSRemovePubKeys() const noexcept {
    return m_removeDSNodePubkeys;
  }

  /// Returns the digest that represents the hash of the sharding structure
  const ShardingHash& GetShardingHash() const noexcept {
    return m_hashset.m_shardingHash;
  }

  /// Returns a reference to the reserved field in the hash set
  const std::array<unsigned char, RESERVED_FIELD_SIZE>&
  GetHashSetReservedField() const noexcept {
    return m_hashset.m_reservedField;
  }

  /// Equality operator.
  bool operator==(const DSBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os, const DSBlockHeader& t);
};

std::ostream& operator<<(std::ostream& os, const DSBlockHeader& t);

#endif  // ZILLIQA_SRC_LIBBLOCKCHAIN_DSBLOCKHEADER_H_
