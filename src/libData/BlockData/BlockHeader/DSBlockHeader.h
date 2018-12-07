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

#ifndef __DSBLOCKHEADER_H__
#define __DSBLOCKHEADER_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
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
  BlockHash m_prevHash;    // Hash of the previous block
  PubKey m_leaderPubKey;   // The one who proposed this DS block
  uint64_t m_blockNum;     // Block index, starting from 0 in the genesis block
  uint64_t m_epochNum;     // Tx Epoch Num when the DS block was generated
  boost::multiprecision::uint128_t m_gasPrice;
  SWInfo m_swInfo;
  std::map<PubKey, Peer> m_PoWDSWinners;
  DSBlockHashSet m_hashset;

 public:
  /// Default constructor.
  DSBlockHeader();  // creates a dummy invalid placeholder BlockHeader

  /// Constructor for loading DS block header information from a byte stream.
  DSBlockHeader(const std::vector<unsigned char>& src, unsigned int offset);

  /// Constructor with specified DS block header parameters.
  DSBlockHeader(const uint8_t dsDifficulty, const uint8_t difficulty,
                const BlockHash& prevHash, const PubKey& leaderPubKey,
                const uint64_t& blockNum, const uint64_t& epochNum,
                const boost::multiprecision::uint128_t& gasPrice,
                const SWInfo& swInfo,
                const std::map<PubKey, Peer>& powDSWinners,
                const DSBlockHashSet& hashset,
                const CommitteeHash& committeeHash);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst,
                 unsigned int offset) const override;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::vector<unsigned char>& src,
                   unsigned int offset) override;

  /// Implements the GetHash function for serializing based on concrete vars
  /// only, primarily used for generating randomness seed
  BlockHash GetHashForRandom() const;

  /// Returns the difficulty of the PoW puzzle.
  const uint8_t& GetDSDifficulty() const;

  /// Returns the difficulty of the PoW puzzle.
  const uint8_t& GetDifficulty() const;

  /// Returns the hash of prev dir block
  const BlockHash& GetPrevHash() const;

  /// Returns the public key of the leader of the DS committee that composed
  /// this block.
  const PubKey& GetLeaderPubKey() const;

  /// Returns the number of ancestor blocks.
  const uint64_t& GetBlockNum() const;

  /// Returns the number of tx epoch when block is mined
  const uint64_t& GetEpochNum() const;

  /// Returns the number of global minimum gas price accepteable for the coming
  /// epoch
  const boost::multiprecision::uint128_t& GetGasPrice() const;

  /// Returns the software version information used during creation of this
  /// block.
  const SWInfo& GetSWInfo() const;

  const std::map<PubKey, Peer>& GetDSPoWWinners() const;

  /// Returns the digest that represents the hash of the sharding structure
  const ShardingHash& GetShardingHash() const;

  /// Returns the digest that represents the hash of the transaction sharing
  /// assignments
  const TxSharingHash& GetTxSharingHash() const;

  /// Returns a reference to the reserved field in the hash set
  const std::array<unsigned char, RESERVED_FIELD_SIZE>&
  GetHashSetReservedField() const;

  /// Equality operator.
  bool operator==(const DSBlockHeader& header) const;

  /// Less-than comparison operator.
  bool operator<(const DSBlockHeader& header) const;

  /// Greater-than comparison operator.
  bool operator>(const DSBlockHeader& header) const;
};

#endif  // __DSBLOCKHEADER_H__
