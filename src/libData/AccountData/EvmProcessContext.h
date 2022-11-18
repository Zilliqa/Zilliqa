/*
 * Copyright (C) 2022 Zilliqa
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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSCONTEXT_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSCONTEXT_H_

#include <memory>

class Transaction;
class TxnExtras;

/* EvmProcessContext
 * *
 * This structure is the holding structure for data about
 * to be passed to the evm-ds processing engine.
 *
 * Balances within Zilliqa Blockchain are  :
 * measured in the smallest accounting unit Qa (or 10^-12 Zil).
 *
 * This Context is targeted at an ETH Evm based engine, therefore
 * storage for this context is in gwei (Ethereum units).
 * Gwei is a denomination of the cryptocurrency ether (ETH),
 * used on the Ethereum network to buy and sell goods and services.
 * · A gwei is one-billionth of one ETH.
 *
 * Incoming Zil/Qa will be converted to Eth/Gwei using the following methodology
 *
 * At the time of writing, MIN_ETH_GAS = 21000, NORMAL_TRAN_GAS = 50;
 * SCALING_FACTOR = MIN_ETH_GAS / NORMAL_TRAN_GAS;
 * Therefore this module uses a scaling factor of 21000/50 or 420
 *
 * */

#include "common/TxnStatus.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libUtils/Evm.pb.h"
#include "libUtils/EvmUtils.h"

struct EvmProcessContext {
  const uint64_t& GetBlockNumber();

 public:
  /*
   *   EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
   *                       const TxnExtras& extras, bool commit = true)
   *   This is the traditional form of the constructor as used by the existing
   *   Zilliqa platform pre-evm for the 8.3 and beyond series.
   *
   */
  EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
                    const TxnExtras& extras, bool commit = true);
  /*
   *   EvmProcessContext(const uint64_t& blkNum, const Transaction& txn,
   *                       const TxnExtras& extras, bool commit = true)
   *   This is the DirectCall format as used by 8.3 and beyond series.
   *
   */
  EvmProcessContext(const Address& caller, const Address& contract,
                    const zbytes& code, const zbytes& data, const uint64_t& gas,
                    const uint256_t& amount, const uint64_t& blkNum,
                    const TxnExtras& extras, bool estimate = false);

  bool GetCommit() const;

  /*
   * SetCode(const zbytes& code)
   *
   * In the case of a contract_call or non_contract then the contract already
   * exists in the account and the official version from the storage will
   * always be used regardless of what the use has passed to us.
   */

  void SetCode(const zbytes& code);

  /*
   * const zbyte& GetCode()
   *
   * get a const ref to the binary code that represents the EVM contract
   */

  const zbytes& GetCode() const;

  /*
   * const zbyte& GetData()
   *
   * get a const ref to the binary data that usual represents the parameters to
   * the EVM contract
   */

  const zbytes& GetData() const;

  /*  SetContractAddress()
   *
   *  Used within a create contract and must be set by the user when they
   *  actually create a new contract.
   *
   */

  void SetContractAddress(const Address& addr);

  /* GetTranID()
   *
   * GetTransactionId() supplied by transaction
   * Probably useful for debugging
   * */

  dev::h256 GetTranID() const;

  /* GetStatus()
   * returns true when all is good, otherwise Journal
   * contains the log of operations performed.
   * */

  const bool& GetStatus() const;


  /*
   * GetEvmArgs()
   *
   * Get the arguments in the format ready for passing to evm
   * must have called generateArgs()
   *
   */

  inline const evm::EvmArgs& GetEvmArgs() { return m_protoData; }

  /*
   * Return internal structure populated by call to evm
   */

  const evm::EvmResult& GetEvmResult() const;

  /*
   * Return internal structure populated by call to evm
   */

  void SetEvmResult(const evm::EvmResult& result);

  /*
   * GetEstimateOnly()
   */

  bool GetEstimateOnly() const;

  /*
   * SetEvmReceipt(const TransactionReceipt& tr)
   */

  void SetEvmReceipt(const TransactionReceipt& tr);

  /*
   * GetEvmReceipt()
   */

  const TransactionReceipt& GetEvmReceipt() const;

  bool GetDirect() { return m_direct; }

  inline Transaction::ContractType GetContractType() {
    return Transaction::GetTransactionType(m_legacyTxn);
  }

  inline const Transaction& GetTransaction() const { return m_legacyTxn; }

 private:
  const zbytes& m_txnCode;
  const zbytes& m_txnData;
  const Transaction& m_legacyTxn;
  const Transaction m_dummyTransaction{};

  evm::EvmArgs m_protoData;

  bool m_direct{false};
  bool m_commit{false};
  bool m_status{true};

  evm::EvmResult m_evmResult;
  TransactionReceipt m_evmRcpt;

  const uint64_t& m_blockNumber;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_EVMPROCESSCONTEXT_H_
