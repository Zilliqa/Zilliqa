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

#ifndef __BLOCKHEADERBASE_H__
#define __BLOCKHEADERBASE_H__

#include <array>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "common/Constants.h"
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libData/AccountData/Transaction.h"

// Hash for the committee that generated the block
using CommitteeHash = dev::h256;

const uint64_t INIT_BLOCK_NUMBER = (uint64_t)-1;

/// [TODO] Base class for all supported block header types
class BlockHeaderBase : public SerializableDataBlock {
 protected:
  // TODO: pull out all common code from ds, micro and tx block header
  CommitteeHash m_committeeHash;

 public:
  // Constructors
  BlockHeaderBase();
  BlockHeaderBase(const CommitteeHash& committeeHash);

  /// Calculate my hash
  BlockHash GetMyHash() const;

  const CommitteeHash& GetCommitteeHash() const;
};

#endif  // __BLOCKHEADERBASE_H__
