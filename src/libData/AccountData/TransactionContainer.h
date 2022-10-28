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

#ifndef ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H
#define ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H

#include <memory>

// A holder for the transaction that can be safely queued and dispatched for
// later processing

/// The new transaction holder.

class TransactionEnvelope {
 public:
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

 private:
  unsigned int m_version{1};
  Transaction m_txn{};
  TxnExtras m_extras{};
  TransactionReceipt m_receipt{};
};

#endif  // ZILLIQA_SRC_LIBDATA_ACCOUNTDATA_TRANSACTIONCONTAINER_H
