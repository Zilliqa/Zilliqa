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
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/included/unit_test.hpp>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
// #include "depends/json_spirit/JsonSpiritHeaders.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/MemoryDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
// #include "libTestUtils/MemTrie.h"
// #include "libTestUtils/TestCommon.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

// namespace fs = boost::filesystem;
// namespace js = json_spirit;

// static unsigned fac(unsigned _i) { return _i > 2 ? _i * fac(_i - 1) : _i; }

BOOST_AUTO_TEST_SUITE(trietest)

BOOST_AUTO_TEST_CASE(fat_trie) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  h256 r;
  MemoryDB fm;
  {
    FatGenericTrieDB<MemoryDB> ft(&fm);
    ft.init();
    ft.insert(h256("69", h256::FromHex, h256::AlignRight).ref(),
              h256("414243", h256::FromHex, h256::AlignRight).ref());
    for (auto i : ft) {
      LOG_GENERAL(INFO, i.first << i.second);
      LOG_GENERAL(INFO, "kk");
    }
    //            LOG_GENERAL(INFO, i.first << i.second;
    r = ft.root();
  }
  {
    FatGenericTrieDB<MemoryDB> ft(&fm);
    ft.setRoot(r);
    for (auto i : ft) LOG_GENERAL(INFO, i.first << i.second);
    //            LOG_GENERAL(INFO, i.first << i.second;
  }

  //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value
  //    from DB not equal to inserted value");
}

// TODO: try to enable these commented parts by fixing the compilation errors
// BOOST_AUTO_TEST_CASE(hex_encoded_securetrie_test)
// {
//     fs::path const testPath = test::getTestPath() / fs::path("TrieTests");
//     LOG_GENERAL(INFO, "Testing Secure Trie... " << testPath);
//     string const s = contentsString(
//         testPath / fs::path("hex_encoded_securetrie_test.json"));
//     BOOST_REQUIRE_MESSAGE(s.length() > 0,
//                           "Contents of 'hex_encoded_securetrie_test.json' is
//                           " "empty. Have you cloned the 'tests' repo branch "
//                           "develop?");

//     js::mValue v;
//     js::read_string(s, v);
//     for (auto& i : v.get_obj())
//     {
//         js::mObject& o = i.second.get_obj();
//         vector<pair<string, string>> ss;
//         for (auto i : o["in"].get_obj())
//         {
//             ss.emplace_back(make_pair(i.first, i.second.get_str()));
//             if (!ss.back().first.find("0x"))
//                 ss.back().first =
//                 asString(fromHex(ss.back().first.substr(2)));
//             if (!ss.back().second.find("0x"))
//                 ss.back().second
//                     = asString(fromHex(ss.back().second.substr(2)));
//         }
//         for (unsigned j = 0; j < min(1000000000u, fac((unsigned)ss.size()));
//              ++j)
//         {
//             next_permutation(ss.begin(), ss.end());
//             MemoryDB m;
//             EnforceRefs r(m, true);
//             GenericTrieDB<MemoryDB> t(&m);
//             MemoryDB hm;
//             EnforceRefs hr(hm, true);
//             HashedGenericTrieDB<MemoryDB> ht(&hm);
//             MemoryDB fm;
//             EnforceRefs fr(fm, true);
//             FatGenericTrieDB<MemoryDB> ft(&fm);
//             t.init();
//             ht.init();
//             ft.init();
//             BOOST_REQUIRE(t.check(true));
//             BOOST_REQUIRE(ht.check(true));
//             BOOST_REQUIRE(ft.check(true));
//             for (auto const& k : ss)
//             {
//                 t.emplace(k.first, k.second);
//                 ht.emplace(k.first, k.second);
//                 ft.emplace(k.first, k.second);
//                 BOOST_REQUIRE(t.check(true));
//                 BOOST_REQUIRE(ht.check(true));
//                 BOOST_REQUIRE(ft.check(true));
//                 //                LOG_GENERAL(INFO, "was here inserting");
//                 auto i = ft.begin();
//                 auto j = t.begin();
//                 for (; i != ft.end() && j != t.end(); ++i, ++j)
//                 {
//                     //                    LOG_GENERAL(INFO, "was here
//                     reading"); BOOST_CHECK_EQUAL(i != ft.end(), j ==
//                     t.end()); BOOST_REQUIRE((*i).first.toBytes() ==
//                     (*j).first.toBytes());
//                     BOOST_REQUIRE((*i).second.toBytes()
//                                   == (*j).second.toBytes());
//                 }
//                 BOOST_CHECK_EQUAL(ht.root(), ft.root());
//             }
//             BOOST_REQUIRE(!o["root"].is_null());
//             //            LOG_GENERAL(INFO, "o[root] = " <<
//             o["root"].get_str());
//             //            LOG_GENERAL(INFO,
//             "toHexPrefixed(ht.root().asArray()) = " <<
//             toHexPrefixed(ht.root().asArray()));
//             BOOST_CHECK_EQUAL(o["root"].get_str(),
//                               toHexPrefixed(ht.root().asArray()));
//             BOOST_CHECK_EQUAL(o["root"].get_str(),
//                               toHexPrefixed(ft.root().asArray()));
//         }
//     }
// }

