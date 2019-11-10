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
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE transactiontest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace boost::multiprecision;
using namespace std;

BOOST_AUTO_TEST_SUITE(TransactionPrefillPerformance)

// decltype(auto) GenWithSigning(const PairOfKey& sender, const PairOfKey&
// receiver,
//                               size_t n)
// {
//     LOG_MARKER();
//     unsigned int version = 0;
//     auto nonce = 0;

//     const auto& fromPrivKey = sender.first;
//     const auto& fromPubKey = sender.second;
//     auto toAddr = Account::GetAddressFromPublicKey(receiver.second);

//     std::vector<Transaction> txns;
//     txns.reserve(n);

//     for (auto i = 0u; i < n; i++)
//     {
//         auto amount = i;

//         Transaction txn{version,    nonce,  toAddr,
//                         fromPubKey, amount, {/* empty sig */}};

//         bytes buf;
//         txn.SerializeWithoutSignature(buf, 0);

//         Signature sig;
//         Schnorr::Sign(buf, fromPrivKey, fromPubKey, sig);

//         bytes sigBuf;
//         sig.Serialize(sigBuf, 0);
//         txn.SetSignature(sigBuf);

//         txns.emplace_back(move(txn));
//     }

//     return txns;
// }

// decltype(auto) GenWithoutSigning(const PairOfKey& sender, const PairOfKey&
// receiver,
//                                  size_t n)
// {
//     LOG_MARKER();
//     unsigned int version = 0;
//     auto nonce = 0;

//     // const auto &fromPrivKey = sender.first;
//     const auto& fromPubKey = sender.second;
//     auto toAddr = Account::GetAddressFromPublicKey(receiver.second);

//     std::vector<Transaction> txns;
//     txns.reserve(n);

//     for (auto i = 0u; i < n; i++)
//     {
//         auto amount = i;

//         Transaction txn{version,    nonce,  toAddr,
//                         fromPubKey, amount, {/* empty sig */}};

//         bytes buf;
//         txn.SerializeWithoutSignature(buf, 0);

//         // Signature sig;
//         // Schnorr::Sign(buf, fromPrivKey, fromPubKey, sig);

//         // bytes sigBuf;
//         // sig.Serialize(sigBuf, 0);
//         // txn.SetSignature(sigBuf);

//         txns.emplace_back(move(txn));
//     }

//     return txns;
// }

// decltype(auto) GenWithoutSigningAndSerializing(const PairOfKey& sender,
//                                                const PairOfKey& receiver,
//                                                size_t n)
// {
//     LOG_MARKER();
//     unsigned int version = 0;
//     auto nonce = 0;

//     // const auto &fromPrivKey = sender.first;
//     const auto& fromPubKey = sender.second;
//     auto toAddr = Account::GetAddressFromPublicKey(receiver.second);

//     std::vector<Transaction> txns;
//     txns.reserve(n);

//     for (auto i = 0u; i < n; i++)
//     {
//         auto amount = i;

//         Transaction txn{version,    nonce,  toAddr,
//                         fromPubKey, amount, {/* empty sig */}};

//         // bytes buf;
//         // txn.SerializeWithoutSignature(buf, 0);

//         // Signature sig;
//         // Schnorr::Sign(buf, fromPrivKey, fromPubKey, sig);

//         // bytes sigBuf;
//         // sig.Serialize(sigBuf, 0);
//         // txn.SetSignature(sigBuf);

//         txns.emplace_back(move(txn));
//     }

//     return txns;
// }

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
  auto n = 1000u;
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

  auto t_start = std::chrono::high_resolution_clock::now();
  auto txns4 = GenWithDummyValue(sender, receiver, n);
  auto t_end = std::chrono::high_resolution_clock::now();

  LOG_GENERAL(
      INFO, (std::chrono::duration<double, std::milli>(t_end - t_start).count())
                << " ms");
}

BOOST_AUTO_TEST_SUITE_END()
