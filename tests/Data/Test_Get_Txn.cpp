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

#include "libData/AccountData/Account.h"
#include "libUtils/GetTxnFromFile.h"
#include "libUtils/Logger.h"

#define BOOST_TEST_MODULE DispacthTxnTest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;

BOOST_AUTO_TEST_SUITE(DispacthTxnTest)

BOOST_AUTO_TEST_CASE(test1) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  bytes vec;

  for (auto& i : GENESIS_KEYS) {
    auto privKeyBytes{DataConversion::HexStrToUint8Vec(i)};
    auto privKey = PrivKey{privKeyBytes, 0};
    auto pubKey = PubKey{privKey};
    auto addr = Account::GetAddressFromPublicKey(pubKey);

    std::vector<Transaction> txns;
    bool b = GetTxnFromFile::GetFromFile(addr, 1, 9, txns);

    LOG_GENERAL(INFO, "Size: " << txns.size());
    BOOST_CHECK_MESSAGE(b, "Failed");

    for (const auto& tx : txns) {
      LOG_GENERAL(INFO, "Nonce of " << i << " " << tx.GetNonce());
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