// BOOST_AUTO_TEST_CASE(trie_test_anyorder)
// {
//     fs::path const testPath = test::getTestPath() / fs::path("TrieTests");
//     LOG_GENERAL(INFO, "Testing Secure Trie... " << testPath);
//     string const s = contentsString(testPath /
//     fs::path("trieanyorder.json")); BOOST_REQUIRE_MESSAGE(s.length() > 0,
//                           "Contents of 'trieanyorder.json' is empty. Have you
//                           " "cloned the 'tests' repo branch develop?");

//     js::mValue v;
//     js::read_string(s, v);
//     for (auto& i : v.get_obj())
//     {
//         js::mObject& o = i.second.get_obj();
//         vector<pair<string, string>> ss;
//         for (auto i : o["in"].get_obj())
//         {
//             ss.emplace_back(make_pair(i.first, i.second.get_str()));
//             if (!ss.back().first.find("0x"))
//                 ss.back().first =
//                 asString(fromHex(ss.back().first.substr(2)));
//             if (!ss.back().second.find("0x"))
//                 ss.back().second
//                     = asString(fromHex(ss.back().second.substr(2)));
//         }
//         for (unsigned j = 0; j < min(1000u, fac((unsigned)ss.size())); ++j)
//         {
//             next_permutation(ss.begin(), ss.end());
//             MemoryDB m;
//             EnforceRefs r(m, true);
//             GenericTrieDB<MemoryDB> t(&m);
//             MemoryDB hm;
//             EnforceRefs hr(hm, true);
//             HashedGenericTrieDB<MemoryDB> ht(&hm);
//             MemoryDB fm;
//             EnforceRefs fr(fm, true);
//             FatGenericTrieDB<MemoryDB> ft(&fm);
//             t.init();
//             ht.init();
//             ft.init();
//             BOOST_REQUIRE(t.check(true));
//             BOOST_REQUIRE(ht.check(true));
//             BOOST_REQUIRE(ft.check(true));
//             for (auto const& k : ss)
//             {
//                 t.emplace(k.first, k.second);
//                 ht.emplace(k.first, k.second);
//                 ft.emplace(k.first, k.second);
//                 BOOST_REQUIRE(t.check(true));
//                 BOOST_REQUIRE(ht.check(true));
//                 BOOST_REQUIRE(ft.check(true));
//                 auto i = ft.begin();
//                 auto j = t.begin();
//                 for (; i != ft.end() && j != t.end(); ++i, ++j)
//                 {
//                     LOG_GENERAL(INFO, "was here reading");
//                     BOOST_CHECK_EQUAL(i == ft.end(), j == t.end());
//                     BOOST_REQUIRE((*i).first.toBytes() ==
//                     (*j).first.toBytes());
//                     BOOST_REQUIRE((*i).second.toBytes()
//                                   == (*j).second.toBytes());
//                 }
//                 BOOST_CHECK_EQUAL(ht.root(), ft.root());
//             }
//             BOOST_REQUIRE(!o["root"].is_null());
//             BOOST_CHECK_EQUAL(o["root"].get_str(),
//                               toHexPrefixed(t.root().asArray()));
//             BOOST_CHECK_EQUAL(ht.root(), ft.root());
//         }
//     }
// }

