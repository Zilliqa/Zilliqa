/**
* Copyright (c) 2018 Zilliqa 
* This source code is being disclosed to you solely for the purpose of your participation in 
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to 
* the protocols and algorithms that are programmed into, and intended by, the code. You may 
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd., 
* including modifying or publishing the code (or any part of it), and developing or forming 
* another public or private blockchain network. This source code is provided ‘as is’ and no 
* warranties are given as to title or non-infringement, merchantability or fitness for purpose 
* and, to the extent permitted by law, all liability for your use of the code is disclaimed. 
* Some programs in this code are governed by the GNU General Public License v3.0 (available at 
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by 
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends 
* and which include a reference to GPLv3 in their program files.
**/

#include <leveldb/db.h>
#include <string>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"

using namespace std;
using namespace dev;

#define BOOST_TEST_MODULE persistencetest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

template<class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::HashedGenericTrieDB<DB>, KeyType>;

BOOST_AUTO_TEST_SUITE(persistencetest)

dev::h256 root1, root2;
bytesConstRef k;
h256 h;

BOOST_AUTO_TEST_CASE(createTwoTrieOnOneDB)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    dev::OverlayDB m_db("trieDB");
    m_db.ResetDB();

    SecureTrieDB<bytesConstRef, dev::OverlayDB> m_trie1(&m_db);
    m_trie1.init();
    k = dev::h256::random().ref();
    m_trie1.insert(k, string("aaa"));
    m_trie1.db()->commit();
    root1 = m_trie1.root();
    LOG_GENERAL(INFO, "root1 = " << root1);
    LOG_GENERAL(INFO, "k: " << k << " \nv: " << m_trie1.at(k));

    BOOST_CHECK_MESSAGE(m_trie1.contains(k),
                        "ERROR: Trie1 cannot get the element in Trie1");

    SecureTrieDB<h256, dev::OverlayDB> m_trie2(&m_db);
    m_trie2.init();
    h = dev::h256::random();
    m_trie2.insert(h, string("bbb"));
    m_trie2.db()->commit();
    root2 = m_trie2.root();
    LOG_GENERAL(INFO, "root2 = " << root2);
    LOG_GENERAL(INFO, "h: " << h << " \nv: " << m_trie2.at(h));

    BOOST_CHECK_MESSAGE(m_trie2.contains(h),
                        "ERROR: Trie1 cannot get the element in Trie2");
}

BOOST_AUTO_TEST_CASE(retrieveDataStoredInTheTwoTrie)
{
    dev::OverlayDB m_db("trieDB");
    SecureTrieDB<bytesConstRef, dev::OverlayDB> m_trie3(&m_db);
    SecureTrieDB<h256, dev::OverlayDB> m_trie4(&m_db);
    m_trie3.setRoot(root1);
    m_trie4.setRoot(root2);

    BOOST_CHECK_MESSAGE(m_trie3.contains(k),
                        "ERROR: Trie3 cannot get the element in Trie1");
    BOOST_CHECK_MESSAGE(m_trie4.contains(h),
                        "ERROR: Trie4 cannot get the element in Trie2");
}

BOOST_AUTO_TEST_SUITE_END()