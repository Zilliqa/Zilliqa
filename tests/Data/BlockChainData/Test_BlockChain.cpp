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

// Fills vector of BlockLink starting at position _from_ with random number of BlockLink elements in range <_min_,_max_ >. Maximum is size(uint8_t).
void appendBlockLinkAndChain_v(BlockLinkChain& blc, vector<BlockLink>& bl_v, const uint8_t& min, const uint8_t& max) {
  if (max < min) {
    throw "Invalid range, to < from + 1";
  }

  uint8_t lastIndex = bl_v.size();

  for (uint8_t i = lastIndex; i < lastIndex + TestUtils::RandomIntInRng<uint8_t>(min, max); i++) {
    uint64_t index = i;
    uint64_t dsindex = TestUtils::DistUint64();
    BlockType blocktype =static_cast<BlockType>(TestUtils::RandomIntInRng<unsigned char>(0, 4));
    BlockHash blockhash = BlockHash::random();
    bl_v.push_back(make_tuple(BLOCKLINK_VERSION, index, dsindex, blocktype, blockhash));
    BOOST_CHECK_MESSAGE(blc.AddBlockLink(index, dsindex, blocktype, blockhash),
                          "Cannot add block link\n");
  }
}

bool operator== (const BlockLink &c1, const BlockLink &c2) {
  cout << get<0>(c1) << " " << get<1>(c1) << " " << get<2>(c1) << " " << get<3>(c1) << " " << get<4>(c1) << " " << endl;
  cout << get<0>(c2) << " " << get<1>(c2) << " " << get<2>(c2) << " " << get<3>(c2) << " " << get<4>(c2) << " " << endl;
  if (get<0>(c1) == get<0>(c2) && get<1>(c1) == get<1>(c2) && get<2>(c1) == get<2>(c2) && get<3>(c1) == get<3>(c2) && get<4>(c1) == get<4>(c2)) {
    return true;
  }
  return false;
}

BOOST_AUTO_TEST_CASE(DSBlock_test) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  BlockLinkChain blc;

  vector<BlockLink> blTest_v;

  // Add first BlockLink with index higher than 0
  BOOST_CHECK_MESSAGE(blc.AddBlockLink(1, 1, DS, BlockHash::random()) == false,
                            "Can add first BlockLink with index greater than zero\n");

  appendBlockLinkAndChain_v(blc, blTest_v, 1, BLOCKCHAIN_SIZE);

  // Get and compare added random BlockLink
  uint8_t randBlockLinkIndex =  TestUtils::RandomIntInRng<uint8_t>(0, blTest_v.size() - 1);
  std::cout << "blTest_v.size() " << blTest_v.size() << endl;
  std::cout << "randBlockLinkIndex " << randBlockLinkIndex << endl;
  BOOST_CHECK_MESSAGE(blc.GetBlockLink(randBlockLinkIndex) == blTest_v[randBlockLinkIndex],
                              "BlockLink in BlockLinkChain not equals to added one\n");

  // Access out of index
  BlockLink empty_blc;
  BlockLink empty_blc_ = blc.GetBlockLink(blTest_v.size());
  BOOST_CHECK_MESSAGE(empty_blc == empty_blc_,
                            "Empty BlockLinkChain had to be returned when accessed out of index\n");

  // Get BlockLink from persistent storage - BLOCKCHAIN_SIZE exceeded and element on index > BLOCKCHAIN_SIZE will be accessed
  appendBlockLinkAndChain_v(blc, blTest_v, BLOCKCHAIN_SIZE - blTest_v.size(), BLOCKCHAIN_SIZE + 10);
  std::cout << "size:" <<  blTest_v.size() << endl;
  BlockLink persistent_blc = blc.GetBlockLink(1);
  BOOST_CHECK_MESSAGE(persistent_blc == blc.GetBlockLink(1),
                              "Incorrect BlockLink returned from persistent storage.\n");

  // Get latest BlockLink
  BOOST_CHECK_MESSAGE(blc.GetLatestBlockLink() == blTest_v.back(),
                                "Incorrect latest BlockLink returned.\n");

  // Set and get DSComm
  PubKey pk_in = TestUtils::GenerateRandomPubKey();
  Peer peer_in = TestUtils::GenerateRandomPeer();
  std::deque<std::pair<PubKey, Peer>> dq_in = {make_pair(pk_in, peer_in)};
  blc.SetBuiltDSComm(dq_in);
  std::deque<std::pair<PubKey, Peer>> dq_out;

  dq_out = blc.GetBuiltDSComm();
  PubKey pk_out = dq_out.back().first;
  Peer peer_out = dq_out.back().second;
  BOOST_CHECK_MESSAGE(pk_in == pk_out && peer_in == peer_out,
                                  "DSComm obtained not equal to the set one.\n");

  // Add BlockLink with lower index than the latest
  BOOST_CHECK_MESSAGE(blc.AddBlockLink(1, 1, DS, BlockHash::random()) == false,
                            "Can add BlockLink with index lower then the latest index\n");
}

BOOST_AUTO_TEST_SUITE_END()