// BOOST_AUTO_TEST_CASE(trie_tests_ordered)
// {
//     fs::path const testPath = test::getTestPath() / fs::path("TrieTests");
//     LOG_GENERAL(INFO, "Testing Trie..." << testPath);
//     string const s = contentsString(testPath / fs::path("trietest.json"));
//     BOOST_REQUIRE_MESSAGE(s.length() > 0,
//                           "Contents of 'trietest.json' is empty. Have you "
//                           "cloned the 'tests' repo branch develop?");

//     js::mValue v;
//     js::read_string(s, v);

//     for (auto& i : v.get_obj())
//     {
//         js::mObject& o = i.second.get_obj();
//         vector<pair<string, string>> ss;
//         vector<string> keysToBeDeleted;
//         for (auto& i : o["in"].get_array())
//         {
//             vector<string> values;
//             for (auto& s : i.get_array())
//             {
//                 if (s.type() == json_spirit::str_type)
//                     values.emplace_back(s.get_str());
//                 else if (s.type() == json_spirit::null_type)
//                 {
//                     // mark entry for deletion
//                     values.emplace_back("");
//                     if (!values[0].find("0x"))
//                         values[0] = asString(fromHex(values[0].substr(2)));
//                     keysToBeDeleted.emplace_back(values[0]);
//                 }
//                 else
//                     BOOST_FAIL("Bad type (expected string)");
//             }

//             BOOST_REQUIRE(values.size() == 2);
//             ss.emplace_back(make_pair(values[0], values[1]));
//             if (!ss.back().first.find("0x"))
//                 ss.back().first =
//                 asString(fromHex(ss.back().first.substr(2)));
//             if (!ss.back().second.find("0x"))
//                 ss.back().second
//                     = asString(fromHex(ss.back().second.substr(2)));
//         }

//         MemoryDB m;
//         EnforceRefs r(m, true);
//         GenericTrieDB<MemoryDB> t(&m);
//         MemoryDB hm;
//         EnforceRefs hr(hm, true);
//         HashedGenericTrieDB<MemoryDB> ht(&hm);
//         MemoryDB fm;
//         EnforceRefs fr(fm, true);
//         FatGenericTrieDB<MemoryDB> ft(&fm);
//         t.init();
//         ht.init();
//         ft.init();
//         BOOST_REQUIRE(t.check(true));
//         BOOST_REQUIRE(ht.check(true));
//         BOOST_REQUIRE(ft.check(true));

//         for (auto const& k : ss)
//         {
//             if (find(keysToBeDeleted.begin(), keysToBeDeleted.end(), k.first)
//                     != keysToBeDeleted.end()
//                 && k.second.empty())
//                 t.remove(k.first), ht.remove(k.first), ft.remove(k.first);
//             else
//                 t.emplace(k.first, k.second), ht.emplace(k.first, k.second),
//                     ft.emplace(k.first, k.second);
//             BOOST_REQUIRE(t.check(true));
//             BOOST_REQUIRE(ht.check(true));
//             BOOST_REQUIRE(ft.check(true));
//             auto i = ft.begin();
//             auto j = t.begin();
//             for (; i != ft.end() && j != t.end(); ++i, ++j)
//             {
//                 BOOST_CHECK_EQUAL(i == ft.end(), j == t.end());
//                 BOOST_REQUIRE((*i).first.toBytes() == (*j).first.toBytes());
//                 BOOST_REQUIRE((*i).second.toBytes() ==
//                 (*j).second.toBytes());
//             }
//             BOOST_CHECK_EQUAL(ht.root(), ft.root());
//         }

//         BOOST_REQUIRE(!o["root"].is_null());
//         BOOST_CHECK_EQUAL(o["root"].get_str(),
//                           toHexPrefixed(t.root().asArray()));
//     }
// }

// h256 stringMapHash256(StringMap const& _s)
// {
//     BytesMap bytesMap;
//     for (auto const& _v : _s)
//         bytesMap.emplace(
//             std::make_pair(bytes(_v.first.begin(), _v.first.end()),
//                            bytes(_v.second.begin(), _v.second.end())));
//     return hash256(bytesMap);
// }

