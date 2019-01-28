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

#include "libCrypto/Sha2.h"
#include "libData/BlockChainData/BlockChain.h"
#include "libData/BlockChainData/BlockLinkChain.h"
#include "libUtils/DataConversion.h"
#include "libUtils/Logger.h"
#include "libTestUtils/TestUtils.h"

#define BOOST_TEST_MODULE blockchaintest
#include <boost/test/included/unit_test.hpp>

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(blockchaintest)

BOOST_AUTO_TEST_CASE(DSBlock_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockLinkChain blc;

  uint64_t index;
  uint64_t dsindex;
  //BlockType blocktype;
  BlockType blocktype =static_cast<BlockType>(TestUtils::RandomIntInRng<unsigned char>(0, 4));
  BlockHash blockhash;

  blc.AddBlockLink(index, dsindex, blocktype, blockhash);
}

BOOST_AUTO_TEST_SUITE_END()
