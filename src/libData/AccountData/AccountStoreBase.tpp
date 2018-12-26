/*
 * Copyright (c) 2018 Zilliqa
 * This source code is being disclosed to you solely for the purpose of your
 * participation in testing Zilliqa. You may view, compile and run the code for
 * that purpose and pursuant to the protocols and algorithms that are programmed
 * into, and intended by, the code. You may not do anything else with the code
 * without express permission from Zilliqa Research Pte. Ltd., including
 * modifying or publishing the code (or any part of it), and developing or
 * forming another public or private blockchain network. This source code is
 * provided 'as is' and no warranties are given as to title or non-infringement,
 * merchantability or fitness for purpose and, to the extent permitted by law,
 * all liability for your use of the code is disclaimed. Some programs in this
 * code are governed by the GNU General Public License v3.0 (available at
 * https://www.gnu.org/licenses/gpl-3.0.en.html) ('GPLv3'). The programs that
 * are governed by GPLv3.0 are those programs that are located in the folders
 * src/depends and tests/depends and which include a reference to GPLv3 in their
 * program files.
 */

#include <type_traits>

#include "libMessage/MessengerAccountStoreBase.h"
#include "libUtils/Logger.h"
#include "libUtils/SafeMath.h"

template <class MAP>
AccountStoreBase<MAP>::AccountStoreBase() {
  m_addressToAccount = std::make_shared<MAP>();
}

template <class MAP>
void AccountStoreBase<MAP>::Init() {
  m_addressToAccount->clear();
}

