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
#define BOOST_TEST_DYN_LINK
#include <boost/filesystem/path.hpp>
#include <boost/test/unit_test.hpp>

#include "common/Constants.h"
#include "depends/common/CommonIO.h"
#include "depends/common/FixedHash.h"
#include "depends/libDatabase/LevelDB.h"
#include "libUtils/Logger.h"

using namespace std;
using namespace boost::multiprecision;
using namespace dev;

BOOST_AUTO_TEST_SUITE(trietest)

BOOST_AUTO_TEST_CASE(fat_trie) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  LevelDB m_testDB("test");

  m_testDB.Insert((uint256_t)1, "ABB");

  BOOST_CHECK_MESSAGE(m_testDB.Lookup((uint256_t)1) == "ABB",
                      "ERROR: (boost_int, string)");

  m_testDB.Insert((uint256_t)2, "apples");

  BOOST_CHECK_MESSAGE(m_testDB.Lookup((uint256_t)2) == "apples",
                      "ERROR: (boost_int, string)");

  bytes mangoMsg = {'m', 'a', 'n', 'g', 'o'};

  m_testDB.Insert((uint256_t)3, mangoMsg);

  LOG_GENERAL(INFO, m_testDB.Lookup((uint256_t)3));
}

BOOST_AUTO_TEST_CASE(iterator_order) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  LevelDB m_testDB("iterator_order");

  stringstream keystream;
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "3";

  m_testDB.Insert(keystream.str(), {'C'});

  keystream.str(std::string());
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "1";

  m_testDB.Insert(keystream.str(), {'A'});

  keystream.str(std::string());
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "5";

  m_testDB.Insert(keystream.str(), {'E'});

  keystream.str(std::string());
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "2";

  m_testDB.Insert(keystream.str(), {'B'});

  keystream.str(std::string());
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "14";

  m_testDB.Insert(keystream.str(), {'N'});

  keystream.str(std::string());
  keystream.fill('0');
  keystream.width(BLOCK_NUMERIC_DIGITS);
  keystream << "4";

  m_testDB.Insert(keystream.str(), {'D'});

  leveldb::Iterator* iter;
  iter = m_testDB.GetDB()->NewIterator(leveldb::ReadOptions());
  iter->SeekToFirst();

  for (; iter->Valid(); iter->Next()) {
    LOG_GENERAL(INFO, "key: " << iter->key().ToString()
                              << " value: " << iter->value().ToString());
    uint64_t key = stoull(iter->key().ToString());
    LOG_GENERAL(INFO, "num: " << key)
  }
}

BOOST_AUTO_TEST_SUITE_END()
