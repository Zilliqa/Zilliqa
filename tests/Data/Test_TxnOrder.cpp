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
#include <algorithm>
#include <cstdlib>
#include <vector>
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

BOOST_AUTO_TEST_SUITE(TxnOrderVerifying)

decltype(auto) GenWithDummyValue(const PairOfKey& sender,
                                 const PairOfKey& receiver, size_t n) {
  LOG_MARKER();
  std::vector<Transaction> txns;

  // Generate to account
  uint32_t version = DataConversion::Pack(CHAIN_ID, 1);
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
  auto sender = Schnorr::GenKeyPair();
  auto receiver = Schnorr::GenKeyPair();

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
  local_txnHashes.reserve(txns.size());
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
  std::random_shuffle(rcvd_txnHashes_2.begin(),
                      rcvd_txnHashes_2.begin() +
                          static_cast<long>(TXN_MISORDER_TOLERANCE_IN_PERCENT *
                                            n / ONE_HUNDRED_PERCENT));

  BOOST_CHECK_EQUAL(
      true, VerifyTxnOrderWTolerance(local_txnHashes, rcvd_txnHashes_2,
                                     TXN_MISORDER_TOLERANCE_IN_PERCENT));

  // Shuffle # tolerance_num txns from the tail
  std::random_shuffle(rcvd_txnHashes_3.end() -
                          static_cast<long>(TXN_MISORDER_TOLERANCE_IN_PERCENT *
                                            n / ONE_HUNDRED_PERCENT),
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
