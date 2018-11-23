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

#ifndef __ACCOUNTSTOREBASE_H__
#define __ACCOUNTSTOREBASE_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop

#include "Account.h"
#include "Address.h"
#include "Transaction.h"
#include "TransactionReceipt.h"
#include "common/Constants.h"
#include "common/Serializable.h"
#include "depends/common/FixedHash.h"
#include "libCrypto/Schnorr.h"

template <class MAP>
class AccountStoreBase : public SerializableDataBlock {
 protected:
  std::shared_ptr<MAP> m_addressToAccount;

  AccountStoreBase();

  bool CalculateGasRefund(const boost::multiprecision::uint128_t& gasDeposit,
                          const uint64_t& gasUnit,
                          const boost::multiprecision::uint128_t& gasPrice,
                          boost::multiprecision::uint128_t& gasRefund);

  bool UpdateAccounts(const Transaction& transaction,
                      TransactionReceipt& receipt);

 public:
  virtual void Init();

  /// Implements the Serialize function inherited from Serializable.
  bool Serialize(std::vector<unsigned char>& dst, unsigned int offset) const;

  /// Implements the Deserialize function inherited from Serializable.
  virtual bool Deserialize(const std::vector<unsigned char>& src,
                           unsigned int offset);

  virtual Account* GetAccount(const Address& address);

  /// Verifies existence of Account in the list.
  bool IsAccountExist(const Address& address);

  /// Adds an Account to the list.
  void AddAccount(const Address& address, const Account& account);
  void AddAccount(const PubKey& pubKey, const Account& account);

  void RemoveAccount(const Address& address);

  size_t GetNumOfAccounts() const;

  bool IncreaseBalance(const Address& address,
                       const boost::multiprecision::uint128_t& delta);
  bool DecreaseBalance(const Address& address,
                       const boost::multiprecision::uint128_t& delta);

  /// Updates the source and destination accounts included in the specified
  /// Transaction.
  bool TransferBalance(const Address& from, const Address& to,
                       const boost::multiprecision::uint128_t& delta);
  boost::multiprecision::uint128_t GetBalance(const Address& address);

  bool IncreaseNonce(const Address& address);
  uint64_t GetNonce(const Address& address);

  virtual void PrintAccountState();
};

#include "AccountStoreBase.tpp"

#endif  // __ACCOUNTSTOREBASE_H__
