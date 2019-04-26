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

using namespace std;

BOOST_AUTO_TEST_SUITE(test_txn)

BOOST_AUTO_TEST_CASE(rectify_perf) {
  INIT_STDOUT_LOGGER();

  PairOfKey key;
  Peer peer;
  Mediator md(key, peer);
  Node nd(md, SyncType::NO_SYNC, false);
  Lookup lk(md, SyncType::NO_SYNC);

  AccountStore::GetInstance().Init();

  nd.AddBalanceToGenesisAccount();
  md.RegisterColleagues(nullptr, &nd, &lk, nullptr);

  const auto txnSize = 10000;

  map<uint32_t, vector<Transaction>> mp;

  pair<uint, uint> rangeOfShards = make_pair(2, 5);

  for (uint i = rangeOfShards.first; i <= rangeOfShards.second; i++) {
    mp.clear();
    if (!lk.GenTxnToSend(txnSize, mp, i)) {
      LOG_GENERAL(WARNING, "Failed to fetch txns");
      return;
    }
    for (uint j = rangeOfShards.first; j <= rangeOfShards.second; j++) {
      for (const auto& shard : mp) {
        for (const auto& tx : shard.second) {
          lk.AddToTxnShardMap(tx, shard.first);
        }
      }
      lk.RectifyTxnSharMap(i, j);

      for (uint k = 0; k <= j; k++) {
        lk.DeleteTxnShardMap(k);
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()