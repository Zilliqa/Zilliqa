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
#include <string>

#define BOOST_TEST_MODULE accountstoretest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/AccountStoreSC.h"
#include "libData/AccountData/Address.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

#include "../ScillaTestUtil.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(accountstoretest)

BOOST_AUTO_TEST_CASE(getaccountracecondition) {
  INIT_STDOUT_LOGGER();
  AccountStore::GetInstance().Init();

  PubKey pubKey1 = Schnorr::GenKeyPair().second;
  Address address1 = Account::GetAddressFromPublicKey(pubKey1);

  AccountStore::GetInstance().AddAccount(address1, make_shared<Account>(1, 11));

  // std::mutex a_mutex;

  auto func2 = [this, &address1]() mutable -> void {
    shared_ptr<Account> acc;
    std::unique_lock<std::mutex> g(
        AccountStore::GetInstance().GetAccountWMutex(address1, acc));

    // std::unique_lock<std::mutex> g1(a_mutex);
    sleep(2);
    LOG_GENERAL(INFO, "log2");
    if (acc != nullptr) {
      LOG_GENERAL(INFO, "add balance");
      acc->IncreaseBalance(1);
    }
    LOG_GENERAL(INFO, "log3");
  };
  DetachedFunction(1, func2);
  sleep(1);
  LOG_GENERAL(INFO, "log1");
  // std::unique_lock<std::mutex> g2(a_mutex);
  AccountStore::GetInstance().RemoveAccount(address1);
  LOG_GENERAL(INFO, "log4");
  sleep(3);
  LOG_GENERAL(INFO, "log5");
}

BOOST_AUTO_TEST_SUITE_END()