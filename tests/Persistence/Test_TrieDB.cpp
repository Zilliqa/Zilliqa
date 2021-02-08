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

#include <leveldb/db.h>
#include <string>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "libData/AccountData/Account.h"
#include "libData/DataStructures/TraceableDB.h"
#include "libUtils/DataConversion.h"
#include "libUtils/JsonUtils.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/OverlayDB.h"
#pragma GCC diagnostic pop

#include "depends/libTrie/TrieDB.h"

using namespace std;
using namespace dev;

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

template <class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::GenericTrieDB<DB>, KeyType>;

BOOST_AUTO_TEST_SUITE(persistencetest)

dev::h256 root1, root2;
// bytesConstRef k, k1;
string k1, k2;
h256 h;

BOOST_AUTO_TEST_CASE(createTwoTrieOnOneDB) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  dev::OverlayDB m_db("trieDB");
  m_db.ResetDB();

  SecureTrieDB<bytesConstRef, dev::OverlayDB> m_trie1(&m_db);
  m_trie1.init();

  k1 = "TestA";
  dev::RLPStream rlpStream1(2);
  rlpStream1 << "aaa"
             << "AAA";
  m_trie1.insert(k1, rlpStream1.out());
  k2 = "TestB";
  dev::RLPStream rlpStream2(2);
  rlpStream2 << "bbb"
             << "BBB";
  m_trie1.insert(k2, rlpStream2.out());
  m_trie1.db()->commit();
  root1 = m_trie1.root();
  LOG_GENERAL(INFO, "root1 = " << root1);

  for (auto i : m_trie1) {
    dev::RLP rlp(i.second);
    LOG_GENERAL(INFO, "ITERATE k: " << i.first.toString()
                                    << " v: " << rlp[0].toString() << " "
                                    << rlp[1].toString());
  }

  BOOST_CHECK_MESSAGE(m_trie1.contains(k1),
                      "ERROR: Trie1 cannot get the element in Trie1");

  SecureTrieDB<h256, dev::OverlayDB> m_trie2(&m_db);
  m_trie2.init();
  h = dev::h256::random();
  m_trie2.insert(h, string("hhh"));

  m_trie2.db()->commit();
  root2 = m_trie2.root();
  LOG_GENERAL(INFO, "root2 = " << root2);
  LOG_GENERAL(INFO, "h: " << h << " v: " << m_trie2.at(h));

  BOOST_CHECK_MESSAGE(m_trie2.contains(h),
                      "ERROR: Trie2 cannot get the element in Trie2");

  // Test Rollback
  h256 t = dev::h256::random();
  m_trie2.insert(t, string("ttt"));
  BOOST_CHECK_MESSAGE(m_trie2.contains(t),
                      "ERROR: Trie2 cannot get the element not committed");
  BOOST_CHECK_MESSAGE(
      root2 != m_trie2.root(),
      "ERROR, Trie2 still has the same root after insert and before commit");
  m_trie2.db()->rollback();
  BOOST_CHECK_MESSAGE(!m_trie2.contains(t),
                      "ERROR: Trie2 still have the new element after rollback");
  m_trie2.setRoot(root2);
  BOOST_CHECK_MESSAGE(m_trie2.contains(h),
                      "ERROR: Trie2 still cannot get the the old element "
                      "after reset the root to the old one");
}

BOOST_AUTO_TEST_CASE(retrieveDataStoredInTheTwoTrie) {
  dev::OverlayDB m_db("trieDB");
  SecureTrieDB<bytesConstRef, dev::OverlayDB> m_trie3(&m_db);
  SecureTrieDB<h256, dev::OverlayDB> m_trie4(&m_db);
  m_trie3.setRoot(root1);
  m_trie4.setRoot(root2);

  BOOST_CHECK_MESSAGE(m_trie3.contains(k1),
                      "ERROR: Trie3 cannot get the element in Trie1");
  BOOST_CHECK_MESSAGE(m_trie4.contains(h),
                      "ERROR: Trie4 cannot get the element in Trie2");
}