// bytes stringMapRlp256(StringMap const& _s)
// {
//     BytesMap bytesMap;
//     for (auto const& _v : _s)
//         bytesMap.emplace(
//             std::make_pair(bytes(_v.first.begin(), _v.first.end()),
//                            bytes(_v.second.begin(), _v.second.end())));
//     return rlp256(bytesMap);
// }

// BOOST_AUTO_TEST_CASE(moreTrieTests)
// {
//     LOG_GENERAL(INFO, "Testing Trie more...");
//     // More tests...
//     {
//         MemoryDB m;
//         GenericTrieDB<MemoryDB> t(&m);
//         t.init(); // initialise as empty tree.
//         LOG_GENERAL(INFO, t);
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO, stringMapHash256(StringMap()));

//         t.emplace(string("test"), string("test"));
//         LOG_GENERAL(INFO, t);
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO, stringMapHash256({{"test", "test"}}));

//         t.emplace(string("te"), string("testy"));
//         LOG_GENERAL(INFO, t);
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO,
//                     stringMapHash256({{"test", "test"}, {"te", "testy"}}));
//         LOG_GENERAL(INFO, t.at(string("test")));
//         LOG_GENERAL(INFO, t.at(string("te")));
//         LOG_GENERAL(INFO, t.at(string("t")));

//         t.remove(string("te"));
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO, stringMapHash256({{"test", "test"}}));

//         t.remove(string("test"));
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO, stringMapHash256(StringMap()));
//     }
//     {
//         MemoryDB m;
//         GenericTrieDB<MemoryDB> t(&m);
//         t.init(); // initialise as empty tree.
//         t.emplace(string("a"), string("A"));
//         t.emplace(string("b"), string("B"));
//         LOG_GENERAL(INFO, t);
//         LOG_GENERAL(INFO, m);
//         LOG_GENERAL(INFO, t.root());
//         LOG_GENERAL(INFO, stringMapHash256({{"b", "B"}, {"a", "A"}}));
//         bytes r(stringMapRlp256({{"b", "B"}, {"a", "A"}}));
//         LOG_GENERAL(INFO, RLP(r));
//     }
//     {
//         MemTrie t;
//         t.emplace("dog", "puppy");
//         LOG_GENERAL(INFO, hex << t.hash256());
//         bytes r(t.rlp());
//         LOG_GENERAL(INFO, RLP(r));
//     }
//     {
//         MemTrie t;
//         t.emplace("bed", "d");
//         t.emplace("be", "e");
//         LOG_GENERAL(INFO, hex << t.hash256());
//         bytes r(t.rlp());
//         LOG_GENERAL(INFO, RLP(r));
//     }
//     {
//         LOG_GENERAL(
//             INFO,
//             hex << stringMapHash256({{"dog", "puppy"}, {"doe",
//             "reindeer"}}));
//         MemTrie t;
//         t.emplace("dog", "puppy");
//         t.emplace("doe", "reindeer");
//         LOG_GENERAL(INFO, hex << t.hash256());
//         bytes r(t.rlp());
//         LOG_GENERAL(INFO, RLP(r));
//         LOG_GENERAL(INFO, toHex(t.rlp()));
//     }
//     {
//         MemoryDB m;
//         EnforceRefs r(m, true);
//         GenericTrieDB<MemoryDB> d(&m);
//         d.init(); // initialise as empty tree.
//         MemTrie t;
//         StringMap s;

//         auto add = [&](char const* a, char const* b) {
//             d.emplace(string(a), string(b));
//             t.emplace(a, b);
//             s[a] = b;

//             LOG_GENERAL(INFO, "/n-------------------------------");
//             LOG_GENERAL(INFO, a << " -> " << b);
//             LOG_GENERAL(INFO, d);
//             LOG_GENERAL(INFO, m);
//             LOG_GENERAL(INFO, d.root());
//             LOG_GENERAL(INFO, stringMapHash256(s));

