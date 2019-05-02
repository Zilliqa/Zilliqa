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

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#define BOOST_TEST_MODULE trietest
#include <boost/filesystem/path.hpp>
#include <boost/test/included/unit_test.hpp>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/json_spirit/JsonSpiritHeaders.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libTestUtils/MemTrie.h"
#include "libTestUtils/TestCommon.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

namespace fs = boost::filesystem;
namespace js = json_spirit;

static unsigned fac(unsigned _i) { return _i > 2 ? _i * fac(_i - 1) : _i; }

BOOST_AUTO_TEST_SUITE(trietest)

Transaction constructDummyTxBody(int instanceNum) {
  Address addr = NullAddress;
  array<unsigned char, BLOCK_SIG_SIZE> sign = {0};
  return Transaction(0, instanceNum, addr, addr, 0, sign);
}

// BOOST_AUTO_TEST_CASE (fat_trie)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     MemoryDB tm;
//     GenericTrieDB<MemoryDB> transactionsTrie(&tm);
//     transactionsTrie.init();

//     Transaction txn1 = constructDummyTxBody(1);
//     bytes serializedTxn1;
//     txn1.Serialize(serializedTxn1, 0);

//     RLPStream k;
//     k << 1;

//     transactionsTrie.emplace(&k.out(), serializedTxn1);

//     LOG_GENERAL(INFO, transactionsTrie);
//     LOG_GENERAL(INFO, tm);
//     LOG_GENERAL(INFO, transactionsTrie.root());

//     Transaction txn2 = constructDummyTxBody(2);
//     bytes serializedTxn2;
//     txn2.Serialize(serializedTxn2, 0);

//     k << 2;

//     transactionsTrie.emplace(&k.out(), serializedTxn2);

//     LOG_GENERAL(INFO, transactionsTrie);
//     LOG_GENERAL(INFO, tm);
//     LOG_GENERAL(INFO, transactionsTrie.root());

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value
// from DB not equal to inserted value");
// }

BOOST_AUTO_TEST_CASE(fat_trie2) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  MemoryDB tm;
  GenericTrieDB<MemoryDB> transactionsTrie(&tm);
  transactionsTrie.init();

  Transaction txn1 = constructDummyTxBody(1);
  bytes serializedTxn1;
  txn1.Serialize(serializedTxn1, 0);

  RLPStream k;
  k << 1;

  transactionsTrie.emplace(&k.out(), serializedTxn1);

  LOG_GENERAL(INFO, transactionsTrie);
  LOG_GENERAL(INFO, tm);
  LOG_GENERAL(INFO, transactionsTrie.root());

  Transaction txn2 = constructDummyTxBody(2);
  bytes serializedTxn2;
  txn2.Serialize(serializedTxn2, 0);

  k << 2;

  transactionsTrie.emplace(&k.out(), serializedTxn2);

  LOG_GENERAL(INFO, transactionsTrie);
  LOG_GENERAL(INFO, tm);
  LOG_GENERAL(INFO, transactionsTrie.root());

  MemoryDB tm2;
  GenericTrieDB<MemoryDB> transactionsTrie2(&tm2);
  transactionsTrie2.init();

  txn1 = constructDummyTxBody(2);
  txn1.Serialize(serializedTxn1, 0);

  k << 2;

  transactionsTrie2.emplace(&k.out(), serializedTxn1);

  LOG_GENERAL(INFO, transactionsTrie2);
  LOG_GENERAL(INFO, tm2);
  LOG_GENERAL(INFO, transactionsTrie2.root());

  txn2 = constructDummyTxBody(1);
  txn2.Serialize(serializedTxn2, 0);

  k << 1;

  transactionsTrie2.emplace(&k.out(), serializedTxn2);

  LOG_GENERAL(INFO, transactionsTrie2);
  LOG_GENERAL(INFO, tm2);
  LOG_GENERAL(INFO, transactionsTrie2.root());

  BOOST_CHECK_MESSAGE(transactionsTrie.root() == transactionsTrie2.root(),
                      "ERROR: ordering affects root value");
}

BOOST_AUTO_TEST_SUITE_END()
