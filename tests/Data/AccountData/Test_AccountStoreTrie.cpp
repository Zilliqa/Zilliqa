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

#define BOOST_TEST_MODULE AccountStoreTrieTest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/AccountStoreSC.h"
#include "libData/AccountData/Address.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/Logger.h"
#include "libUtils/SysCommand.h"

#include "../ScillaTestUtil.h"

BOOST_AUTO_TEST_SUITE(accountStoreTrieTest)

BOOST_AUTO_TEST_CASE(add_transactions_test) {
  AccountStore::GetInstance().Init();

  {
    const std::string address1{"b744160c3de133495ab9f9d77ea54b325b045671"};
    const Address accountAddress1{address1};
    Account account1;
    account1.SetBalance(1'000'000U);
    AccountStore::GetInstance().AddAccount(accountAddress1, account1);
  }

  {
    const std::string address2{"b744160c3de133495ab9f9d77ea54b325b045672"};
    const Address accountAddress2{address2};
    Account account2;
    account2.SetBalance(1'000'000U);
    AccountStore::GetInstance().AddAccount(accountAddress2, account2);

    //AccountStore::GetInstance().UpdateStateTrie(accountAddress2, account2);
  }
  AccountStore::GetInstance().PrintTrie();
}

BOOST_AUTO_TEST_SUITE_END()
