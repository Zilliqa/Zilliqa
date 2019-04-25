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

#include <array>
#include <chrono>
#include <string>

#define BOOST_TEST_MODULE TriePerformance
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "depends/common/RLP.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libDatabase/OverlayDB.h"
#include "depends/libTrie/TrieDB.h"
#pragma GCC diagnostic pop
#include <time.h>
#include "libData/AccountData/Address.h"
#include "libUtils/Logger.h"

BOOST_AUTO_TEST_SUITE(TriePerformance)

template <class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::HashedGenericTrieDB<DB>, KeyType>;

BOOST_AUTO_TEST_CASE(TestSecureTrieDB) {
  INIT_STDOUT_LOGGER();

  dev::OverlayDB tm("state");
  auto m_state = SecureTrieDB<Address, dev::OverlayDB>{&tm};
  m_state.init();

  auto t_start = std::chrono::high_resolution_clock::now();
  clock_t start = clock();

  for (auto i = 0; i < 10000; i++) {
    uint128_t m_balance{i + 9999998945}, m_nonce{i + 9999998945};
    Address address;

    dev::RLPStream rlpStream(2);
    rlpStream << m_balance << m_nonce;
    m_state.insert(address, &rlpStream.out());

    if (i % 1000 == 0 && i > 0) {
      auto t_end = std::chrono::high_resolution_clock::now();
      LOG_GENERAL(INFO, "Time for "
                            << i / 1000 << "k insertions: "
                            << (std::chrono::duration<double, std::milli>(
                                    t_end - t_start)
                                    .count())
                            << " ms");
    }
  }
  clock_t end = clock();
  float seconds = (float)(end - start) / CLOCKS_PER_SEC;

  LOG_GENERAL(INFO, "CPU time: " << seconds);

  auto t_end = std::chrono::high_resolution_clock::now();
  LOG_GENERAL(INFO,
              "SecureTrie Time for 10k insertions: "
                  << (std::chrono::duration<double, std::milli>(t_end - t_start)
                          .count())
                  << " ms");
}

BOOST_AUTO_TEST_CASE(TestSecureTrieDBWithDifferentAddress) {
  INIT_STDOUT_LOGGER();

  dev::OverlayDB tm("state");
  auto m_state = SecureTrieDB<Address, dev::OverlayDB>{&tm};
  m_state.init();

  auto t_start = std::chrono::high_resolution_clock::now();
  clock_t start = clock();

  for (auto i = 0u; i < 10000; i++) {
    uint128_t m_balance{i + 9999998945}, m_nonce{i + 9999998945};
    Address address{i};

    dev::RLPStream rlpStream(2);
    rlpStream << m_balance << m_nonce;
    m_state.insert(address, &rlpStream.out());

    if (i % 1000 == 0 && i > 0) {
      auto t_end = std::chrono::high_resolution_clock::now();
      LOG_GENERAL(INFO, "Time for "
                            << i / 1000 << "k insertions: "
                            << (std::chrono::duration<double, std::milli>(
                                    t_end - t_start)
                                    .count())
                            << " ms");
    }
  }
  clock_t end = clock();
  float seconds = (float)(end - start) / CLOCKS_PER_SEC;

  LOG_GENERAL(INFO, "CPU Time: " << seconds);

  auto t_end = std::chrono::high_resolution_clock::now();
  LOG_GENERAL(INFO,
              "SecureTrie (different address) Time for 10k insertions: "
                  << (std::chrono::duration<double, std::milli>(t_end - t_start)
                          .count())
                  << " ms");
}

BOOST_AUTO_TEST_CASE(TestMemoryDB) {
  dev::MemoryDB tm;
  dev::GenericTrieDB<dev::MemoryDB> transactionsTrie(
      &tm);  // memoryDB (unordered_map)

  transactionsTrie.init();

  auto t_start = std::chrono::high_resolution_clock::now();

  for (auto i = 0; i < 10000; i++) {
    dev::RLPStream rlpStream;
    rlpStream << i;
    transactionsTrie.insert(&rlpStream.out(), &rlpStream.out());
    if (i % 1000 == 0 && i > 0) {
      auto t_end = std::chrono::high_resolution_clock::now();
      LOG_GENERAL(INFO, "Time for "
                            << i / 1000 << "k insertions: "
                            << (std::chrono::duration<double, std::milli>(
                                    t_end - t_start)
                                    .count())
                            << " ms");
    }
  }

  auto t_end = std::chrono::high_resolution_clock::now();
  LOG_GENERAL(INFO,
              "Memory DB Time for 10k insertions: "
                  << (std::chrono::duration<double, std::milli>(t_end - t_start)
                          .count())
                  << " ms");
}

BOOST_AUTO_TEST_SUITE_END()
