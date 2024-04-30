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
#include "TransactionLite.h"
#include "libUtils/Logger.h"

TransactionLite::TransactionLite(const TxnHash& tranID, uint64_t nonce,
                                 uint64_t currentEpoch)
    : m_tranID(tranID), m_nonce(nonce), m_currentEpoch(currentEpoch) {}

std::ostream& operator<<(std::ostream& os, const TransactionLite& txn) {
  os << "<TransactionLite>" << std::endl
     << "Transaction ID: " << txn.m_tranID.hex() << std::endl
     << "Nonce: " << txn.m_nonce << std::endl
     << "Current Epoch: " << txn.m_currentEpoch;
  return os;
}

void TransactionLiteManager::AddTransaction(
    const Address& address, const TransactionLite&& transaction) {
  LOG_MARKER();  // TODO: Remove all LOG_MARKER and LOG_GENERAL
  std::lock_guard<std::mutex> g(m_currDSEpochTxnLiteMemPoolMutex);
  m_txnLiteMemPool[address].push_back(std::move(transaction));
}

void TransactionLiteManager::RemoveTransaction(const Address& address,
                                               const TxnHash& txnhash) {
  LOG_MARKER();  // TODO: Remove all LOG_MARKER and LOG_GENERAL
  std::lock_guard<std::mutex> g(m_currDSEpochTxnLiteMemPoolMutex);
  auto it = m_txnLiteMemPool.find(address);
  if (it != m_txnLiteMemPool.end()) {
    auto& txns = it->second;
    txns.erase(std::remove_if(txns.begin(), txns.end(),
                              [&txnhash](const TransactionLite& txn) {
                                return txn.GetTransactionID() == txnhash;
                              }),
               txns.end());
    // Remove the key if there exists no transactions
    if (txns.empty()) {
      m_txnLiteMemPool.erase(it);
    }
  }
}

// TODO : Remove this function which was kept for debugging purpose
void TransactionLiteManager::PrintAllTransactions() {
  LOG_MARKER();  // TODO: Remove all LOG_MARKER and LOG_GENERAL
  LOG_GENERAL(INFO, "m_txnLiteMemPool size = " << m_txnLiteMemPool.size());
  std::lock_guard<std::mutex> g(m_currDSEpochTxnLiteMemPoolMutex);
  for (const auto& pair : m_txnLiteMemPool) {
    LOG_GENERAL(INFO, "Address: " << pair.first.hex())
    for (const auto& txn : pair.second) {
      LOG_GENERAL(INFO, txn);
    }
  }
}

void TransactionLiteManager::ClearTransactionLitePool() {
  LOG_MARKER();  // TODO: Remove all LOG_MARKER and LOG_GENERAL
  std::lock_guard<std::mutex> g(m_currDSEpochTxnLiteMemPoolMutex);
  m_txnLiteMemPool.clear();
}

uint64_t TransactionLiteManager::GetHighestNonceForAddress(
    const Address& address, const uint64_t& currentTxEpoch) {
  LOG_MARKER();  // TODO: Remove all LOG_MARKER and LOG_GENERAL
  std::lock_guard<std::mutex> lock(m_currDSEpochTxnLiteMemPoolMutex);
  uint64_t maxNonce = 0;
  LOG_GENERAL(INFO, "GetHighestNonceForAddress address = "
                        << address << " current txEPoch = " << currentTxEpoch
                        << " MAX_EPOCH_DIFFERENCE_FOR_ETH_TXN_COUNT = "
                        << MAX_EPOCH_DIFFERENCE_FOR_ETH_TXN_COUNT);
  const auto& transactions = m_txnLiteMemPool[address];
  const auto& it = m_txnLiteMemPool.find(address);
  if (it != m_txnLiteMemPool.end()) {
    const auto& transactions = it->second;
    for (const auto& txn : transactions) {
      LOG_GENERAL(INFO, txn);
      uint64_t epochDiff = currentTxEpoch - txn.GetCurrentEpoch();
      LOG_GENERAL(INFO, "GetHighestNonceForAddress  txn.m_nonce = "
                            << txn.GetNonce() << " maxNonce =" << maxNonce
                            << " txepoch = " << txn.GetCurrentEpoch()
                            << " epochDiff = " << epochDiff);

      if (txn.GetNonce() > maxNonce &&
          epochDiff < MAX_EPOCH_DIFFERENCE_FOR_ETH_TXN_COUNT) {
        maxNonce = txn.GetNonce();
      }
    }
  }
  return maxNonce;
}
