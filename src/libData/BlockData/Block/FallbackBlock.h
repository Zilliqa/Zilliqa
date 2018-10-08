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

#ifndef __FALLBACKBLOCK_H__
#define __FALLBACKBLOCK_H__

#include <boost/multiprecision/cpp_int.hpp>

#include "BlockBase.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/BlockData/BlockHeader/BlockHashSet.h"
#include "libData/BlockData/BlockHeader/FallbackBlockHeader.h"
#include "libNetwork/Peer.h"

/// Stores the fallback block header and signature

class FallbackBlock : public BlockBase {
  FallbackBlockHeader m_header;

 public:
  /// Default constructor.
  FallbackBlock();  // creates a dummy invalid placeholder block

  /// Constructor for loading finalblock block information from a byte stream.
  FallbackBlock(const std::vector<unsigned char>& src, unsigned int offset);

  /// Constructor with specified fallback block parameters.
  FallbackBlock(FallbackBlockHeader&& header, CoSignatures&& cosigs);

  /// Implements the Serialize function inherited from Serializable.
  unsigned int Serialize(std::vector<unsigned char>& dst,
                         unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  int Deserialize(const std::vector<unsigned char>& src, unsigned int offset);

  /// Returns the size in bytes when serializing the block.
  unsigned int GetSerializedSize() const;

  /// Returns the minimum required size in bytes for obtaining a fallback block
  /// from a byte stream.
  static unsigned int GetMinSize();

  /// Returns the reference to the FallbackBlockHeader part of the fallback
  /// block.
  const FallbackBlockHeader& GetHeader() const;

  /// Equality comparison operator.
  bool operator==(const FallbackBlock& block) const;

  /// Less-than comparison operator.
  bool operator<(const FallbackBlock& block) const;

  /// Greater-than comparison operator.
  bool operator>(const FallbackBlock& block) const;
};

#endif  // __FALLBACKBLOCK_H__