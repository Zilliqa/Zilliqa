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

#include <algorithm>
#include <cstdlib>
#include <vector>
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/AccountData/TxnOrderVerifier.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE transactiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

using KeyPair = std::pair<PrivKey, PubKey>;

BOOST_AUTO_TEST_SUITE(TxnOrderVerifying)

decltype(auto) GenWithDummyValue(const KeyPair& sender, const KeyPair& receiver,
                                 size_t n) {
  LOG_MARKER();
  std::vector<Transaction> txns;

  // Generate to account
  uint32_t version = 1;
  uint64_t nonce = 0;
  Address toAddr = Account::GetAddressFromPublicKey(receiver.second);
  uint128_t amount = 123;
  uint128_t gasPrice = PRECISION_MIN_VALUE;
  uint64_t gasLimit = 789;

  for (unsigned i = 0; i < n; i++) {
    Transaction txn(version, nonce, toAddr, sender, amount, gasPrice, gasLimit,
                    {}, {});

    // LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(),
    // "Created txns: " << txn.GetTranID())
    // LOG_MESSAGE(txn.GetSerializedSize());

    txns.emplace_back(txn);
    nonce++;
    amount++;
    gasPrice++;
    gasLimit++;
  }

  return txns;
}

BOOST_AUTO_TEST_CASE(GenTxn1000) {
  INIT_STDOUT_LOGGER();
  auto n = 100u;
  auto sender = Schnorr::GetInstance().GenKeyPair();
  auto receiver = Schnorr::GetInstance().GenKeyPair();

  // LOG_GENERAL(INFO, "Generating " << n << " txns with multiple methods");

  // auto t_start = std::chrono::high_resolution_clock::now();
  // auto txns1 = GenWithSigning(sender, receiver, n);
  // auto t_end = std::chrono::high_resolution_clock::now();

  // LOG_GENERAL(
  //     INFO,
  //     (std::chrono::duration<double, std::milli>(t_end - t_start).count())
  //         << " ms");

  // t_start = std::chrono::high_resolution_clock::now();
  // auto txns2 = GenWithoutSigning(sender, receiver, n);
  // t_end = std::chrono::high_resolution_clock::now();

  // LOG_GENERAL(
  //     INFO,
  //     (std::chrono::duration<double, std::milli>(t_end - t_start).count())
  //         << " ms");

  // t_start = std::chrono::high_resolution_clock::now();
  // auto txns3 = GenWithoutSigningAndSerializing(sender, receiver, n);
  // t_end = std::chrono::high_resolution_clock::now();

  // LOG_GENERAL(
  //     INFO,
  //     (std::chrono::duration<double, std::milli>(t_end - t_start).count())
  //         << " ms");

  LOG_GENERAL(INFO, "Generating " << n << " txns with dummy values");

  auto txns = GenWithDummyValue(sender, receiver, n);

  std::vector<TxnHash> local_txnHashes;
  for (const auto& t : txns) {
    local_txnHashes.emplace_back(t.GetTranID());
  }

  std::vector<TxnHash> rcvd_txnHashes_1 = local_txnHashes,
                       rcvd_txnHashes_2 = local_txnHashes,
                       rcvd_txnHashes_3 = local_txnHashes,
                       rcvd_txnHashes_4 = local_txnHashes;

  BOOST_CHECK_EQUAL(
      true, VerifyTxnOrderWTolerance(local_txnHashes, rcvd_txnHashes_1,
                                     TXN_MISORDER_TOLERANCE_IN_PERCENT));

  // Shuffle # tolerance_num txns from the head
  std::random_shuffle(
      rcvd_txnHashes_2.begin(),
      rcvd_txnHashes_2.begin() + TXN_MISORDER_TOLERANCE_IN_PERCENT);

  BOOST_CHECK_EQUAL(
      true, VerifyTxnOrderWTolerance(local_txnHashes, rcvd_txnHashes_2,
                                     TXN_MISORDER_TOLERANCE_IN_PERCENT));

  // Shuffle # tolerance_num txns from the tail
  std::random_shuffle(
      rcvd_txnHashes_3.end() - TXN_MISORDER_TOLERANCE_IN_PERCENT,
      rcvd_txnHashes_3.end());

  BOOST_CHECK_EQUAL(
      true, VerifyTxnOrderWTolerance(local_txnHashes, rcvd_txnHashes_3,
                                     TXN_MISORDER_TOLERANCE_IN_PERCENT));

  // Shuffle the txns totally
  std::random_shuffle(rcvd_txnHashes_4.begin(), rcvd_txnHashes_4.end());

  bool verifyAfterFullyShuffle = VerifyTxnOrderWTolerance(
      local_txnHashes, rcvd_txnHashes_4, TXN_MISORDER_TOLERANCE_IN_PERCENT);
  if (!verifyAfterFullyShuffle) {
    LOG_GENERAL(INFO, "Verification failed as expected after fully shuffled.");
  } else {
    LOG_GENERAL(INFO,
                "Verification succeed surprisingly after fully shuffled! Maybe "
                "not well shuffled");
  }
}

BOOST_AUTO_TEST_SUITE_END()