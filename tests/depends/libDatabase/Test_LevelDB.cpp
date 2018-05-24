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
#define BOOST_TEST_DYN_LINK
#include <boost/filesystem/path.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/unit_test.hpp>

#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/LevelDB.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

BOOST_AUTO_TEST_SUITE(trietest)

BOOST_AUTO_TEST_CASE(fat_trie)
{
    INIT_STDOUT_LOGGER();

    LOG_MARKER();

    LevelDB m_testDB("test");

    m_testDB.Insert((boost::multiprecision::uint256_t)1, "ABB");

    BOOST_CHECK_MESSAGE(m_testDB.Lookup((boost::multiprecision::uint256_t)1)
                            == "ABB",
                        "ERROR: (boost_int, string)");

    m_testDB.Insert((boost::multiprecision::uint256_t)2, "apples");

    BOOST_CHECK_MESSAGE(m_testDB.Lookup((boost::multiprecision::uint256_t)2)
                            == "apples",
                        "ERROR: (boost_int, string)");

    std::vector<unsigned char> mangoMsg = {'m', 'a', 'n', 'g', 'o'};

    m_testDB.Insert((boost::multiprecision::uint256_t)3, mangoMsg);

    LOG_GENERAL(INFO, m_testDB.Lookup((boost::multiprecision::uint256_t)3));
}

BOOST_AUTO_TEST_SUITE_END()
