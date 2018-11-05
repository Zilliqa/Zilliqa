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

#include <iostream>
#include "libData/AccountData/Transaction.h"
#include "libMessage/Message.pb.h"
#include "libMessage/Messenger.h"
#include "libMessage/ZilliqaMessage.pb.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE message
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace ZilliqaMessage;

BOOST_AUTO_TEST_SUITE(message)

BOOST_AUTO_TEST_CASE(testMessage) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  // Verify that the version of the library that we linked against is compatible
  // with the version of the headers we compiled against.
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  ProtoMessage::Test test;
  vector<uint8_t> testVec = {0, 1, 2, 3, 4, 5};

  // ========================================================================
  // Initialize
  // ========================================================================

  BOOST_CHECK(test.IsInitialized() == false);

  // Primitives
  test.set_m_uint32(32);
  test.set_m_uint64(64);
  test.set_m_bool(true);

  // Byte array
  test.set_m_bytes(testVec.data(), testVec.size());

  // Repeated, ordered type (primitive)
  for (int i = 0; i < 3; i++) {
    test.add_m_bitmap((i % 2) == 0);
  }

  // Repeated, ordered type (class)
  for (int i = 0; i < 3; i++) {
    test.add_m_nested()->set_m_uint32(i);
  }

  // Enumeration
  test.set_m_type(ProtoMessage::TYPE::TYPE_1);

  // Backward-compatible map
  for (int i = 0; i < 3; i++) {
    ProtoMessage::Test::MapEntry* entry = test.add_m_map();
    entry->set_m_key(i);
    entry->mutable_m_val()->set_m_uint32(i * 2);
  }

  // ========================================================================
  // Check initialized message
  // ========================================================================

  BOOST_CHECK(test.IsInitialized() == true);

  // Primitives
  BOOST_CHECK(test.m_uint32() == 32);
  BOOST_CHECK(test.m_uint64() == 64);
  BOOST_CHECK(test.m_bool() == true);

  // Byte array
  BOOST_CHECK(equal(testVec.begin(), testVec.end(), test.m_bytes().begin()));

  // Repeated, ordered type (primitive)
  BOOST_CHECK(test.m_bitmap_size() == 3);
  for (int i = 0; i < test.m_bitmap_size(); i++) {
    BOOST_CHECK(test.m_bitmap(i) == ((i % 2) == 0));
  }

  // Repeated, ordered type (class)
  BOOST_CHECK(test.m_nested_size() == 3);
  for (int i = 0; i < test.m_nested_size(); i++) {
    BOOST_CHECK(test.m_nested(i).m_uint32() == (uint32_t)i);
  }

  // Enumeration
  BOOST_CHECK(test.m_type() == ProtoMessage::TYPE::TYPE_1);

  // Backward-compatible map
  BOOST_CHECK(test.m_map_size() == 3);
  for (int i = 0; i < test.m_map_size(); i++) {
    BOOST_CHECK(test.m_map(i).m_key() == (uint32_t)i);
    BOOST_CHECK(test.m_map(i).m_val().m_uint32() == (uint32_t)(i * 2));
  }

  LOG_GENERAL(INFO, "Debug string:\n" << test.DebugString());

  // ========================================================================
  // Serialize
  // ========================================================================
  vector<uint8_t> serialized(test.ByteSize());
  BOOST_CHECK(test.SerializeToArray(serialized.data(), test.ByteSize()) ==
              true);

  // ========================================================================
  // Deserialize
  // ========================================================================
  ProtoMessage::Test test2;
  BOOST_CHECK(test2.ParseFromArray(serialized.data(), serialized.size()) ==
              true);

  // ========================================================================
  // Check deserialized message
  // ========================================================================
  BOOST_CHECK(test2.IsInitialized() == true);

  // Primitives
  BOOST_CHECK(test2.m_uint32() == 32);
  BOOST_CHECK(test2.m_uint64() == 64);
  BOOST_CHECK(test2.m_bool() == true);

  // Byte array
  BOOST_CHECK(equal(testVec.begin(), testVec.end(), test2.m_bytes().begin()));

  // Repeated, ordered type (primitive)
  BOOST_CHECK(test2.m_bitmap_size() == 3);
  for (int i = 0; i < test2.m_bitmap_size(); i++) {
    BOOST_CHECK(test2.m_bitmap(i) == ((i % 2) == 0));
  }

  // Repeated, ordered type (class)
  BOOST_CHECK(test2.m_nested_size() == 3);
  for (int i = 0; i < test2.m_nested_size(); i++) {
    BOOST_CHECK(test2.m_nested(i).m_uint32() == (uint32_t)i);
  }

  // Enumeration
  BOOST_CHECK(test2.m_type() == ProtoMessage::TYPE::TYPE_1);

  // Backward-compatible map
  BOOST_CHECK(test2.m_map_size() == 3);
  for (int i = 0; i < test2.m_map_size(); i++) {
    BOOST_CHECK(test2.m_map(i).m_key() == (uint32_t)i);
    BOOST_CHECK(test2.m_map(i).m_val().m_uint32() == (uint32_t)(i * 2));
  }
}

