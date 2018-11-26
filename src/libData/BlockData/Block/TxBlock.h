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

#ifndef __TXBLOCK_H__
#define __TXBLOCK_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
#include <vector>

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libData/BlockData/BlockHeader/TxBlockHeader.h"
#include "libNetwork/Peer.h"

struct MicroBlockInfo {
  BlockHash m_microBlockHash;
  TxnHash m_txnRootHash;
  uint32_t m_shardId;

  bool operator==(const MicroBlockInfo& mbInfo) const {
    return std::tie(m_microBlockHash, m_txnRootHash, m_shardId) ==
           std::tie(mbInfo.m_microBlockHash, mbInfo.m_txnRootHash,
                    mbInfo.m_shardId);
  }
  bool operator<(const MicroBlockInfo& mbInfo) const {
    return std::tie(mbInfo.m_microBlockHash, mbInfo.m_txnRootHash,
                    mbInfo.m_shardId) >
           std::tie(m_microBlockHash, m_txnRootHash, m_shardId);
  }
  bool operator>(const MicroBlockInfo& mbInfo) const { return mbInfo < *this; }
};

/// Stores the Tx block header and signature.

class TxBlock : public BlockBase {
  TxBlockHeader m_header;
  std::vector<MicroBlockInfo> m_mbInfos;

 public:
  /// Default constructor.
  TxBlock();  // creates a dummy invalid placeholder block -- blocknum is
              // maxsize of uint256

  /// Constructor for loading Tx block information from a byte stream.
  TxBlock(const std::vector<unsigned char>& src, unsigned int offset);

  /// Constructor with specified Tx block parameters.
  TxBlock(const TxBlockHeader& header,
          const std::vector<MicroBlockInfo>& mbInfos, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  bool Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

  /// Returns the reference to the TxBlockHeader part of the Tx block.
  const TxBlockHeader& GetHeader() const;

  /// Returns the vector of MicroBlockInfo.
  const std::vector<MicroBlockInfo>& GetMicroBlockInfos() const;

  /// Equality comparison operator.
  bool operator==(const TxBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const TxBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const TxBlock& block) const;
};

#endif  // __TXBLOCK_H__