//             BOOST_REQUIRE(d.check(true));
//             BOOST_REQUIRE_EQUAL(t.hash256(), stringMapHash256(s));
//             BOOST_REQUIRE_EQUAL(d.root(), stringMapHash256(s));
//             for (auto const& i : s)
//             {
//                 (void)i;
//                 BOOST_REQUIRE_EQUAL(t.at(i.first), i.second);
//                 BOOST_REQUIRE_EQUAL(d.at(i.first), i.second);
//             }
//         };

//         auto remove = [&](char const* a) {
//             s.erase(a);
//             t.remove(a);
//             d.remove(string(a));

//             LOG_GENERAL(INFO, endl << "-------------------------------");
//             LOG_GENERAL(INFO, "X " << a);
//             LOG_GENERAL(INFO, d);
//             LOG_GENERAL(INFO, m);
//             LOG_GENERAL(INFO, d.root());
//             //            LOG_GENERAL(INFO, hash256(s));

//             BOOST_REQUIRE(d.check(true));
//             BOOST_REQUIRE(t.at(a).empty());
//             BOOST_REQUIRE(d.at(string(a)).empty());
//             BOOST_REQUIRE_EQUAL(t.hash256(), stringMapHash256(s));
//             BOOST_REQUIRE_EQUAL(d.root(), stringMapHash256(s));
//             for (auto const& i : s)
//             {
//                 (void)i;
//                 BOOST_REQUIRE_EQUAL(t.at(i.first), i.second);
//                 BOOST_REQUIRE_EQUAL(d.at(i.first), i.second);
//             }
//         };

//         add("dogglesworth", "cat");
//         add("doe", "reindeer");
//         remove("dogglesworth");
//         add("horse", "stallion");
//         add("do", "verb");
//         add("doge", "coin");
//         remove("horse");
//         remove("do");
//         remove("doge");
//         remove("doe");
//     }
// }

BOOST_AUTO_TEST_CASE(trieLowerBound) {
  LOG_GENERAL(INFO, "Stress-testing Trie.lower_bound...");
  if (0) {
    MemoryDB dm;
    EnforceRefs e(dm, true);
    GenericTrieDB<MemoryDB> d(&dm);
    d.init();  // initialise as empty tree.
    for (int a = 0; a < 20; ++a) {
      StringMap m;
      for (int i = 0; i < 50; ++i) {
        auto k = randomWord();
        auto v = toString(i);
        m[k] = v;
        d.insert(k, v);
      }

      for (auto i : d) {
        auto it = d.lower_bound(i.first);
        for (auto iit = d.begin(); iit != d.end(); ++iit)
          if ((*iit).first.toString() >= i.first.toString()) {
            BOOST_REQUIRE(it == iit);
            break;
          }
      }
      for (unsigned i = 0; i < 100; ++i) {
        auto k = randomWord();
        auto it = d.lower_bound(k);
        for (auto iit = d.begin(); iit != d.end(); ++iit)
          if ((*iit).first.toString() >= k) {
            BOOST_REQUIRE(it == iit);
            break;
          }
      }
    }
  }
}

// BOOST_AUTO_TEST_CASE(trieStess)
// {
//     LOG_GENERAL(INFO, "Stress-testing Trie...");
//     {
//         MemoryDB m;
//         MemoryDB dm;
//         EnforceRefs e(dm, true);
//         GenericTrieDB<MemoryDB> d(&dm);
//         d.init(); // initialise as empty tree.
//         MemTrie t;
//         BOOST_REQUIRE(d.check(true));
//         for (int a = 0; a < 20; ++a)
//         {
//             StringMap m;
//             for (int i = 0; i < 50; ++i)
//             {
//                 auto k = randomWord();
//                 auto v = toString(i);
//                 m[k] = v;
//                 t.emplace(k, v);
//                 d.emplace(k, v);
//                 BOOST_REQUIRE_EQUAL(stringMapHash256(m), t.hash256());
//                 BOOST_REQUIRE_EQUAL(stringMapHash256(m), d.root());
//                 BOOST_REQUIRE(d.check(true));
//             }
//             while (!m.empty())
//             {
//                 auto k = m.begin()->first;
//                 auto v = m.begin()->second;
//                 d.remove(k);
//                 t.remove(k);
//                 m.erase(k);
//                 if (!d.check(true))
//                 {
//                     // cwarn << m;
//                     for (auto i : d)
//                         LOG_GENERAL(INFO,
//                                     i.first.toString() <<
//                                     i.second.toString());

