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

#ifndef __VALIDATOR_H__
#define __VALIDATOR_H__

#include <boost/variant.hpp>
#include <string>
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libData/BlockData/Block.h"
#include "libData/BlockData/Block/FallbackBlockWShardingStructure.h"
#include "libNetwork/Peer.h"

class Mediator;

class ValidatorBase {
 public:
  enum TxBlockValidationMsg { VALID = 0, STALEDSINFO, INVALID };
  virtual ~ValidatorBase() {}
  virtual std::string name() const = 0;

  /// Verifies the transaction w.r.t given pubKey and signature
  virtual bool VerifyTransaction(const Transaction& tran) const = 0;

  virtual bool CheckCreatedTransaction(const Transaction& tx,
                                       TransactionReceipt& receipt) const = 0;

  virtual bool CheckCreatedTransactionFromLookup(const Transaction& tx) = 0;

  virtual bool CheckDirBlocks(
      const std::vector<boost::variant<
          DSBlock, VCBlock, FallbackBlockWShardingStructure>>& dirBlocks,
      const DequeOfNode& initDsComm, const uint64_t& index_num,
      DequeOfNode& newDSComm) = 0;
  virtual TxBlockValidationMsg CheckTxBlocks(
      const std::vector<TxBlock>& txblocks, const DequeOfNode& dsComm,
      const BlockLink& latestBlockLink) = 0;
};

class Validator : public ValidatorBase {
 public:
  Validator(Mediator& mediator);
  ~Validator();
  std::string name() const override { return "Validator"; }
  bool VerifyTransaction(const Transaction& tran) const override;

  bool CheckCreatedTransaction(const Transaction& tx,
                               TransactionReceipt& receipt) const override;

  bool CheckCreatedTransactionFromLookup(const Transaction& tx) override;

  template <class Container, class DirectoryBlock>
  bool CheckBlockCosignature(const DirectoryBlock& block,
                             const Container& commKeys);

  bool CheckDirBlocks(
      const std::vector<boost::variant<
          DSBlock, VCBlock, FallbackBlockWShardingStructure>>& dirBlocks,
      const DequeOfNode& initDsComm, const uint64_t& index_num,
      DequeOfNode& newDSComm) override;
  // TxBlocks must be in increasing order or it will fail
  TxBlockValidationMsg CheckTxBlocks(const std::vector<TxBlock>& txBlocks,
                                     const DequeOfNode& dsComm,
                                     const BlockLink& latestBlockLink) override;
  Mediator& m_mediator;
};

#endif  // __VALIDATOR_H__
