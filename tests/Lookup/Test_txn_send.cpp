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

#define BOOST_TEST_MODULE test_txn
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "common/Constants.h"
#include "libLookup/Lookup.h"
#include "libMediator/Mediator.h"
#include "libNode/Node.h"
#include "libTestUtils/TestUtils.h"

using namespace std;

bool GenTxns(uint txnSize, map<uint32_t, vector<Transaction>>& mp,
             uint numShards, const Transaction::ContractType& type) {
  mp.clear();
  for (uint i = 0; i < txnSize; i++) {
    const auto tx = TestUtils::GenerateRandomTransaction(1, 1, type);
    const auto& fromAddr = tx.GetSenderAddr();
    auto index = Transaction::GetShardIndex(fromAddr, numShards);
    mp[index].emplace_back(tx);
  }

  return true;
}

void test_transaction(const map<uint32_t, vector<Transaction>>& mp,
                      const unsigned int oldNumShard,
                      const unsigned newShardNum, Lookup& lk) {
  for (const auto& shard : mp) {
    for (const auto& tx : shard.second) {
      lk.AddToTxnShardMap(tx, shard.first);
    }
  }
  lk.RectifyTxnShardMap(oldNumShard, newShardNum);

  for (uint k = 0; k <= newShardNum; k++) {
    const auto& txns = lk.GetTxnFromShardMap(k);
    for (const auto& tx_and_count : txns) {
      const auto& fromShard = tx_and_count.first.GetShardIndex(newShardNum);
      auto index = fromShard;
      if (Transaction::GetTransactionType(tx_and_count.first) ==
          Transaction::CONTRACT_CALL) {
        const auto& toShard = Transaction::GetShardIndex(
            tx_and_count.first.GetToAddr(), newShardNum);
        if (toShard != fromShard) {
          LOG_GENERAL(INFO, "Sent to ds");
          index = newShardNum;
        }
      }

      BOOST_CHECK_MESSAGE(k == index, "The index in map "
                                          << k << " and actual index " << index
                                          << " does not match");
    }
    lk.DeleteTxnShardMap(k);
  }
}

BOOST_AUTO_TEST_SUITE(test_txn)

BOOST_AUTO_TEST_CASE(rectify_txns_perf) {
  INIT_STDOUT_LOGGER();

  const auto txnSize{100};
  pair<uint, uint> rangeOfShards{2, 5};
  const auto txn_types = {Transaction::NON_CONTRACT, Transaction::CONTRACT_CALL,
                          Transaction::CONTRACT_CREATION};

  PairOfKey key;
  Peer peer;
  Mediator md(key, peer);
  Node nd(md, SyncType::NO_SYNC, false);
  Lookup lk(md, SyncType::NO_SYNC);
  md.RegisterColleagues(nullptr, &nd, &lk, nullptr);

  map<uint32_t, vector<Transaction>> txnShardmap;

  for (auto const& type : txn_types) {
    LOG_GENERAL(INFO, "Type: " << type);
    for (unsigned int i = rangeOfShards.first; i <= rangeOfShards.second; i++) {
      for (unsigned int j = rangeOfShards.first; j <= rangeOfShards.second;
           j++) {
        GenTxns(txnSize, txnShardmap, i, type);
        test_transaction(txnShardmap, i, j, lk);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()