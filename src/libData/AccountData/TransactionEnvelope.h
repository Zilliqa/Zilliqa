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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONENVELOPE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONENVELOPE_H_

#include <memory>
#include <future>
#include "libUtils/TxnExtras.h"
#include "libUtils/Evm.pb.h"


// A holder for the transaction that can be safely queued and dispatched for
// later processing

/// The new transaction holder.

class TransactionEnvelope {
 public:
  enum TX_TYPE{
    NORMAL=0,
    NON_TRANSMISSABLE=1,
    TEST=2
  };
  TransactionEnvelope() = delete;
  explicit TransactionEnvelope(const Transaction& tx,const TxnExtras& extras,TransactionReceipt& rc,TX_TYPE tx_type=NORMAL):
    m_txn(std::move(tx)), m_extras(extras), m_receipt(rc),m_txType(tx_type){
    m_callFuture=m_callPromise.get_future();
  }

  const Transaction& GetTransaction() {
    return const_cast<const Transaction&>(m_txn);
  }

  TxnExtras& GetExtras() {
    return m_extras;
  }

  TransactionReceipt& GetReceipt() {
    return m_receipt;
  }

  TX_TYPE GetTxType(){
    return m_txType;
  }

  void SetResponse(const evm::EvmResult& result){
    m_callPromise.set_value(std::move(result));
  }

  evm::EvmResult GetResponse(){
    return m_callFuture.get();
  }

  void SetSource(const std::string& str){
    m_fromAddress = str;
  }

  const std::string& GetSource(){
    return m_fromAddress;
  }


 private:
  unsigned int m_version{1};
  Transaction m_txn;
  TxnExtras m_extras;
  TransactionReceipt& m_receipt;
  TX_TYPE   m_txType;
  std::future<evm::EvmResult>    m_callFuture;
  std::promise<evm::EvmResult>   m_callPromise;
  std::string                           m_fromAddress;
};

using TransactionEnvelopeSp=std::shared_ptr<TransactionEnvelope>;

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONENVELOPE_H_

