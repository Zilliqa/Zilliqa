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

Transaction crateTransaction(const uint128_t& gasPrice, const TxnHash& tranID,
                             const PubKey& senderPubKey,
                             const uint64_t& nonce) {
  return Transaction(
      tranID, TestUtils::DistUint32(), nonce, Address().random(), senderPubKey,
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

  // Make sure that all the initializers for a Transaction object later used for
  // TxnPool members will be unique not necessary but easier to check all at
  // once rather than just necessary tuples.
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

  return crateTransaction(gasPrice, tranID, senderPubKey, nonce);
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

  for (uint i = 0; i < transaction_v.size(); i++) {
    BOOST_CHECK_EQUAL(true, tp.insert(transaction_v[i]));
    BOOST_CHECK_EQUAL(true, tp.exist(transaction_v[i].GetTranID()));
    Transaction tran = Transaction();
    BOOST_CHECK_EQUAL(true, tp.get(transaction_v[i].GetTranID(), tran));
    BOOST_CHECK_EQUAL(true, tran == transaction_v[i]);
  }

  BOOST_CHECK_EQUAL(transaction_v.size(), tp.size());
  BOOST_CHECK_EQUAL(false,
                    tp.insert(transaction_v[TestUtils::RandomIntInRng<uint8_t>(
                        0, transaction_v.size() - 1)]));

  Transaction transactionHigherGas;
  while (true) {
    int i = 0;
    uint128_t gasprice = transaction_v[i].GetGasPrice();
    if (gasprice < gasprice + 1) {
      transactionHigherGas = crateTransaction(
          gasprice + 1, transaction_v[i].GetTranID(),
          transaction_v[i].GetSenderPubKey(), transaction_v[i].GetNonce());
      break;
    }
    ++i;
  }
  tp.insert(transactionHigherGas);
  Transaction transactionHigherGas_test = transactionHigherGas;
  tp.findSameNonceButHigherGas(transactionHigherGas_test);
  BOOST_CHECK_EQUAL(true, transactionHigherGas == transactionHigherGas_test);

  BOOST_CHECK_EQUAL(true, tp.findOne(transactionHigherGas_test));
  Transaction tran_unique = generateUniqueTransaction();

  Transaction transactionTest;
  uint size = tp.size();
  for (uint i = 0; i < size; i++) {
    BOOST_CHECK_EQUAL(true, tp.findOne(transactionTest));
  }
  BOOST_CHECK_EQUAL(false, tp.findOne(transactionTest));
}

BOOST_AUTO_TEST_SUITE_END()