BOOST_AUTO_TEST_CASE(proof) {
  INIT_STDOUT_LOGGER();
  dev::MemoryDB m_db1;
  GenericTrieDB<dev::MemoryDB> m_trie1(&m_db1);
  m_trie1.init();

  Address addr;

  for (unsigned int i = 0; i < 10000; i++) {
    PairOfKey kpair = Schnorr::GenKeyPair();
    addr = Account::GetAddressFromPublicKey(kpair.second);
    m_trie1.insert(DataConversion::StringToCharArray(addr.hex()),
                   DataConversion::StringToCharArray(to_string(i)));
  }

  // PairOfKey kpairA = Schnorr::GenKeyPair();
  // PairOfKey kpairB = Schnorr::GenKeyPair();
  // PairOfKey kpairC = Schnorr::GenKeyPair();

  // Address addrA = Account::GetAddressFromPublicKey(kpairA.second);
  // Address addrB = Account::GetAddressFromPublicKey(kpairB.second);
  // Address addrC = Account::GetAddressFromPublicKey(kpairC.second);

  // LOG_GENERAL(INFO, "root0" << m_trie1.root().hex());

  // LOG_GENERAL(INFO, "marker");

  // string ka = "TestA";
  // dev::RLPStream rlpStream1(2);
  // rlpStream1 << "aaa"
  //            << "AAA";
  // m_trie1.insert(ka, rlpStream1.out());
  // m_trie1.insert(addrA, DataConversion::StringToCharArray("aaa"));
  // LOG_GENERAL(INFO, "rootA" << m_trie1.root().hex());

  // LOG_GENERAL(INFO, "marker");
  // string kb = "TestB";
  // dev::RLPStream rlpStream2(2);
  // rlpStream2 << "bbb"
  //            << "BBB";
  // m_trie1.insert(kb, rlpStream2.out());
  // m_trie1.insert(addrB, DataConversion::StringToCharArray("bbb"));
  // LOG_GENERAL(INFO, "rootB" << m_trie1.root().hex());

  // LOG_GENERAL(INFO, "marker");
  // string kc = "TestC";
  // dev::RLPStream rlpStream3(2);
  // rlpStream3 << "ccc"
  //            << "CCC";
  // m_trie1.insert(kc, rlpStream3.out());
  // m_trie1.insert(addrC, DataConversion::StringToCharArray("ccc"));
  // LOG_GENERAL(INFO, "rootC" << m_trie1.root().hex());
  // LOG_GENERAL(INFO, "marker");

  // auto ka_byte = DataConversion::StringToCharArray(ka);
  // string result = m_trie1.at(addr);
  LOG_GENERAL(INFO, "result: " << m_trie1.at(
                        DataConversion::StringToCharArray(addr.hex())));
  // LOG_GENERAL(INFO, "size: " << ka_result.size());

  std::set<std::string> proof;
  if (m_trie1.getProof(DataConversion::StringToCharArray(addr.hex()), proof)
          .empty()) {
    LOG_GENERAL(INFO, "failed to getproof");
  }

  // LOG_GENERAL(INFO, "marker");

  dev::MemoryDB m_db2;
  size_t size = 0;
  Json::Value j_proof;
  for (const auto& p : proof) {
    m_db2.insert(dev::sha3(p), &p);
    LOG_GENERAL(INFO, "h256: " << dev::sha3(p).hex());
    LOG_GENERAL(INFO, "value: " << p);
    LOG_GENERAL(INFO, "size: " << p.size() << endl);
    size += p.size();
    string hexstr;
    if (!DataConversion::StringToHexStr(p, hexstr)) {
      LOG_GENERAL(INFO, "StringToHexStr failed");
      return;
    }
    j_proof.append(hexstr);
  }
  GenericTrieDB<dev::MemoryDB> m_trie2(&m_db2);
  m_trie2.setRoot(m_trie1.root());
  LOG_GENERAL(INFO, "result: " << m_trie2.at(
                        DataConversion::StringToCharArray(addr.hex())));

  LOG_GENERAL(INFO, "total size: " << size);
  Json::Value j_value;
  j_value["root"] = m_trie2.root().hex();
  j_value["proof"] = j_proof;
  j_value["key"] = addr.hex();
  LOG_GENERAL(INFO, JSONUtils::GetInstance().convertJsontoStr(j_value));
}

/*
  No longer applicable since we introduce TraceableDB
*/
// BOOST_AUTO_TEST_CASE(snapshot) {
//   INIT_STDOUT_LOGGER();
//   dev::OverlayDB m_db1("trieDB1");
//   GenericTrieDB<dev::OverlayDB> m_trie1(&m_db1);
//   m_trie1.init();

