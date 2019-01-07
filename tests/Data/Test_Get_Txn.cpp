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
    bytes privKeyBytes;
    if (!DataConversion::HexStrToUint8Vec(i, privKeyBytes)) {
      BOOST_CHECK_MESSAGE(false, "Failed");
    }

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
