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
#include <vector>

#include "libData/BlockData/Block.h"
#include "libPersistence/BlockStorage.h"
#include "libPersistence/DB.h"
#include "libUtils/DataConversion.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE persistencetest
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(persistencetest)

BOOST_AUTO_TEST_CASE(testTransaction) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  std::string hash;

  std::cout << "Enter tx hash: ";
  std::cin >> hash;

  TxBodySharedPtr tx;

  BlockStorage::GetBlockStorage().GetTxBody(hash, tx);

  LOG_GENERAL(INFO, "Transaction amount: " << tx->GetAmount());
  LOG_GENERAL(INFO, "Transaction from address: " << tx->GetFromAddr());
  LOG_GENERAL(INFO, "Transaction to address: " << tx->GetToAddr());
  LOG_GENERAL(INFO, "Transaction nonce: " << tx->GetNonce());
}

BOOST_AUTO_TEST_SUITE_END()