//   m_trie1.insert(DataConversion::StringToCharArray("aaa"),
//                  DataConversion::StringToCharArray("111"));
//   m_trie1.db()->commit();
//   dev::h256 root1 = m_trie1.root();
//   LOG_GENERAL(
//       INFO, "1.aaa: " <<
//       m_trie1.at(DataConversion::StringToCharArray("aaa")));
//   m_trie1.insert(DataConversion::StringToCharArray("aaa"),
//                  DataConversion::StringToCharArray("222"));
//   m_trie1.db()->commit();
//   // dev::h256 root2 = m_trie1.root();
//   LOG_GENERAL(
//       INFO, "2.aaa: " <<
//       m_trie1.at(DataConversion::StringToCharArray("aaa")));
//   m_trie1.remove(DataConversion::StringToCharArray("aaa"));
//   m_trie1.db()->commit();
//   dev::h256 root2 = m_trie1.root();
//   LOG_GENERAL(
//       INFO, "3.aaa: " <<
//       m_trie1.at(DataConversion::StringToCharArray("aaa")));

//   GenericTrieDB<dev::OverlayDB> m_trie2(&m_db1);
//   m_trie2.setRoot(root1);
//   LOG_GENERAL(
//       INFO, "4.aaa: " <<
//       m_trie2.at(DataConversion::StringToCharArray("aaa")));

//   GenericTrieDB<dev::OverlayDB> m_trie3(&m_db1);
//   m_trie2.setRoot(root2);
//   LOG_GENERAL(
//       INFO, "5.aaa: " <<
//       m_trie3.at(DataConversion::StringToCharArray("aaa")));
// }

/*
  Only success with following constants.xml settings:
  1. KEEP_HISTORICAL_STATE -> true
  2. NUM_DS_EPOCHS_STATE_HISTORY < 100
*/
/*
BOOST_AUTO_TEST_CASE(traceabledb) {
  INIT_STDOUT_LOGGER();
  TraceableDB db("traceabledb");
  dev::GenericTrieDB<TraceableDB> m_state(&db);
  m_state.init();
  // data writing
  m_state.insert(DataConversion::StringToCharArray("aaa"),
                 DataConversion::StringToCharArray("111"));
  m_state.insert(DataConversion::StringToCharArray("aaa1"),
                 DataConversion::StringToCharArray("111a"));
  // commit
  uint64_t dsBlock = 100;
  m_state.db()->commit(dsBlock);
  // data accessing
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa")) == "111",
      "Unable to fetch state for aaa");
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa1")) == "111a",
      "Unable to fetch state for aaa1");
  dev::h256 root1 = m_state.root();
  // update key
  m_state.insert(DataConversion::StringToCharArray("aaa"),
                 DataConversion::StringToCharArray("222"));
  m_state.insert(DataConversion::StringToCharArray("aaa1"),
                 DataConversion::StringToCharArray("222a"));
  m_state.db()->commit(dsBlock++);
  // data accessing
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa")) == "222",
      "Unable to fetch state for aaa");
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa1")) == "222a",
      "Unable to fetch state for aaa1");
  dev::h256 root2 = m_state.root();

  // check historical state
  LOG_GENERAL(INFO, "wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww");
  m_state.setRoot(root1);
  LOG_GENERAL(INFO, "wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww");
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa")) == "111",
      "Unable to fetch state for aaa");
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa1")) == "111a",
      "Unable to fetch state for aaa1");
  m_state.setRoot(root2);
  // commit until expire
  for (unsigned int i = 0; i < 100; i++) {
    LOG_GENERAL(INFO, "dsBlock: " << dsBlock);
    m_state.db()->commit(dsBlock++);
  }
  // check latest state
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa")) == "222",
      "Unable to fetch state for aaa");
  BOOST_CHECK_MESSAGE(
      m_state.at(DataConversion::StringToCharArray("aaa1")) == "222a",
      "Unable to fetch state for aaa1");
  try {
    m_state.setRoot(root1);
    BOOST_CHECK(false);
  } catch (...) {
    LOG_GENERAL(INFO, "It's normal to fail here")
  }
}
*/

BOOST_AUTO_TEST_SUITE_END()
