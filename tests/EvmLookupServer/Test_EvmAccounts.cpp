/*
 * Copyright (C) 2022 Zilliqa
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

#define BOOST_TEST_MODULE EvmAccounts
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include "libData/AccountData/AccountStore.h"

BOOST_AUTO_TEST_SUITE(BOOST_TEST_MODULE)

BOOST_AUTO_TEST_CASE(test_account) {
  uint64_t blockNum{};
  unsigned int numShards{};
  bool isDS{};
  const Address accountAddress{"a744160c3De133495aB9F9D77EA54b325b045670"};
  PairOfKey senderKeyPair = Schnorr::GenKeyPair();
  const uint128_t amount{500};
  const uint128_t gasPrice{150};
  const uint64_t gasLimit{100};
  const bytes code{};
  const bytes data{1, 2, 3, 4, 5, 6, 7, 8, 9, 0};

  Transaction transaction{1,      10,       accountAddress, senderKeyPair,
                          amount, gasPrice, gasLimit,       code,
                          data};
  TransactionReceipt receipt{};
  TxnStatus error_code{};

  AccountStore::GetInstance().UpdateAccountsTemp(
      blockNum, numShards, isDS, transaction, receipt, error_code);
}

BOOST_AUTO_TEST_SUITE_END()
