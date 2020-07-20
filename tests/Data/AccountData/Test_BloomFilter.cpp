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

#include <Schnorr.h>
#include <iostream>
#include <string>
#include "libData/AccountData/BloomFilter.h"

#define BOOST_TEST_MODULE testbloomfilter
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/Logger.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(bloomfiltertest)

decltype(auto) GenWithSigning(const PairOfKey& sender,
                              const PairOfKey& receiver, size_t n) {
  LOG_MARKER();
  uint32_t version = 0;
  uint64_t nonce = 0;

  Address toAddr = Account::GetAddressFromPublicKey(receiver.second);

  std::vector<Transaction> txns;
  txns.reserve(n);

  for (auto i = 0u; i < n; i++) {
    uint128_t amount = i;

    Transaction txn(version, nonce, toAddr, sender, amount, PRECISION_MIN_VALUE,
                    22);

    // bytes buf;
    // txn.SerializeWithoutSignature(buf, 0);

    // Signature sig;
    // Schnorr::Sign(buf, fromPrivKey, fromPubKey, sig);

    // bytes sigBuf;
    // sig.Serialize(sigBuf, 0);
    // txn.SetSignature(sigBuf);

    txns.emplace_back(move(txn));
  }

  return txns;
}

BOOST_AUTO_TEST_CASE(bloomfiltersize) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  auto n = 10000u;
  auto m = 2000u;
  auto sender = Schnorr::GenKeyPair();
  auto receiver = Schnorr::GenKeyPair();

  auto n_txns = GenWithSigning(sender, receiver, n);
  auto m_txns = GenWithSigning(receiver, sender, m);

  vector<TxnHash> n_txn_ids(n_txns.size());
  vector<TxnHash> m_txn_ids(m_txns.size());

  unsigned int n_txn_ids_size = 0;

  for (const auto& txn : n_txns) {
    n_txn_ids.emplace_back(txn.GetTranID());
    n_txn_ids_size += TRAN_HASH_SIZE;
  }

  for (const auto& txn : m_txns) {
    m_txn_ids.emplace_back(txn.GetTranID());
  }

  LOG_GENERAL(INFO, "txn_ids_size: " << n_txn_ids_size);

  // compose bloom filter
  BloomParameters params;
  params.projected_element_count = n;
  params.false_positive_probability = 0.0001;
  params.random_seed = 0xA5A5A5A5;
  if (!params) {
    LOG_GENERAL(WARNING, "Set bloom filter parameters failed");
    return;
  }
  params.compute_optimal_parameters();
  BloomFilter filter(params);

  for (const auto& iter : n_txn_ids) {
    filter.insert(iter.hex());
  }

  bytes serialized_bf;

  if (!filter.Serialize(serialized_bf, 0)) {
    LOG_GENERAL(INFO, "bloom_filter::Serialize failed");
    return;
  }

  LOG_GENERAL(INFO, "bloom_size: " << serialized_bf.size());

  auto exist = 0;
  auto notexist = 0;
  for (const auto id : n_txn_ids) {
    if (filter.contains(id.hex())) {
      exist++;
    } else {
      notexist++;
    }
  }

  for (const auto id : m_txn_ids) {
    if (filter.contains(id.hex())) {
      exist++;
    } else {
      notexist++;
    }
  }

  LOG_GENERAL(INFO, "exist: " << exist);
  LOG_GENERAL(INFO, "notexist: " << notexist);
}

BOOST_AUTO_TEST_SUITE_END()
