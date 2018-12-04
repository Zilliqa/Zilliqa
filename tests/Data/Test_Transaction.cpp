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
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libValidator/Validator.h"

#define BOOST_TEST_MODULE transactiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

uint32_t GetShardIndex(const Address& fromAddr, unsigned int numShards) {
  uint32_t x = 0;

  if (numShards == 0) {
    return 0;
  }

  // Take the last four bytes of the address
  for (unsigned int i = 0; i < 4; i++) {
    x = (x << 8) | fromAddr.asArray().at(ACC_ADDR_SIZE - 4 + i);
  }

  return x % numShards;
}

BOOST_AUTO_TEST_SUITE(transactiontest)

BOOST_AUTO_TEST_CASE(test1) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Address toAddr;

  Mediator* m = nullptr;
  unique_ptr<ValidatorBase> m_validator = make_unique<Validator>(*m);

  for (unsigned int i = 0; i < toAddr.asArray().size(); i++) {
    toAddr.asArray().at(i) = i + 4;
  }

  Address fromAddr;

  for (unsigned int i = 0; i < fromAddr.asArray().size(); i++) {
    fromAddr.asArray().at(i) = i + 8;
  }

  KeyPair sender = Schnorr::GetInstance().GenKeyPair();
  Address fromCheck = TestUtils::GetAddressFromPubKey(sender.second);
  Signature sig = TestUtils::GetSignature(
      TestUtils::GenerateRandomCharVector(TestUtils::Dist1to99()), sender.first,
      sender.second);

  Transaction tx1(1, 5, toAddr, sender, 55, PRECISION_MIN_VALUE, 22, {}, {});
  Transaction tx2 =
      Transaction(1, 5, toAddr, sender.second, 55, PRECISION_MIN_VALUE, 22, {},
                  {}, sig);  // Coverage increase

  BOOST_CHECK_MESSAGE(tx1.GetSenderAddr() == fromCheck,
                      "Address from public key converted not properly.");

  BOOST_CHECK_MESSAGE(m_validator->VerifyTransaction(tx1),
                      "Signature not verified\n");

  BOOST_CHECK_MESSAGE(0 == tx1.GetShardIndex(fromAddr, 0),
                      "Shard index > 0 when passing zero shards");

  uint32_t shardSize = TestUtils::DistUint32();
  BOOST_CHECK_MESSAGE(GetShardIndex(fromCheck, shardSize) ==
                          tx1.GetShardIndex(fromCheck, shardSize),
                      "Shard index calculation error");

  std::vector<unsigned char> message1;
  tx1.Serialize(message1, 0);

  LOG_PAYLOAD(INFO, "Transaction1 serialized", message1,
              Logger::MAX_BYTES_TO_DISPLAY);

  tx2 = Transaction(message1, 0);

  if (tx1 == tx2) {
    LOG_PAYLOAD(INFO, "SERIALZED", message1, Logger::MAX_BYTES_TO_DISPLAY);
  }
  LOG_GENERAL(INFO, "address 1" << fromCheck.hex());
  std::vector<unsigned char> message2;
  tx2.Serialize(message2, 0);

  LOG_PAYLOAD(INFO, "Transaction2 serialized", message2,
              Logger::MAX_BYTES_TO_DISPLAY);

  const std::array<unsigned char, TRAN_HASH_SIZE>& tranID2 =
      tx2.GetTranID().asArray();
  const uint128_t& version2 = tx2.GetVersion();
  const uint128_t& nonce2 = tx2.GetNonce();
  const Address& toAddr2 = tx2.GetToAddr();
  const PubKey& senderPubKey = tx2.GetSenderPubKey();
  const Address& fromAddr2 = Account::GetAddressFromPublicKey(senderPubKey);
  const uint128_t& amount2 = tx2.GetAmount();
  const uint128_t& gasPrice2 = tx2.GetGasPrice();
  const uint128_t& gasLimit2 = tx2.GetGasLimit();
  const vector<unsigned char>& code2 = tx2.GetCode();
  const vector<unsigned char>& data2 = tx2.GetData();
  Signature sign = TestUtils::GenerateRandomSignature();

  std::vector<unsigned char> byteVec;
  byteVec.resize(TRAN_HASH_SIZE);
  copy(tranID2.begin(), tranID2.end(), byteVec.begin());
  LOG_PAYLOAD(INFO, "Transaction2 tranID", byteVec,
              Logger::MAX_BYTES_TO_DISPLAY);
  LOG_GENERAL(INFO, "Checking Serialization");
  BOOST_CHECK_MESSAGE(tx1 == tx2, "Not serialized properly");

  LOG_GENERAL(INFO, "Transaction2 version: " << version2);
  BOOST_CHECK_MESSAGE(version2 == 1,
                      "expected: " << 1 << " actual: " << version2 << "\n");

  LOG_GENERAL(INFO, "Transaction2 nonce: " << nonce2);
  BOOST_CHECK_MESSAGE(nonce2 == 5,
                      "expected: " << 5 << " actual: " << nonce2 << "\n");

  byteVec.clear();
  byteVec.resize(ACC_ADDR_SIZE);
  copy(toAddr2.begin(), toAddr2.end(), byteVec.begin());
  LOG_PAYLOAD(INFO, "Transaction2 toAddr", byteVec,
              Logger::MAX_BYTES_TO_DISPLAY);
  BOOST_CHECK_MESSAGE(
      byteVec.at(19) == 23,
      "expected: " << 23 << " actual: " << byteVec.at(19) << "\n");

  copy(fromAddr2.begin(), fromAddr2.end(), byteVec.begin());
  LOG_PAYLOAD(INFO, "Transaction2 fromAddr", byteVec,
              Logger::MAX_BYTES_TO_DISPLAY);
  BOOST_CHECK_MESSAGE(fromCheck == fromAddr2, "PubKey not converted properly");

  LOG_GENERAL(INFO, "Transaction2 amount: " << amount2);
  BOOST_CHECK_MESSAGE(
      amount2 == tx1.GetAmount(),
      "expected: " << tx1.GetAmount() << " actual: " << amount2 << "\n");

  LOG_GENERAL(INFO, "Transaction2 gasPrice: " << gasPrice2);
  BOOST_CHECK_MESSAGE(
      gasPrice2 == tx1.GetGasPrice(),
      "expected: " << tx1.GetGasPrice() << " actual: " << gasPrice2 << "\n");

  LOG_GENERAL(INFO, "Transaction2 gasLimit: " << gasLimit2);
  BOOST_CHECK_MESSAGE(
      gasLimit2 == tx1.GetGasLimit(),
      "expected: " << tx1.GetGasLimit() << " actual: " << gasLimit2 << "\n");

  LOG_PAYLOAD(INFO, "Transaction2 code", code2, Logger::MAX_BYTES_TO_DISPLAY);
  BOOST_CHECK_MESSAGE(code2 == tx1.GetCode(), "Code not converted properly");

  LOG_PAYLOAD(INFO, "Transaction2 data", data2, Logger::MAX_BYTES_TO_DISPLAY);
  BOOST_CHECK_MESSAGE(data2 == tx1.GetData(), "Data not converted properly");

  BOOST_CHECK_MESSAGE(m_validator->VerifyTransaction(tx2),
                      "Signature not verified\n");

  tx2.SetSignature(sign);

  BOOST_CHECK_MESSAGE(sign == tx2.GetSignature(),
                      "Signature not converted properly");
}

BOOST_AUTO_TEST_CASE(testOperators) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  TxnHash txH1 = TxnHash();
  TxnHash txH2 = txH1;
  KeyPair kp = TestUtils::GenerateRandomKeyPair();
  Signature sig = TestUtils::GetSignature(
      TestUtils::GenerateRandomCharVector(TestUtils::Dist1to99()), kp.first,
      kp.second);

  Transaction tx1 = Transaction(txH1, TransactionCoreInfo(), sig);
  Transaction tx2 = Transaction(txH1, TransactionCoreInfo(), sig);
  txH2.operator++();
  Transaction tx3 = Transaction(txH2, TransactionCoreInfo(), sig);

  BOOST_CHECK_MESSAGE(tx1 == tx2, "Equality operator failed");
  BOOST_CHECK_MESSAGE(tx3 > tx1, "More-than operator failed");
  BOOST_CHECK_MESSAGE(tx1 < tx3, "Less-than operator failed");
}

BOOST_AUTO_TEST_SUITE_END()
