/*
 * Copyright (C) 2024 Zilliqa
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
#include <Schnorr.h>
#include "common/Constants.h"
#include "libData/AccountData/Address.h"

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONLITE_H_
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONLITE_H_

class TransactionLite {
 private:
  TxnHash m_tranID;
  uint64_t m_nonce{};
  uint64_t m_currentEpoch;

 public:
  // Delete default constructor
  TransactionLite() = delete;
  const TxnHash& GetTransactionID() const { return m_tranID; }
  uint64_t GetNonce() const { return m_nonce; }
  uint64_t GetCurrentEpoch() const { return m_currentEpoch; }

  TransactionLite(const TxnHash& tranID, uint64_t nonce, uint64_t currentEpoch);
  friend std::ostream& operator<<(std::ostream& os, const TransactionLite& txn);
};
class TransactionLiteManager {
 public:
  void AddTransaction(const Address& address,
                      const TransactionLite&& transaction);

  void RemoveTransaction(const Address& address, const TxnHash& txnId);

  // TODO : Remove the print function
  void PrintAllTransactions();

  void ClearTransactionLitePool();

  uint64_t GetHighestNonceForAddress(const Address& address,
                                     const uint64_t& currentTxEpoch);

 private:
  std::mutex m_currDSEpochTxnLiteMemPoolMutex;
  std::map<Address, std::vector<TransactionLite>> m_txnLiteMemPool;
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONLITE_H_
