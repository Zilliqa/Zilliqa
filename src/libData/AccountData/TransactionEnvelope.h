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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H_

#include <memory>
#include <future>
#include "libUtils/EvmCallParameters.h"
#include "libUtils/EvmJsonResponse.h"

// A holder for the transaction that can be safely queued and dispatched for
// later processing

class TransactionEnvelope {
 public:
  enum TxType{
    NORMAL=0,
    FAST=1
  };

  TransactionEnvelope(const Transaction& t, const TxnExtras& tex)
      : m_txType(NORMAL),m_txn(std::move(t)), m_extras(tex){
  }

  TransactionEnvelope(const EvmCallParameters& p)
      : m_txType(FAST), m_params(std::move(p)){
      m_callFuture=m_callPromise.get_future();
  }

  TransactionEnvelope()
      : m_txType(NORMAL){
  }


  const Transaction& GetTransaction() {
    return const_cast<const Transaction&>(m_txn);
  }

  void CopyTransaction(const Transaction& orig) {
    m_txn = const_cast<Transaction&>(orig);
  }

  void SetExtras(const TxnExtras& extras) { m_extras = extras; }

  const TxnExtras& GetExtras() {
    return const_cast<const TxnExtras&>(m_extras);
  }

  TransactionReceipt& GetReceipt() { return m_receipt; }

  evmproj::CallResponse GetResponse(){
    return m_callFuture.get();
  }

  const EvmCallParameters& GetParameters(){
    return m_params;
  }

  void SetResponse(const evmproj::CallResponse& result){
    m_callPromise.set_value(result);
  }

  const TxType& GetContentType(){
    return m_txType;
  }

 private:
  unsigned int m_version{1};
  TxType       m_txType{NORMAL};
  Transaction m_txn{};
  TxnExtras m_extras{};
  TransactionReceipt m_receipt{};

  EvmCallParameters  m_params;
  std::future<evmproj::CallResponse>    m_callFuture;
  std::promise<evmproj::CallResponse>   m_callPromise;
};

using TransactionEnvelopePtr=std::shared_ptr<TransactionEnvelope>;

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H_
