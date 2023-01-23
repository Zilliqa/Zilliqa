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

#include "libData/AccountStore/AccountStoreBase.h"

AccountStoreBase::AccountStoreBase() {
  m_addressToAccount = std::make_shared<std::unordered_map<Address, Account>>();
}

void AccountStoreBase::Init() { m_addressToAccount->clear(); }

bool AccountStoreBase::Serialize(zbytes& dst, unsigned int offset) const {
  if (!MessengerAccountStoreBase::SetAccountStore(dst, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::Deserialize(const zbytes& src, unsigned int offset) {
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::Deserialize(const std::string& src,
                                   unsigned int offset) {
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

bool AccountStoreBase::UpdateAccounts(const Transaction& transaction,
                                      TransactionReceipt& receipt,
                                      TxnStatus& error_code) {
  const Address fromAddr = transaction.GetSenderAddr();
  Address toAddr = transaction.GetToAddr();
  const uint128_t amount = transaction.GetAmountQa();
  error_code = TxnStatus::NOT_PRESENT;

  Account* fromAccount = this->GetAccount(fromAddr);
  if (fromAccount == nullptr) {
    LOG_GENERAL(WARNING, "sender " << fromAddr.hex() << " not exist");
    error_code = TxnStatus::INVALID_FROM_ACCOUNT;
    return false;
  }

  if (transaction.GetGasLimitZil() < NORMAL_TRAN_GAS) {
    LOG_GENERAL(WARNING,
                "The gas limit "
                    << transaction.GetGasLimitZil()
                    << " should be larger than the normal transaction gas ("
                    << NORMAL_TRAN_GAS << ")");
    error_code = TxnStatus::INSUFFICIENT_GAS_LIMIT;
    return false;
  }

  uint128_t gasDeposit = 0;
  if (!SafeMath<uint128_t>::mul(transaction.GetGasLimitZil(),
                                transaction.GetGasPriceQa(), gasDeposit)) {
    LOG_GENERAL(
        WARNING,
        "transaction.GetGasLimit() * transaction.GetGasPriceQa() overflow!");
    error_code = TxnStatus::MATH_ERROR;
    return false;
  }

  if (fromAccount->GetBalance() < amount + gasDeposit) {
    LOG_GENERAL(WARNING,
                "The account (balance: "
                    << fromAccount->GetBalance()
                    << ") "
                       "doesn't have enough balance to pay for the gas limit ("
                    << gasDeposit
                    << ") "
                       "with amount ("
                    << amount << ") in the transaction");
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
                          transaction.GetGasPriceQa(), gasRefund)) {
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

bool AccountStoreBase::CalculateGasRefund(const uint128_t& gasDeposit,
                                          const uint64_t& gasUnit,
                                          const uint128_t& gasPrice,
                                          uint128_t& gasRefund) {
  uint128_t gasFee;
  if (!SafeMath<uint128_t>::mul(gasUnit, gasPrice, gasFee)) {
    LOG_GENERAL(WARNING, "gasUnit * gasPrice overflow!");
    return false;
  }

  if (!SafeMath<uint128_t>::sub(gasDeposit, gasFee, gasRefund)) {
    LOG_GENERAL(WARNING, "gasDeposit - gasFee overflow!");
    return false;
  }

  // LOG_GENERAL(INFO, "gas price to refund: " << gasRefund);
  return true;
}

bool AccountStoreBase::IsAccountExist(const Address& address) {
  // LOG_MARKER();
  return (nullptr != GetAccount(address));
}

bool AccountStoreBase::AddAccount(const Address& address,
                                  const Account& account, bool toReplace) {
  // LOG_MARKER();
  if (toReplace || !IsAccountExist(address)) {
    m_addressToAccount->insert_or_assign(address, account);

    return true;
  }
  LOG_GENERAL(WARNING, "Address "
                           << address
                           << " could not be added because already present");
  return false;
}

bool AccountStoreBase::AddAccount(const PubKey& pubKey,
                                  const Account& account) {
  return AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

void AccountStoreBase::RemoveAccount(const Address& address) {
  if (IsAccountExist(address)) {
    m_addressToAccount->erase(address);
  }
}

Account* AccountStoreBase::GetAccount(const Address& address) {
  auto it = m_addressToAccount->find(address);
  if (it != m_addressToAccount->end()) {
    return &it->second;
  }
  return nullptr;
}

size_t AccountStoreBase::GetNumOfAccounts() const {
  // LOG_MARKER();
  return m_addressToAccount->size();
}

bool AccountStoreBase::IncreaseBalance(const Address& address,
                                       const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

  if (account != nullptr && account->IncreaseBalance(delta)) {
    return true;
  } else if (account == nullptr) {
    return AddAccount(address, {delta, 0});
  }

  return false;
}

bool AccountStoreBase::DecreaseBalance(const Address& address,
                                       const uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

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

bool AccountStoreBase::TransferBalance(const Address& from, const Address& to,
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

uint128_t AccountStoreBase::GetBalance(const Address& address) {
  // LOG_MARKER();

  const Account* account = GetAccount(address);

  if (account != nullptr) {
    return account->GetBalance();
  }

  return 0;
}

bool AccountStoreBase::IncreaseNonce(const Address& address) {
  // LOG_MARKER();

  Account* account = GetAccount(address);

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

uint64_t AccountStoreBase::GetNonce(const Address& address) {
  // LOG_MARKER();

  Account* account = GetAccount(address);

  if (account != nullptr) {
    return account->GetNonce();
  }

  return 0;
}

void AccountStoreBase::PrintAccountState() {
  LOG_MARKER();
  for (const auto& entry : *m_addressToAccount) {
    LOG_GENERAL(INFO, entry.first << " " << entry.second);
  }
}