//                     MemoryDB dm2;
//                     EnforceRefs e2(dm2, true);
//                     GenericTrieDB<MemoryDB> d2(&dm2);
//                     d2.init(); // initialise as empty tree.
//                     for (auto i : d)
//                         d2.emplace(i.first, i.second);

//                     LOG_GENERAL(INFO, "Good:" << d2.root());
//                     //					for (auto i:
//                     dm2.get())
//                     //						cwarn <<
//                     i.first
//                     <<
//                     ":
//                     "
//                     << RLP(i.second); d2.debugStructure(cerr); LOG_GENERAL(
//                         INFO,
//                         "Broken:"
//                             << d.root()); // Leaves an extension -> extension
//                             (3c1... -> 742...)
//                     //					for (auto i:
//                     dm.get())
//                     //						cwarn <<
//                     i.first
//                     <<
//                     ":
//                     "
//                     << RLP(i.second); d.debugStructure(cerr);

//                     d2.emplace(k, v);
//                     LOG_GENERAL(INFO, "Pres:" << d2.root());
//                     //					for (auto i:
//                     dm2.get())
//                     //						cwarn <<
//                     i.first
//                     <<
//                     ":
//                     "
//                     << RLP(i.second); d2.debugStructure(cerr); d2.remove(k);

//                     LOG_GENERAL(INFO, "Good?" << d2.root());
//                 }
//                 BOOST_REQUIRE(d.check(true));
//                 BOOST_REQUIRE_EQUAL(stringMapHash256(m), t.hash256());
//                 BOOST_REQUIRE_EQUAL(stringMapHash256(m), d.root());
//             }
//         }
//     }
// }

template <typename Trie>
void perfTestTrie(char const* _name) {
  for (size_t p = 1000; p != 10000;
       p *= 10)  // later make the upper bound 1000000
  {
    MemoryDB dm;
    Trie d(&dm);
    d.init();
    LOG_GENERAL(INFO, "TriePerf " << _name << p);
    std::vector<h256> keys(1000);
    //        Timer t;
    size_t ki = 0;
    for (size_t i = 0; i < p; ++i) {
      auto k = h256::random();
      auto v = toString(i);
      d.insert(k, v);

      if (i % (p / 1000) == 0) keys[ki++] = k;
    }
    //        LOG_GENERAL(INFO, "Insert " << p << "values: " << t.elapsed());
    //        t.restart();
    for (auto k : keys) {
      //            LOG_GENERAL(INFO, "key: " << k);
      d.at(k);
    }
    //        LOG_GENERAL(INFO, "Query 1000 values: " << t.elapsed());
    //        t.restart();
    // size_t i = 0;
    // for (auto it = d.begin(); i < 1000 && it != d.end(); ++it, ++i)
    for (auto it = d.begin(); it != d.end(); ++it) {
      // LOG_GENERAL(INFO, "it: ");
      *it;
    }
    //        LOG_GENERAL(INFO, "Iterate 1000 values: " << t.elapsed());
    //        t.restart();
    for (auto k : keys) {
      d.remove(k);
    }
    //        LOG_GENERAL(INFO, "Remove 1000 values:" << t.elapsed() << "\n");
  }
}

BOOST_AUTO_TEST_CASE(triePerf) {
  //    if (test::Options::get().all)
  //    {
  perfTestTrie<SpecificTrieDB<GenericTrieDB<MemoryDB>, h256>>("GenericTrieDB");
  perfTestTrie<SpecificTrieDB<HashedGenericTrieDB<MemoryDB>, h256>>(
      "HashedGenericTrieDB");
  perfTestTrie<SpecificTrieDB<FatGenericTrieDB<MemoryDB>, h256>>(
      "FatGenericTrieDB");
  //    }
  //    else
  //        clog << "Skipping hive test Crypto/Trie/triePerf. Use --all to run
  //        it.\n";
}

BOOST_AUTO_TEST_SUITE_END()
