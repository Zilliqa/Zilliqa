/*
 * Copyright (C) 2020 Zilliqa
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

#include "libRemoteStorageDB/RemoteStorageDB.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE mongodbTest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(mongodbtest)

BOOST_AUTO_TEST_CASE(test_mongo) {
  const uint64_t& epochNum = 1000;
  const auto& txn1 = TestUtils::GenerateRandomTransaction(
      TRANSACTION_VERSION, 1, Transaction::NON_CONTRACT);
  const auto& txn1_hash = txn1.GetTranID().hex();

  const auto& txn2 = TestUtils::GenerateRandomTransaction(
      TRANSACTION_VERSION, 1, Transaction::NON_CONTRACT);

  const auto& txn2_hash = txn2.GetTranID().hex();

  RemoteStorageDB::GetInstance().Init(false);
  RemoteStorageDB::GetInstance().InsertTxn(txn1, TxnStatus::DISPATCHED,
                                           epochNum);
  RemoteStorageDB::GetInstance().ExecuteWrite();
  auto query_ret = RemoteStorageDB::GetInstance().QueryTxnHash(txn1_hash);
  cout << query_ret.toStyledString() << endl;

  RemoteStorageDB::GetInstance().UpdateTxn(txn1_hash, TxnStatus::CONFIRMED,
                                           epochNum + 2, true);
  RemoteStorageDB::GetInstance().ExecuteWrite();
  query_ret = RemoteStorageDB::GetInstance().QueryTxnHash(txn1_hash);
  cout << query_ret.toStyledString() << endl;
  // try and insert same txn
  RemoteStorageDB::GetInstance().InsertTxn(txn1, TxnStatus::DISPATCHED,
                                           epochNum);
  RemoteStorageDB::GetInstance().ExecuteWrite();

  // try and query non-existent txn
  query_ret = RemoteStorageDB::GetInstance().QueryTxnHash("abcd");
  // a null JSON is returned
  BOOST_CHECK_EQUAL(query_ret, Json::Value::null);

  // try and update a non-existent txn
  RemoteStorageDB::GetInstance().UpdateTxn(txn2_hash, TxnStatus::DISPATCHED,
                                           epochNum + 2, true);
  RemoteStorageDB::GetInstance().ExecuteWrite();

  query_ret = RemoteStorageDB::GetInstance().QueryTxnHash(txn2_hash);
  BOOST_CHECK_EQUAL(Json::Value::null, query_ret);

  // try and update to backward modification state
  RemoteStorageDB::GetInstance().UpdateTxn(txn1_hash, TxnStatus::SOFT_CONFIRMED,
                                           epochNum + 1, true);
  RemoteStorageDB::GetInstance().ExecuteWrite();

  RemoteStorageDB::GetInstance().InsertTxn(txn2, TxnStatus::DISPATCHED,
                                           epochNum + 3);
  RemoteStorageDB::GetInstance().ExecuteWrite();

  // insert 100 txns
  const auto num = 100;

  for (int i = 0; i < num; i++) {
    const auto& bulk_txn = TestUtils::GenerateRandomTransaction(
        TRANSACTION_VERSION, i + 1, Transaction::NON_CONTRACT);
    RemoteStorageDB::GetInstance().InsertTxn(bulk_txn, TxnStatus::DISPATCHED,
                                             epochNum + 5);
  }

  RemoteStorageDB::GetInstance().ExecuteWrite();
}
BOOST_AUTO_TEST_SUITE_END()
