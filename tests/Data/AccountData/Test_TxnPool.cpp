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
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/TxnPool.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

std::vector<Transaction> GenerateRandomTransactions(const uint32_t& count) {
  std::vector<Transaction> transaction_v;
  for (uint32_t c = 0; c < count; c++) {
    transaction_v.emplace_back(TxnHash().random(), TestUtils::DistUint32(), TestUtils::DistUint64(), Address().random(), TestUtils::GenerateRandomPubKey(), TestUtils::DistUint128(), TestUtils::DistUint128(), TestUtils::DistUint64(), TestUtils::GenerateRandomCharVector(TestUtils::DistUint8()), TestUtils::GenerateRandomCharVector(TestUtils::DistUint8()), TestUtils::GenerateRandomSignature());
  }
  return transaction_v;
}

BOOST_AUTO_TEST_SUITE(accountstoretest)

BOOST_AUTO_TEST_CASE(txnpool) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  TxnPool tp = TxnPool();

  std::vector<Transaction> transaction_v = GenerateRandomTransactions(TestUtils::Dist1to99() + 1);
  std::unordered_map<TxnHash, Transaction> hashIndex;

  for(uint i = 0; i < transaction_v.size(); i++){
    hashIndex.emplace(transaction_v[i].GetTranID(), transaction_v[i]);
  }
  tp.HashIndex = hashIndex;

  for(uint i = 0; i < transaction_v.size(); i++){
    BOOST_CHECK_EQUAL(true, tp.exist(transaction_v[0].GetTranID()));
    Transaction tran = Transaction();
    BOOST_CHECK_EQUAL(true, tp.get(transaction_v[0].GetTranID(), tran));
    BOOST_CHECK_EQUAL(true, tran == transaction_v[0]);
  }

  BOOST_CHECK_EQUAL(transaction_v.size(),tp.size());

}

BOOST_AUTO_TEST_SUITE_END()
