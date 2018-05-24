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
#include "common/Serializable.h"
#include "libCrypto/Schnorr.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Transaction.h"
#include "libUtils/TxnRootComputation.h"

#include <boost/multiprecision/cpp_int.hpp>
#include <cstdint>
#include <vector>

#define BOOST_TEST_MODULE utils
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(utils)

Transaction createDummyTransaction()
{
    Address toAddr;
    for (unsigned int i = 0; i < toAddr.asArray().size(); i++)
    {
        toAddr.asArray().at(i) = i + 4;
    }

    Transaction tx(1, 5, toAddr, Schnorr::GetInstance().GenKeyPair(), 55, 11,
                   22, {0x33}, {0x44});
    return tx;
}

decltype(auto) generateDummyTransactions(size_t n)
{
    std::unordered_map<TxnHash, Transaction> txns;

    for (auto i = 0u; i != n; i++)
    {
        auto txn = createDummyTransaction();
        txns.insert({txn.GetTranID(), txn});
    }

    return txns;
}

BOOST_AUTO_TEST_CASE(compareAllThreeVersions)
{
    auto txnMap1 = generateDummyTransactions(100);
    auto txnMap2 = generateDummyTransactions(100);

    std::vector<TxnHash> txnHashVec; // join the hashes of two lists;
    std::list<Transaction> txnList1, txnList2;

    for (auto& txnPair : txnMap1)
    {
        txnHashVec.push_back(txnPair.first);
        txnList1.push_back(txnPair.second);
    }

    for (auto& txnPair : txnMap2)
    {
        txnHashVec.push_back(txnPair.first);
        txnList2.push_back(txnPair.second);
    }

    auto hashRoot1 = ComputeTransactionsRoot(txnHashVec);
    auto hashRoot2 = ComputeTransactionsRoot(txnList1, txnList2);
    auto hashRoot3 = ComputeTransactionsRoot(txnMap1, txnMap2);

    BOOST_CHECK_EQUAL(hashRoot1, hashRoot2);
    BOOST_CHECK_EQUAL(hashRoot1, hashRoot3);
}

BOOST_AUTO_TEST_SUITE_END()
