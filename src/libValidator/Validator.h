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

#ifndef ZILLIQA_SRC_LIBVALIDATOR_VALIDATOR_H_
#define ZILLIQA_SRC_LIBVALIDATOR_VALIDATOR_H_

#include <boost/variant.hpp>
#include <string>
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libNetwork/Peer.h"

class Mediator;

class Validator {
 public:
  enum TxBlockValidationMsg { VALID = 0, STALEDSINFO, INVALID };
  Validator(Mediator& mediator);
  ~Validator();
  std::string name() const { return "Validator"; }

  static bool VerifyTransaction(const Transaction& tran);

  bool CheckCreatedTransaction(const Transaction& tx,
                               TransactionReceipt& receipt,
                               ErrTxnStatus& error_code) const;

  bool CheckCreatedTransactionFromLookup(const Transaction& tx,
                                         ErrTxnStatus& error_code);

  template <class Container, class DirectoryBlock>
  bool CheckBlockCosignature(const DirectoryBlock& block,
                             const Container& commKeys,
                             const bool showLogs = true);

  bool CheckDirBlocks(
      const std::vector<boost::variant<
          DSBlock, VCBlock, FallbackBlockWShardingStructure>>& dirBlocks,
      const DequeOfNode& initDsComm, const uint64_t& index_num,
      DequeOfNode& newDSComm);

  bool CheckDirBlocksNoUpdate(
      const std::vector<boost::variant<
          DSBlock, VCBlock, FallbackBlockWShardingStructure>>& dirBlocks,
      const DequeOfNode& initDsComm, const uint64_t& index_num,
      DequeOfNode& newDSComm);

  // TxBlocks must be in increasing order or it will fail
  TxBlockValidationMsg CheckTxBlocks(const std::vector<TxBlock>& txBlocks,
                                     const DequeOfNode& dsComm,
                                     const BlockLink& latestBlockLink);
  Mediator& m_mediator;
};

#endif  // ZILLIQA_SRC_LIBVALIDATOR_VALIDATOR_H_
