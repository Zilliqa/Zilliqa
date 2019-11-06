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
#include <array>
#include <string>
#include <vector>
#include "common/Constants.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TransactionReceipt.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libUtils/TimeUtils.h"

#include <boost/filesystem.hpp>

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testReadWriteSimpleStringToDB) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  DB db("test.db");

  db.WriteToDB("fruit", "vegetable");

  string ret = db.ReadFromDB("fruit");

  BOOST_CHECK_MESSAGE(
      ret == "vegetable",
      "ERROR: return value from DB not equal to inserted value");
}

TransactionWithReceipt constructDummyTxBody(int instanceNum) {
  Address toAddr;

  for (unsigned int i = 0; i < toAddr.asArray().size(); i++) {
    toAddr.asArray().at(i) = i + 8;
  }

  // return Transaction(0, instanceNum, addr,
  //                    Schnorr::GenKeyPair(), 0, 1, 2, {}, {});
  return TransactionWithReceipt(
      Transaction(0, instanceNum, toAddr, Schnorr::GenKeyPair(), 0, 1, 2, {},
                  {}),
      TransactionReceipt());
}

BOOST_AUTO_TEST_CASE(testSerializationDeserialization) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // checking if normal serialization and deserialization of blocks is working
  // or not

  TransactionWithReceipt body1 = constructDummyTxBody(0);

  bytes serializedTxBody;
  body1.Serialize(serializedTxBody, 0);

  TransactionWithReceipt body2(serializedTxBody, 0);

  BOOST_CHECK_MESSAGE(
      body1.GetTransaction().GetTranID() == body2.GetTransaction().GetTranID(),
      "Error: Transaction id shouldn't change after "
      "serailization and deserialization");
}

BOOST_AUTO_TEST_CASE(testBlockStorage) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    TransactionWithReceipt body1 = constructDummyTxBody(0);

    auto tx_hash = body1.GetTransaction().GetTranID();

    bytes serializedTxBody;
    body1.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash, serializedTxBody);

    TxBodySharedPtr body2;
    BlockStorage::GetBlockStorage().GetTxBody(tx_hash, body2);

    // BOOST_CHECK_MESSAGE(body1 == *body2,
    //     "block shouldn't change after writing to/ reading from disk");
  }
}

BOOST_AUTO_TEST_CASE(testTRDeserializationFromFile) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // checking if serialization and deserialization of TransactionWithReceipt
  // to/from file is working or not

  std::unordered_map<TxnHash, TransactionWithReceipt> txns;
  TransactionWithReceipt tx_body = constructDummyTxBody(0);
  auto tx_hash = tx_body.GetTransaction().GetTranID();

  ostringstream oss;
  oss << "/tmp/txns.1";
  string txns_filename = oss.str();
  ofstream ofile(txns_filename, std::fstream::binary);

  bytes serializedTxn;
  tx_body.Serialize(serializedTxn, 0);

  // write HASH LEN and HASH
  size_t size = tx_hash.size;
  ofile.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
  ofile.write(reinterpret_cast<const char*>(tx_hash.data()), size);

  // write TXN LEN AND TXN
  size = serializedTxn.size();
  ofile.write(reinterpret_cast<const char*>(&size), sizeof(size_t));
  ofile.write(reinterpret_cast<const char*>(serializedTxn.data()), size);
  ofile.close();

  // Now read back file to see if the TransactionWithReceipt is good
  ifstream infile(txns_filename, ios::in | ios::binary);
  TxnHash r_txn_hash;
  bytes buff;

  // get the txnHash length and raw bytes of txnHash itself
  size_t len;
  infile.read(reinterpret_cast<char*>(&len), sizeof(len));
  infile.read(reinterpret_cast<char*>(&r_txn_hash), len);

  // get the TxnReceipt length and raw bytes of TxnReceipt itself
  infile.read(reinterpret_cast<char*>(&len), sizeof(len));
  buff.resize(len);
  infile.read(reinterpret_cast<char*>(buff.data()), len);

  infile.close();

  // Deserialize the TxnReceipt bytes
  TransactionWithReceipt r_tr;
  r_tr.Deserialize(buff, 0);

  BOOST_CHECK_MESSAGE(r_tr.GetTransaction().GetTranID() == tx_hash,
                      "Error: Transaction id shouldn't change after "
                      "serailization and deserialization from binary file");

  BOOST_CHECK_MESSAGE(
      r_tr.GetTransaction().GetToAddr() == tx_body.GetTransaction().GetToAddr(),
      "Error: ToAddress shouldn't change after "
      "serailization and deserialization from binary file");

  BOOST_CHECK_MESSAGE(
      r_tr.GetTransaction().GetTranID() == r_txn_hash,
      "Error: Transaction id field in binary file and  "
      "that in deserialized TR from binary file should have been same");
}

BOOST_AUTO_TEST_CASE(testRandomBlockAccesses) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();
  if (LOOKUP_NODE_MODE) {
    TransactionWithReceipt body1 = constructDummyTxBody(1);
    TransactionWithReceipt body2 = constructDummyTxBody(2);
    TransactionWithReceipt body3 = constructDummyTxBody(3);
    TransactionWithReceipt body4 = constructDummyTxBody(4);

    auto tx_hash1 = body1.GetTransaction().GetTranID();
    auto tx_hash2 = body2.GetTransaction().GetTranID();
    auto tx_hash3 = body3.GetTransaction().GetTranID();
    auto tx_hash4 = body4.GetTransaction().GetTranID();

    bytes serializedTxBody;

    body1.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash1, serializedTxBody);

    serializedTxBody.clear();
    body2.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash2, serializedTxBody);

    serializedTxBody.clear();
    body3.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash3, serializedTxBody);

    serializedTxBody.clear();
    body4.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash4, serializedTxBody);

    TxBodySharedPtr blockRetrieved;
    BlockStorage::GetBlockStorage().GetTxBody(tx_hash2, blockRetrieved);

    BOOST_CHECK_MESSAGE(
        body2.GetTransaction().GetTranID() ==
            (*blockRetrieved).GetTransaction().GetTranID(),
        "transaction id shouldn't change after writing to/ reading from "
        "disk");

    BlockStorage::GetBlockStorage().GetTxBody(tx_hash4, blockRetrieved);

    BOOST_CHECK_MESSAGE(
        body4.GetTransaction().GetTranID() ==
            (*blockRetrieved).GetTransaction().GetTranID(),
        "transaction id shouldn't change after writing to/ reading from "
        "disk");

    BlockStorage::GetBlockStorage().GetTxBody(tx_hash1, blockRetrieved);

    BOOST_CHECK_MESSAGE(
        body1.GetTransaction().GetTranID() ==
            (*blockRetrieved).GetTransaction().GetTranID(),
        "transaction id shouldn't change after writing to/ reading from "
        "disk");

    BOOST_CHECK_MESSAGE(
        body2.GetTransaction().GetTranID() !=
            (*blockRetrieved).GetTransaction().GetTranID(),
        "transaction id shouldn't be same for different blocks");
  }
}

BOOST_AUTO_TEST_SUITE_END()
