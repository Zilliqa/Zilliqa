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

#include <array>
#include <map>
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/TxnPool.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

using namespace boost::multiprecision;

template <class MAP, class KEY>
bool checkExistenceAndAdd(MAP& m, const KEY& k) {
  if (m.find(k) != m.end()) {
    return false;
  } else {
    m[k] = 1;
    return true;
  }
}

Transaction createTransaction(const uint128_t& gasPrice,
                              const PubKey& senderPubKey,
                              const uint64_t& nonce) {
  return Transaction(
      TestUtils::DistUint32(), nonce, Address().random(), senderPubKey,
      TestUtils::DistUint128(), gasPrice, TestUtils::DistUint64(),
      TestUtils::GenerateRandomCharVector(TestUtils::DistUint8()),
      TestUtils::GenerateRandomCharVector(TestUtils::DistUint8()),
      TestUtils::GenerateRandomSignature());
}

Transaction generateUniqueTransaction() {
  Transaction transaction;

  static std::map<uint64_t, bool> nonce_m;
  static std::map<PubKey, bool> senderPubKey_m;
  static std::map<TxnHash, bool> tranID_m;
  static std::map<uint128_t, bool> gasPrice_m;

  uint128_t gasPrice;
  TxnHash tranID;
  PubKey senderPubKey;
  uint64_t nonce;

  // Make sure that all the initializers for a Transaction object used later for
  // TxnPool will be unique (not necessary but easier to check all at
  // once rather than just tuples).
  bool uniq = true;
  do {
    gasPrice = TestUtils::DistUint128();
    uniq &= checkExistenceAndAdd(gasPrice_m, gasPrice);

    tranID = TxnHash().random();
    uniq &= checkExistenceAndAdd(tranID_m, tranID);

    senderPubKey = TestUtils::GenerateRandomPubKey();
    uniq &= checkExistenceAndAdd(senderPubKey_m, senderPubKey);

    nonce = TestUtils::DistUint64();
    uniq &= checkExistenceAndAdd(nonce_m, nonce);
  } while (!uniq);

  return createTransaction(gasPrice, senderPubKey, nonce);
}

std::vector<Transaction> generateUniqueTransactionVector(
    std::vector<Transaction>& transaction_v, const uint32_t& count) {
  for (uint32_t c = 0; c < count; c++) {
    transaction_v.push_back(generateUniqueTransaction());
  }
  return transaction_v;
}

BOOST_AUTO_TEST_SUITE(accountstoretest)

BOOST_AUTO_TEST_CASE(txnpool) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TestUtils::Initialize();

  TxnPool tp = TxnPool();

  std::vector<Transaction> transaction_v;
  generateUniqueTransactionVector(transaction_v, TestUtils::Dist1to99() + 1);

  // ============================================================
  // Insert Transactions in TxnPool and check that inserted elements can be
  // queried back
  // ============================================================
  MempoolInsertionStatus status;
  for (auto& t : transaction_v) {
    BOOST_CHECK_EQUAL(true, tp.insert(t, status));
    BOOST_CHECK_EQUAL(true, tp.exist(t.GetTranID()));
    Transaction tran = Transaction();
    BOOST_CHECK_EQUAL(true, tp.get(t.GetTranID(), tran));
    BOOST_CHECK_EQUAL(true, tran == t);
  }
  // ============================================================
  // Try to find non existing Transaction
  // ============================================================
  Transaction tran_unique = generateUniqueTransaction();
  BOOST_CHECK_EQUAL(false, tp.exist(tran_unique.GetTranID()));

  // ============================================================
  // Check that number of inserted elements equals
  // ============================================================
  BOOST_CHECK_EQUAL(transaction_v.size(), tp.size());

  // ============================================================
  // Try to insert the same Transaction again
  // ============================================================
  BOOST_CHECK_EQUAL(false,
                    tp.insert(transaction_v[TestUtils::RandomIntInRng<uint8_t>(
                                  0, transaction_v.size() - 1)],
                              status));

  // ============================================================
  // Insert existing Transaction but with higher gas price (+1)
  // ============================================================
  Transaction transactionHigherGas;
  // Find the existing Transaction with gasPrice < MAX (gasPrice)
  int i = 0;
  while (true) {
    uint128_t gasprice = transaction_v[i].GetGasPrice();
    if (gasprice < gasprice + 1) {
      transactionHigherGas =
          createTransaction(gasprice + 1, transaction_v[i].GetSenderPubKey(),
                            transaction_v[i].GetNonce());
      break;
    }
    ++i;
  }
  BOOST_CHECK_EQUAL(true, tp.insert(transactionHigherGas, status));

  // ============================================================
  // Test if findSameNonceButHigherGas returns same Transaction but with higher
  // gas
  // ============================================================
  Transaction transactionLoverGas_test = transaction_v[i];
  tp.findSameNonceButHigherGas(transactionLoverGas_test);
  BOOST_CHECK_EQUAL(true, transactionHigherGas == transactionLoverGas_test);

  // ============================================================
  // Try to find all the transactions
  // ============================================================
  Transaction transactionTest;
  uint size = tp.size();
  for (uint i = 0; i < size; i++) {
    BOOST_CHECK_EQUAL(true, tp.findOne(transactionTest));
  }
  BOOST_CHECK_EQUAL(false, tp.findOne(transactionTest));
}

BOOST_AUTO_TEST_CASE(txnpool_status) {
  TxnPool tp;

  Transaction txn = generateUniqueTransaction();
  const uint128_t& gasprice = txn.GetGasPrice();

  Transaction higherGasTxn =
      createTransaction(gasprice + 1, txn.GetSenderPubKey(), txn.GetNonce());

  MempoolInsertionStatus status;

  /// Try inserting lower gas txn first and then higher gas

  BOOST_CHECK_EQUAL(true, tp.insert(txn, status));
  BOOST_CHECK_EQUAL(status.first, TxnStatus::NOT_PRESENT);

  BOOST_CHECK_EQUAL(true, tp.insert(higherGasTxn, status));
  BOOST_CHECK_EQUAL(status.first, TxnStatus::MEMPOOL_SAME_NONCE_LOWER_GAS);
  BOOST_CHECK_EQUAL(status.second, txn.GetTranID());

  ///

  tp.clear();

  /// Try inserting higher gas txn first and then lower gas

  BOOST_CHECK_EQUAL(true, tp.insert(higherGasTxn, status));
  BOOST_CHECK_EQUAL(status.first, TxnStatus::NOT_PRESENT);

  BOOST_CHECK_EQUAL(false, tp.insert(txn, status));
  BOOST_CHECK_EQUAL(status.first, TxnStatus::MEMPOOL_SAME_NONCE_LOWER_GAS);
  BOOST_CHECK_EQUAL(status.second, txn.GetTranID());
}

BOOST_AUTO_TEST_SUITE_END()
