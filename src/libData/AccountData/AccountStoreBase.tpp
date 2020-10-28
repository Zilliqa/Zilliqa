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

#include <type_traits>

#include "libMessage/MessengerAccountStoreBase.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

template <class MAP>
AccountStoreBase<MAP>::AccountStoreBase() {
  // m_mutexASBase = std::make_shared<std::shared_timed_mutex>();
  m_addressToAccount = std::make_shared<MAP>();
}

template <class MAP>
void AccountStoreBase<MAP>::Init() {
  std::lock_guard<std::mutex> g(m_mutexASBase);
  m_addressToAccount->clear();
}

template <class MAP>
bool AccountStoreBase<MAP>::Serialize(bytes& dst, unsigned int offset) const {
  std::lock_guard<std::mutex> g(m_mutexASBase);
  if (!MessengerAccountStoreBase::SetAccountStore(dst, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStore failed.");
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::Deserialize(const bytes& src, unsigned int offset) {
  std::lock_guard<std::mutex> g(m_mutexASBase);
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::UpdateAccounts(const Transaction& transaction,
                                           TransactionReceipt& receipt,
                                           TxnStatus& error_code) {
  const PubKey& senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  Address toAddr = transaction.GetToAddr();
  const uint128_t& amount = transaction.GetAmount();
  error_code = TxnStatus::NOT_PRESENT;

  uint128_t fromBalance = 0;
  {
    std::shared_ptr<Account> acc;
    std::unique_lock<std::mutex> g(GetAccountWMutex(fromAddr, acc));
    if (acc == nullptr) {
      LOG_GENERAL(WARNING, "sender " << fromAddr.hex() << " not exist");
      error_code = TxnStatus::INVALID_FROM_ACCOUNT;
      return false;
    }
    fromBalance = acc->GetBalance();
  }

  if (transaction.GetGasLimit() < NORMAL_TRAN_GAS) {
    LOG_GENERAL(WARNING,
                "The gas limit "
                    << transaction.GetGasLimit()
                    << " should be larger than the normal transaction gas ("
                    << NORMAL_TRAN_GAS << ")");
    error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
    return false;
  }

  uint128_t gasDeposit = 0;
  if (!SafeMath<uint128_t>::mul(transaction.GetGasLimit(),
                                transaction.GetGasPrice(), gasDeposit)) {
    LOG_GENERAL(
        WARNING,
        "transaction.GetGasLimit() * transaction.GetGasPrice() overflow!");
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  if (fromBalance < transaction.GetAmount() + gasDeposit) {
    LOG_GENERAL(WARNING,
                "The account (balance: "
                    << fromBalance
                    << ") "
                       "doesn't have enough balance to pay for the gas limit ("
                    << gasDeposit
                    << ") "
                       "with amount ("
                    << transaction.GetAmount() << ") in the transaction");
    error_code = TxnStatus::INSUFFICIENT_BALANCE;
    return false;
  }

  if (!DecreaseBalance(fromAddr, gasDeposit)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  if (!TransferBalance(fromAddr, toAddr, amount)) {
    if (!IncreaseBalance(fromAddr, gasDeposit)) {
      LOG_GENERAL(FATAL, "IncreaseBalance failed for gasDeposit");
    }
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  uint128_t gasRefund;
  if (!CalculateGasRefund(gasDeposit, NORMAL_TRAN_GAS,
                          transaction.GetGasPrice(), gasRefund)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  if (!IncreaseBalance(fromAddr, gasRefund)) {
    error_code = TxnStatus::MATH_ERROR;
    LOG_GENERAL(FATAL, "IncreaseBalance failed for gasRefund");
  }

  if (!IncreaseNonce(fromAddr)) {
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  receipt.SetResult(true);
  receipt.SetCumGas(NORMAL_TRAN_GAS);
  receipt.update();

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::CalculateGasRefund(const uint128_t& gasDeposit,
                                               const uint64_t& gasUnit,
                                               const uint128_t& gasPrice,
                                               uint128_t& gasRefund) {
  uint128_t gasFee;
  if (!SafeMath<uint128_t>::mul(gasUnit, gasPrice, gasFee)) {
    LOG_GENERAL(WARNING, "gasUnit * transaction.GetGasPrice() overflow!");
    return false;
  }

  if (!SafeMath<uint128_t>::sub(gasDeposit, gasFee, gasRefund)) {
    LOG_GENERAL(WARNING, "gasDeposit - gasFee overflow!");
    return false;
  }

  // LOG_GENERAL(INFO, "gas price to refund: " << gasRefund);
  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::IsAccountExist(const Address& address, bool base) {
  LOG_MARKER();
  std::shared_ptr<Account> acc;
  if (base) {
    std::unique_lock<std::mutex> g(
        AccountStoreBase<MAP>::GetAccountWMutex(address, acc));
    return acc != nullptr;
  } else {
    std::unique_lock<std::mutex> g(GetAccountWMutex(address, acc));
    return acc != nullptr;
  }
}

template <class MAP>
bool AccountStoreBase<MAP>::AddAccount(const Address& address,
                                       const std::shared_ptr<Account>& account,
                                       bool toReplace) {
  LOG_MARKER();

  if (toReplace || !IsAccountExist(address, true)) {
    LOG_GENERAL(INFO, "added account");
    std::lock_guard<std::mutex> g(m_mutexASBase);
    (*m_addressToAccount)[address] = account;

    return true;
  }
  LOG_GENERAL(WARNING, "Address "
                           << address
                           << " could not be added because already present");
  return false;
}

template <class MAP>
bool AccountStoreBase<MAP>::AddAccount(
    const PubKey& pubKey, const std::shared_ptr<Account>& account) {
  return AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

template <class MAP>
void AccountStoreBase<MAP>::RemoveAccount(const Address& address) {
  if (IsAccountExist(address)) {
    std::lock_guard<std::mutex> g(m_mutexASBase);
    m_addressToAccount->erase(address);
  }
}

template <class MAP>
std::unique_lock<std::mutex> AccountStoreBase<MAP>::GetAccountWMutex(
    const Address& address, std::shared_ptr<Account>& acc) {
  LOG_MARKER();
  acc = nullptr;
  std::unique_lock<std::mutex> g(m_mutexASBase);
  auto it = m_addressToAccount->find(address);
  if (it != m_addressToAccount->end()) {
    LOG_GENERAL(INFO, "found account");
    acc = it->second;
  }
  return g;
}

template <class MAP>
std::unique_lock<std::mutex> AccountStoreBase<MAP>::GetAccounts(
    std::shared_ptr<MAP>& accs) const {
  accs = nullptr;
  std::unique_lock<std::mutex> g(m_mutexASBase);
  accs = m_addressToAccount;
  return g;
}

template <class MAP>
size_t AccountStoreBase<MAP>::GetNumOfAccounts() const {
  // LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexASBase);
  return m_addressToAccount->size();
}

template <class MAP>
bool AccountStoreBase<MAP>::IncreaseBalance(const Address& address,
                                            const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  std::shared_ptr<Account> account;
  std::unique_lock<std::mutex> g(GetAccountWMutex(address, account));

  if (account != nullptr && account->IncreaseBalance(delta)) {
    return true;
  } else if (account == nullptr) {
    g.unlock();
    return AddAccount(address, std::make_shared<Account>(delta, 0));
  }

  return false;
}

template <class MAP>
bool AccountStoreBase<MAP>::DecreaseBalance(const Address& address,
                                            const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  std::shared_ptr<Account> account;
  std::unique_lock<std::mutex> g(GetAccountWMutex(address, account));

  if (nullptr == account) {
    LOG_GENERAL(WARNING, "Account " << address.hex() << " not exist");
    return false;
  }

  if (!account->DecreaseBalance(delta)) {
    LOG_GENERAL(WARNING, "Failed to decrease " << delta << " for account "
                                               << address.hex());
    return false;
  }
  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::TransferBalance(const Address& from,
                                            const Address& to,
                                            const uint128_t& delta) {
  // LOG_MARKER();
  // FIXME: Is there any elegent way to implement this atomic change on balance?
  if (DecreaseBalance(from, delta)) {
    if (IncreaseBalance(to, delta)) {
      return true;
    } else {
      if (!IncreaseBalance(from, delta)) {
        LOG_GENERAL(FATAL, "IncreaseBalance failed for delta");
      }
    }
  }

  return false;
}

template <class MAP>
uint128_t AccountStoreBase<MAP>::GetBalance(const Address& address) {
  // LOG_MARKER();

  std::shared_ptr<Account> account;
  std::unique_lock<std::mutex> g(GetAccountWMutex(address, account));

  if (account != nullptr) {
    return account->GetBalance();
  }

  return 0;
}

template <class MAP>
bool AccountStoreBase<MAP>::IncreaseNonce(const Address& address) {
  // LOG_MARKER();

  std::shared_ptr<Account> account;
  std::unique_lock<std::mutex> g(GetAccountWMutex(address, account));

  // LOG_GENERAL(INFO, "address: " << address << " account: " << *account);

  if (nullptr == account) {
    LOG_GENERAL(WARNING, "Increase nonce failed");

    return false;
  }

  if (account->IncreaseNonce()) {
    // LOG_GENERAL(INFO, "Increase nonce done");
    // UpdateStateTrie(address, *account);
    return true;
  } else {
    LOG_GENERAL(WARNING, "Increase nonce failed");
    return false;
  }
}

template <class MAP>
uint64_t AccountStoreBase<MAP>::GetNonce(const Address& address) {
  // LOG_MARKER();

  std::shared_ptr<Account> account;
  std::unique_lock<std::mutex> g(GetAccountWMutex(address, account));

  if (account != nullptr) {
    return account->GetNonce();
  }

  return 0;
}

template <class MAP>
void AccountStoreBase<MAP>::PrintAccountState() {
  LOG_MARKER();
  std::lock_guard<std::mutex> g(m_mutexASBase);
  for (const auto& entry : *m_addressToAccount) {
    LOG_GENERAL(INFO, entry.first << " " << entry.second);
  }
}
