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
#include "libData/AccountData/Address.h"
#include "libUtils/Logger.h"
#include <time.h>

BOOST_AUTO_TEST_SUITE(TriePerformance)

template<class KeyType, class DB>
using SecureTrieDB = dev::SpecificTrieDB<dev::HashedGenericTrieDB<DB>, KeyType>;

BOOST_AUTO_TEST_CASE(TestSecureTrieDB)
{
    INIT_STDOUT_LOGGER();

    dev::OverlayDB tm("state");
    auto m_state = SecureTrieDB<Address, dev::OverlayDB>{&tm};
    m_state.init();

    auto t_start = std::chrono::high_resolution_clock::now();
    clock_t start = clock();

    for (auto i = 0; i < 10000; i++)
    {
        boost::multiprecision::uint256_t m_balance{i + 9999998945},
            m_nonce{i + 9999998945};
        Address address;

        dev::RLPStream rlpStream(2);
        rlpStream << m_balance << m_nonce;
        m_state.insert(address, &rlpStream.out());

        if (i % 1000 == 0 && i > 0)
        {
            auto t_end = std::chrono::high_resolution_clock::now();
            LOG_GENERAL(INFO,
                        "Time for "
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
    LOG_GENERAL(
        INFO,
        "SecureTrie Time for 10k insertions: "
            << (std::chrono::duration<double, std::milli>(t_end - t_start)
                    .count())
            << " ms");
}

BOOST_AUTO_TEST_CASE(TestSecureTrieDBWithDifferentAddress)
{
    INIT_STDOUT_LOGGER();

    dev::OverlayDB tm("state");
    auto m_state = SecureTrieDB<Address, dev::OverlayDB>{&tm};
    m_state.init();

    auto t_start = std::chrono::high_resolution_clock::now();
    clock_t start = clock();

    for (auto i = 0u; i < 10000; i++)
    {
        boost::multiprecision::uint256_t m_balance{i + 9999998945},
            m_nonce{i + 9999998945};
        Address address{i};

        dev::RLPStream rlpStream(2);
        rlpStream << m_balance << m_nonce;
        m_state.insert(address, &rlpStream.out());

        if (i % 1000 == 0 && i > 0)
        {
            auto t_end = std::chrono::high_resolution_clock::now();
            LOG_GENERAL(INFO,
                        "Time for "
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
    LOG_GENERAL(
        INFO,
        "SecureTrie (different address) Time for 10k insertions: "
            << (std::chrono::duration<double, std::milli>(t_end - t_start)
                    .count())
            << " ms");
}

BOOST_AUTO_TEST_CASE(TestMemoryDB)
{
    dev::MemoryDB tm;
    dev::GenericTrieDB<dev::MemoryDB> transactionsTrie(
        &tm); // memoryDB (unordered_map)

    transactionsTrie.init();

    auto t_start = std::chrono::high_resolution_clock::now();

    for (auto i = 0; i < 10000; i++)
    {
        dev::RLPStream rlpStream;
        rlpStream << i;
        transactionsTrie.insert(&rlpStream.out(), &rlpStream.out());
        if (i % 1000 == 0 && i > 0)
        {
            auto t_end = std::chrono::high_resolution_clock::now();
            LOG_GENERAL(INFO,
                        "Time for "
                            << i / 1000 << "k insertions: "
                            << (std::chrono::duration<double, std::milli>(
                                    t_end - t_start)
                                    .count())
                            << " ms");
        }
    }

    auto t_end = std::chrono::high_resolution_clock::now();
    LOG_GENERAL(
        INFO,
        "Memory DB Time for 10k insertions: "
            << (std::chrono::duration<double, std::milli>(t_end - t_start)
                    .count())
            << " ms");
}

BOOST_AUTO_TEST_SUITE_END()
