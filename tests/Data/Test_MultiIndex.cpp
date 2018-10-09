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

#include "libData/DataStructures/MultiIndexContainer.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE multiindextest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;
using namespace boost::multi_index;

BOOST_AUTO_TEST_SUITE(multiindextest)

BOOST_AUTO_TEST_CASE(MultiIndex_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  Address toAddr;

  for (unsigned int i = 0; i < toAddr.asArray().size(); i++) {
    toAddr.asArray().at(i) = i + 4;
  }

  KeyPair sender = Schnorr::GetInstance().GenKeyPair();

  gas_txnid_comp_txns container;

  auto& listIdx = container.get<MULTI_INDEX_KEY::GAS_PRICE>();

  // version, nonce, toAddr, senderKeyPair, amount, gasPrice, gasLimit, code,
  // data
  Transaction tx1(1, 1, toAddr, sender, 50, 5, 5, {}, {}),
      tx2(1, 2, toAddr, sender, 100, 4, 4, {}, {}),
      tx3(1, 3, toAddr, sender, 150, 3, 3, {}, {});

  LOG_GENERAL(INFO, "mark1");

  // container.insert(tx1);
  listIdx.insert(tx1);
  listIdx.insert(tx2);
  listIdx.insert(tx3);

  BOOST_CHECK_MESSAGE(listIdx.size() == 3, "listIdx size doesn't match");

  uint256_t index = 1;

  for (Transaction tx : listIdx) {
    LOG_GENERAL(INFO, "Tx nonce: " << tx.GetNonce());
    BOOST_CHECK_MESSAGE(tx.GetNonce() == index,
                        "transaction got from listIdx is not correctly "
                        "ordered by gasPrice, current nonce: "
                            << tx.GetNonce() << " desired nonce: " << index);
    index++;
  }

  auto& hashIdx = container.get<MULTI_INDEX_KEY::TXN_ID>();
  BOOST_CHECK_MESSAGE(hashIdx.size() == 3, "hashIdx size doesn't match");

  auto it = hashIdx.find(tx1.GetTranID());

  BOOST_CHECK_MESSAGE(hashIdx.end() != it, "txn is not found");

  BOOST_CHECK_MESSAGE(*it == tx1, "txn found in hashIdx is not identical");

  auto& compIdx = container.get<MULTI_INDEX_KEY::PUBKEY_NONCE>();
  auto it2 = compIdx.find(make_tuple(tx2.GetSenderPubKey(), tx2.GetNonce()));
  BOOST_CHECK_MESSAGE(compIdx.end() != it2, "txn is not found");
  BOOST_CHECK_MESSAGE(*it2 == tx2, "txn found in compIdx is not identical");
}

BOOST_AUTO_TEST_SUITE_END()