BOOST_AUTO_TEST_CASE(test_transaction) {
  boost::multiprecision::uint256_t version = 0;
  boost::multiprecision::uint256_t nonce = 1000;
  Address toAddr;
  PrivKey privKey(
      DataConversion::StringToCharArray(
          "154AF167F12D4C2CEE867186AB03D24FCA2530760DBB1140F122D1E79B020A64"),
      0);
  PubKey pubKey(privKey);
  KeyPair senderKeyPair(privKey, pubKey);
  boost::multiprecision::uint256_t amount = 1000;
  boost::multiprecision::uint256_t gasPrice = 50;
  boost::multiprecision::uint256_t gasLimit = 50;
  std::vector<unsigned char> code;
  std::vector<unsigned char> data;
  Transaction transaction(version, nonce, toAddr, senderKeyPair, amount,
                          gasPrice, gasLimit, code, data);
  dev::bytes dst;
  Messenger::SetTransaction(dst, 0, transaction);
  Transaction transaction1;
  Messenger::GetTransaction(dst, 0, transaction1);

  BOOST_REQUIRE(transaction == transaction1);
}

BOOST_AUTO_TEST_CASE(test_transaction_array) {
  boost::multiprecision::uint256_t version = 0;
  boost::multiprecision::uint256_t nonce = 1000;
  Address toAddr;
  PrivKey privKey(
      DataConversion::StringToCharArray(
          "154AF167F12D4C2CEE867186AB03D24FCA2530760DBB1140F122D1E79B020A64"),
      0);
  PubKey pubKey(privKey);
  KeyPair senderKeyPair(privKey, pubKey);
  boost::multiprecision::uint256_t amount = 1000;
  boost::multiprecision::uint256_t gasPrice = 50;
  boost::multiprecision::uint256_t gasLimit = 50;
  std::vector<Transaction> transactions;
  transactions.emplace_back(version, nonce, toAddr, senderKeyPair, amount,
                            gasPrice, gasLimit, std::vector<unsigned char>{},
                            std::vector<unsigned char>{});
  transactions.emplace_back(
      version, nonce, toAddr, senderKeyPair, amount, gasPrice, gasLimit,
      std::vector<unsigned char>{1, 2}, std::vector<unsigned char>{3, 4});
  transactions.emplace_back(
      version, nonce, toAddr, senderKeyPair, amount, gasPrice, gasLimit,
      std::vector<unsigned char>{5, 6}, std::vector<unsigned char>{7, 8});
  dev::bytes dst;
  Messenger::SetTransactionArray(dst, 0, transactions);
  std::vector<Transaction> transactions1;
  Messenger::GetTransactionArray(dst, 0, transactions1);

  BOOST_REQUIRE(transactions == transactions1);
}

BOOST_AUTO_TEST_CASE(test_transaction_receipt) {
  TransactionReceipt tranReceipt;
  tranReceipt.SetCumGas(1000);
  tranReceipt.SetResult(true);
  tranReceipt.update();

  dev::bytes dst;
  Messenger::SetTransactionReceipt(dst, 0, tranReceipt);
  TransactionReceipt tranReceipt1;
  Messenger::GetTransactionReceipt(dst, 0, tranReceipt1);

  BOOST_REQUIRE(tranReceipt.GetCumGas() == tranReceipt1.GetCumGas());
  BOOST_REQUIRE(tranReceipt.GetString() == tranReceipt1.GetString());
}

BOOST_AUTO_TEST_CASE(test_transaction_with_receipt) {
  boost::multiprecision::uint256_t version = 0;
  boost::multiprecision::uint256_t nonce = 1000;
  Address toAddr;
  PrivKey privKey(
      DataConversion::StringToCharArray(
          "154AF167F12D4C2CEE867186AB03D24FCA2530760DBB1140F122D1E79B020A64"),
      0);
  PubKey pubKey(privKey);
  KeyPair senderKeyPair(privKey, pubKey);
  boost::multiprecision::uint256_t amount = 1000;
  boost::multiprecision::uint256_t gasPrice = 50;
  boost::multiprecision::uint256_t gasLimit = 50;
  std::vector<unsigned char> code;
  std::vector<unsigned char> data;
  Transaction transaction(version, nonce, toAddr, senderKeyPair, amount,
                          gasPrice, gasLimit, code, data);

  TransactionReceipt tranReceipt;
  tranReceipt.SetCumGas(1000);
  tranReceipt.SetResult(true);
  tranReceipt.update();

  TransactionWithReceipt tranWithReceipt(transaction, tranReceipt);

  dev::bytes dst;
  Messenger::SetTransactionWithReceipt(dst, 0, tranWithReceipt);
  TransactionWithReceipt tranWithReceipt1;
  Messenger::GetTransactionWithReceipt(dst, 0, tranWithReceipt1);

  BOOST_REQUIRE(tranWithReceipt.GetTransaction() ==
                tranWithReceipt1.GetTransaction());
  BOOST_REQUIRE(tranWithReceipt.GetTransactionReceipt().GetCumGas() ==
                tranWithReceipt1.GetTransactionReceipt().GetCumGas());
  BOOST_REQUIRE(tranWithReceipt.GetTransactionReceipt().GetString() ==
                tranWithReceipt1.GetTransactionReceipt().GetString());
}

BOOST_AUTO_TEST_SUITE_END()