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

#include <array>
#include <string>
#include <vector>
#include "common/Constants.h"
#include "libCrypto/Schnorr.h"
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
  Address addr;
  // return Transaction(0, instanceNum, addr,
  //                    Schnorr::GetInstance().GenKeyPair(), 0, 1, 2, {}, {});
  return TransactionWithReceipt(
      Transaction(0, instanceNum, addr, Schnorr::GetInstance().GenKeyPair(), 0,
                  1, 2, {}, {}),
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

    vector<unsigned char> serializedTxBody;
    body1.Serialize(serializedTxBody, 0);
    BlockStorage::GetBlockStorage().PutTxBody(tx_hash, serializedTxBody);

    TxBodySharedPtr body2;
    BlockStorage::GetBlockStorage().GetTxBody(tx_hash, body2);

    // BOOST_CHECK_MESSAGE(body1 == *body2,
    //     "block shouldn't change after writing to/ reading from disk");
  }
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
