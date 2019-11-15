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
#include <Schnorr.h>
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/RootComputation.h"

#include <cstdint>
#include <vector>

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

Transaction createDummyTransaction() {
  Address toAddr;
  for (unsigned int i = 0; i < toAddr.asArray().size(); i++) {
    toAddr.asArray().at(i) = i + 4;
  }

  Transaction tx(DataConversion::Pack(CHAIN_ID, 1), 5, toAddr,
                 Schnorr::GenKeyPair(), 55, PRECISION_MIN_VALUE, 22, {0x33},
                 {0x44});
  return tx;
}

decltype(auto) generateDummyTransactions(size_t n) {
  std::unordered_map<TxnHash, Transaction> txns;

  for (auto i = 0u; i != n; i++) {
    auto txn = createDummyTransaction();
    txns.emplace(txn.GetTranID(), txn);
  }

  return txns;
}

BOOST_AUTO_TEST_CASE(compareAllThreeVersions) {
  auto txnMap1 = generateDummyTransactions(100);
  auto txnMap2 = generateDummyTransactions(100);

  std::vector<TxnHash> txnHashVec;  // join the hashes of two lists;
  std::list<Transaction> txnList1, txnList2;

  for (auto& txnPair : txnMap1) {
    txnHashVec.emplace_back(txnPair.first);
    txnList1.emplace_back(txnPair.second);
  }

  for (auto& txnPair : txnMap2) {
    txnHashVec.emplace_back(txnPair.first);
    txnList2.emplace_back(txnPair.second);
  }

  auto hashRoot1 = ComputeRoot(txnHashVec);
  auto hashRoot2 = ComputeRoot(txnList1, txnList2);
  auto hashRoot3 = ComputeRoot(txnMap1, txnMap2);

  BOOST_CHECK_EQUAL(hashRoot1, hashRoot2);
  BOOST_CHECK_EQUAL(hashRoot1, hashRoot3);
}

BOOST_AUTO_TEST_SUITE_END()