template <class MAP>
bool AccountStoreBase<MAP>::Serialize(bytes& dst, unsigned int offset) const {
  if (!MessengerAccountStoreBase::SetAccountStore(dst, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::SetAccountStore failed.");
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::Deserialize(const bytes& src, unsigned int offset) {
  if (!MessengerAccountStoreBase::GetAccountStore(src, offset,
                                                  *m_addressToAccount)) {
    LOG_GENERAL(WARNING, "Messenger::GetAccountStore failed.");
    return false;
  }

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::UpdateAccounts(const Transaction& transaction,
                                           TransactionReceipt& receipt) {
  const PubKey& senderPubKey = transaction.GetSenderPubKey();
  const Address fromAddr = Account::GetAddressFromPublicKey(senderPubKey);
  Address toAddr = transaction.GetToAddr();
  const boost::multiprecision::uint128_t& amount = transaction.GetAmount();

  Account* fromAccount = this->GetAccount(fromAddr);
  if (fromAccount == nullptr) {
    LOG_GENERAL(WARNING, "sender " << fromAddr.hex() << " not exist");
    return false;
  }

  if (transaction.GetGasLimit() < NORMAL_TRAN_GAS) {
    LOG_GENERAL(WARNING,
                "The gas limit "
                    << transaction.GetGasLimit()
                    << " should be larger than the normal transaction gas ("
                    << NORMAL_TRAN_GAS << ")");
    return false;
  }

  // FIXME: Possible integer overflow here
  boost::multiprecision::uint128_t gasDeposit =
      transaction.GetGasLimit() * transaction.GetGasPrice();

  if (fromAccount->GetBalance() < transaction.GetAmount() + gasDeposit) {
    LOG_GENERAL(WARNING,
                "The account (balance: "
                    << fromAccount->GetBalance()
                    << ") "
                       "doesn't have enough balance to pay for the gas limit ("
                    << gasDeposit
                    << ") "
                       "with amount ("
                    << transaction.GetAmount() << ") in the transaction");
    return false;
  }

  if (!DecreaseBalance(fromAddr, gasDeposit)) {
    return false;
  }

  if (!TransferBalance(fromAddr, toAddr, amount)) {
    IncreaseBalance(fromAddr, gasDeposit);
    return false;
  }

  boost::multiprecision::uint128_t gasRefund;
  if (!CalculateGasRefund(gasDeposit, NORMAL_TRAN_GAS,
                          transaction.GetGasPrice(), gasRefund)) {
    return false;
  }

  IncreaseBalance(fromAddr, gasRefund);

  IncreaseNonce(fromAddr);

  receipt.SetResult(true);
  receipt.SetCumGas(NORMAL_TRAN_GAS);
  receipt.update();

  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::CalculateGasRefund(
    const boost::multiprecision::uint128_t& gasDeposit, const uint64_t& gasUnit,
    const boost::multiprecision::uint128_t& gasPrice,
    boost::multiprecision::uint128_t& gasRefund) {
  boost::multiprecision::uint128_t gasFee;
  if (!SafeMath<boost::multiprecision::uint128_t>::mul(gasUnit, gasPrice,
                                                       gasFee)) {
    LOG_GENERAL(WARNING, "gasUnit * transaction.GetGasPrice() overflow!");
    return false;
  }

  if (!SafeMath<boost::multiprecision::uint128_t>::sub(gasDeposit, gasFee,
                                                       gasRefund)) {
    LOG_GENERAL(WARNING, "gasDeposit - gasFee overflow!");
    return false;
  }

  // LOG_GENERAL(INFO, "gas price to refund: " << gasRefund);
  return true;
}

template <class MAP>
bool AccountStoreBase<MAP>::IsAccountExist(const Address& address) {
  // LOG_MARKER();
  return (nullptr != GetAccount(address));
}

template <class MAP>
void AccountStoreBase<MAP>::AddAccount(const Address& address,
                                       const Account& account) {
  // LOG_MARKER();

  if (!IsAccountExist(address)) {
    m_addressToAccount->insert(std::make_pair(address, account));
    // UpdateStateTrie(address, account);
  }
}

template <class MAP>
void AccountStoreBase<MAP>::AddAccount(const PubKey& pubKey,
                                       const Account& account) {
  AddAccount(Account::GetAddressFromPublicKey(pubKey), account);
}

template <class MAP>
void AccountStoreBase<MAP>::RemoveAccount(const Address& address) {
  if (IsAccountExist(address)) {
    m_addressToAccount->erase(address);
  }
}

template <class MAP>
Account* AccountStoreBase<MAP>::GetAccount(const Address& address) {
  auto it = m_addressToAccount->find(address);
  if (it != m_addressToAccount->end()) {
    return &it->second;
  }
  return nullptr;
}

template <class MAP>
size_t AccountStoreBase<MAP>::GetNumOfAccounts() const {
  // LOG_MARKER();
  return m_addressToAccount->size();
}

template <class MAP>
bool AccountStoreBase<MAP>::IncreaseBalance(
    const Address& address, const boost::multiprecision::uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

  // LOG_GENERAL(INFO, "address: " << address);

  if (account != nullptr && account->IncreaseBalance(delta)) {
    // UpdateStateTrie(address, *account);
    // LOG_GENERAL(INFO, "account: " << *account);
    return true;
  }
  // FIXME: remove this, temporary way to test transactions, should return false
  else if (account == nullptr) {
    LOG_GENERAL(WARNING,
                "AddAccount... FIXME: remove this, temporary way to test "
                "transactions");
    AddAccount(address, {delta, 0});
    return true;
  }

  return false;
}

template <class MAP>
bool AccountStoreBase<MAP>::DecreaseBalance(
    const Address& address, const boost::multiprecision::uint128_t& delta) {
  // LOG_MARKER();

  if (delta == 0) {
    return true;
  }

  Account* account = GetAccount(address);

  if (nullptr == account) {
    LOG_GENERAL(WARNING, "Account " << address.hex() << " not exist");
    return false;
  }

  return account->DecreaseBalance(delta);
}

template <class MAP>
bool AccountStoreBase<MAP>::TransferBalance(
    const Address& from, const Address& to,
    const boost::multiprecision::uint128_t& delta) {
  // LOG_MARKER();
  // FIXME: Is there any elegent way to implement this atomic change on balance?
  if (DecreaseBalance(from, delta)) {
    if (IncreaseBalance(to, delta)) {
      return true;
    } else {
      IncreaseBalance(from, delta);
    }
  }

  return false;
}

template <class MAP>
boost::multiprecision::uint128_t AccountStoreBase<MAP>::GetBalance(
    const Address& address) {
  // LOG_MARKER();

  const Account* account = GetAccount(address);

  if (account != nullptr) {
    return account->GetBalance();
  }

  return 0;
}

template <class MAP>
bool AccountStoreBase<MAP>::IncreaseNonce(const Address& address) {
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

template <class MAP>
uint64_t AccountStoreBase<MAP>::GetNonce(const Address& address) {
  // LOG_MARKER();

  Account* account = GetAccount(address);

  if (account != nullptr) {
    return account->GetNonce();
  }

  return 0;
}

template <class MAP>
void AccountStoreBase<MAP>::PrintAccountState() {
  LOG_MARKER();
  for (auto entry : *m_addressToAccount) {
    LOG_GENERAL(INFO, entry.first << " " << entry.second);
  }
}
