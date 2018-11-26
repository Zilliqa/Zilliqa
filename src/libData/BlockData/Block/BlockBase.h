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

#ifndef __BLOCKBASE_H__
#define __BLOCKBASE_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libConsensus/ConsensusCommon.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/BlockHeader/BlockHeaderBase.h"

struct CoSignatures {
  Signature m_CS1;
  std::vector<bool> m_B1;
  Signature m_CS2;
  std::vector<bool> m_B2;

  CoSignatures(unsigned int bitmaplen = 1) : m_B1(bitmaplen), m_B2(bitmaplen) {}
  CoSignatures(const CoSignatures& src) = default;
  CoSignatures(const Signature& CS1, const std::vector<bool>& B1,
               const Signature& CS2, const std::vector<bool>& B2)
      : m_CS1(CS1), m_B1(B1), m_CS2(CS2), m_B2(B2) {}
};

/// [TODO] Base class for all supported block data types
class BlockBase : public SerializableDataBlock {
  // TODO: pull out all common code from ds, micro and tx block
 protected:
  BlockHash m_blockHash;
  CoSignatures m_cosigs;
  uint64_t m_timestamp;

 public:
  /// Default constructor.
  BlockBase();

  /// Returns the block hash
  const BlockHash& GetBlockHash() const;

  /// Returns the timestamp
  const uint64_t& GetTimestamp() const;

  /// Set the timestamp
  void SetTimestamp(const uint64_t& timestamp);

  /// Set the block hash
  void SetBlockHash(const BlockHash& blockHash);

  /// Returns the co-sig for first round.
  const Signature& GetCS1() const;

  /// Returns the co-sig bitmap for first round.
  const std::vector<bool>& GetB1() const;

  /// Returns the co-sig for second round.
  const Signature& GetCS2() const;

  /// Returns the co-sig bitmap for second round.
  const std::vector<bool>& GetB2() const;

  /// Sets the co-sig members.
  void SetCoSignatures(const ConsensusCommon& src);
  void SetCoSignatures(CoSignatures& cosigs);
};

#endif  // __BLOCKBASE_H__
