/**
* Copyright (c) 2018 Zilliqa 
* This is an alpha (internal) release and is not suitable for production.
**/

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#define BOOST_TEST_MODULE trietest

#include <boost/filesystem/path.hpp>
#include <boost/multiprecision/cpp_int.hpp>
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

Transaction constructDummyTxBody(int instanceNum)
{
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
//     std::vector<unsigned char> serializedTxn1;
//     txn1.Serialize(serializedTxn1, 0);

//     RLPStream k;
//     k << 1;

//     transactionsTrie.emplace(&k.out(), serializedTxn1);

//     LOG_GENERAL(INFO, transactionsTrie);
//     LOG_GENERAL(INFO, tm);
//     LOG_GENERAL(INFO, transactionsTrie.root());

//     Transaction txn2 = constructDummyTxBody(2);
//     std::vector<unsigned char> serializedTxn2;
//     txn2.Serialize(serializedTxn2, 0);

//     k << 2;

//     transactionsTrie.emplace(&k.out(), serializedTxn2);

//     LOG_GENERAL(INFO, transactionsTrie);
//     LOG_GENERAL(INFO, tm);
//     LOG_GENERAL(INFO, transactionsTrie.root());

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value from DB not equal to inserted value");
// }

BOOST_AUTO_TEST_CASE(fat_trie2)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    MemoryDB tm;
    GenericTrieDB<MemoryDB> transactionsTrie(&tm);
    transactionsTrie.init();

    Transaction txn1 = constructDummyTxBody(1);
    std::vector<unsigned char> serializedTxn1;
    txn1.Serialize(serializedTxn1, 0);

    RLPStream k;
    k << 1;

    transactionsTrie.emplace(&k.out(), serializedTxn1);

    LOG_GENERAL(INFO, transactionsTrie);
    LOG_GENERAL(INFO, tm);
    LOG_GENERAL(INFO, transactionsTrie.root());

    Transaction txn2 = constructDummyTxBody(2);
    std::vector<unsigned char> serializedTxn2;
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