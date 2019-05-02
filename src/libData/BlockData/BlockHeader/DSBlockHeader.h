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

#ifndef __DSBLOCKHEADER_H__
#define __DSBLOCKHEADER_H__

#include <array>
#include <map>

#include "BlockHashSet.h"
#include "BlockHeaderBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libNetwork/Peer.h"
#include "libUtils/SWInfo.h"

/// Stores information on the header part of the DS block.
class DSBlockHeader : public BlockHeaderBase {
  uint8_t m_dsDifficulty;  // Number of PoW leading zeros
  uint8_t m_difficulty;    // Number of PoW leading zeros
  PubKey m_leaderPubKey;   // The one who proposed this DS block
  uint64_t m_blockNum;     // Block index, starting from 0 in the genesis block
  uint64_t m_epochNum;     // Tx Epoch Num when the DS block was generated
  uint128_t m_gasPrice;
  SWInfo m_swInfo;
  std::map<PubKey, Peer> m_PoWDSWinners;
  DSBlockHashSet m_hashset;

 public:
  /// Default constructor.
  DSBlockHeader();  // creates a dummy invalid placeholder BlockHeader

  /// Constructor for loading DS block header information from a byte stream.
  DSBlockHeader(const bytes& src, unsigned int offset);

  /// Constructor with specified DS block header parameters.
  DSBlockHeader(const uint8_t dsDifficulty, const uint8_t difficulty,
                const PubKey& leaderPubKey, const uint64_t& blockNum,
                const uint64_t& epochNum, const uint128_t& gasPrice,
                const SWInfo& swInfo,
                const std::map<PubKey, Peer>& powDSWinners,
                const DSBlockHashSet& hashset, const uint32_t version = 0,
                const CommitteeHash& committeeHash = CommitteeHash(),
                const BlockHash& prevHash = BlockHash());

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(bytes& dst, unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const bytes& src, unsigned int offset) override;

  /// Implements the GetHash function for serializing based on concrete vars
  /// only, primarily used for generating randomness seed
  BlockHash GetHashForRandom() const;

  /// Returns the difficulty of the PoW puzzle.
  const uint8_t& GetDSDifficulty() const;

  /// Returns the difficulty of the PoW puzzle.
  const uint8_t& GetDifficulty() const;

  /// Returns the public key of the leader of the DS committee that composed
  /// this block.
  const PubKey& GetLeaderPubKey() const;

  /// Returns the number of ancestor blocks.
  const uint64_t& GetBlockNum() const;

  /// Returns the number of tx epoch when block is mined
  const uint64_t& GetEpochNum() const;

  /// Returns the number of global minimum gas price accepteable for the coming
  /// epoch
  const uint128_t& GetGasPrice() const;

  /// Returns the software version information used during creation of this
  /// block.
  const SWInfo& GetSWInfo() const;

  const std::map<PubKey, Peer>& GetDSPoWWinners() const;

  /// Returns the digest that represents the hash of the sharding structure
  const ShardingHash& GetShardingHash() const;

  /// Returns a reference to the reserved field in the hash set
  const std::array<unsigned char, RESERVED_FIELD_SIZE>&
  GetHashSetReservedField() const;

  /// Equality operator.
  bool operator==(const DSBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const DSBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const DSBlockHeader& header) const;

  friend std::ostream& operator<<(std::ostream& os, const DSBlockHeader& t);
};

inline std::ostream& operator<<(std::ostream& os, const DSBlockHeader& t) {
  const BlockHeaderBase& blockHeaderBase(t);

  os << blockHeaderBase << std::endl
     << "<DSBlockHeader>" << std::endl
     << " m_dsDifficulty = " << t.m_dsDifficulty << std::endl
     << " m_difficulty   = " << t.m_difficulty << std::endl
     << " m_leaderPubKey = " << t.m_leaderPubKey << std::endl
     << " m_blockNum     = " << t.m_blockNum << std::endl
     << " m_epochNum     = " << t.m_epochNum << std::endl
     << " m_gasPrice     = " << t.m_gasPrice << std::endl
     << t.m_hashset << std::endl
     << t.m_swInfo << std::endl;
  for (const auto& node : t.m_PoWDSWinners) {
    os << " PoWDSWinner    = " << node.first << " " << node.second << std::endl;
  }

  return os;
}

#endif  // __DSBLOCKHEADER_H__
