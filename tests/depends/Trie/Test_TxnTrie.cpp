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

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#define BOOST_TEST_MODULE trietest
#include <boost/filesystem/path.hpp>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <boost/multiprecision/cpp_int.hpp>
#pragma GCC diagnostic pop
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
