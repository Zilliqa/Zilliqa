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

#include <arpa/inet.h>
#include <array>
#include <string>
#include <thread>
#include <vector>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "libData/AccountData/Address.h"
#include "libData/AccountData/Transaction.h"
#include "libData/BlockData/Block.h"
#include "libMessage/Messenger.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/TimeUtils.h"

#define BOOST_TEST_MODULE lookupnodedsblocktest
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

#include "common/BaseType.h"

using namespace std;
using namespace boost::multiprecision;

BOOST_AUTO_TEST_SUITE(lookupnodedsblocktest)

BOOST_AUTO_TEST_CASE(testDSBlockStoring) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  uint32_t listen_port = 5000;
  struct in_addr ip_addr {};
  inet_pton(AF_INET, "127.0.0.1", &ip_addr);
  Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

  bytes dsblockmsg = {MessageType::NODE, NodeInstructionType::DSBLOCK};
  unsigned int curr_offset = MessageOffset::BODY;

  BlockHash prevHash1;

  for (unsigned int i = 0; i < prevHash1.asArray().size(); i++) {
    prevHash1.asArray().at(i) = i + 1;
  }

  PairOfKey pubKey1 = Schnorr::GenKeyPair();

  std::map<PubKey, Peer> powDSWinners;
  std::vector<PubKey> removeDSNodePubkeys;
  DSBlock dsblock(
      DSBlockHeader(50, 20, pubKey1.second, 0, 0, 0, SWInfo(), powDSWinners,
                    removeDSNodePubkeys, DSBlockHashSet(), DSBLOCK_VERSION,
                    CommitteeHash(), BlockHash()),
      CoSignatures());

  curr_offset += dsblock.Serialize(dsblockmsg, curr_offset);

  dsblockmsg.resize(curr_offset + 32);
  Serializable::SetNumber<uint256_t>(dsblockmsg, curr_offset, 0, UINT256_SIZE);
  curr_offset += UINT256_SIZE;

  struct sockaddr_in localhost {};
  inet_pton(AF_INET, "127.0.0.1", &localhost.sin_addr);

  dsblockmsg.resize(curr_offset + 16);
  Serializable::SetNumber<uint128_t>(dsblockmsg, curr_offset,
                                     (uint128_t)localhost.sin_addr.s_addr,
                                     UINT128_SIZE);
  curr_offset += UINT128_SIZE;

  dsblockmsg.resize(curr_offset + 4);
  Serializable::SetNumber<uint32_t>(dsblockmsg, curr_offset, (uint32_t)5001, 4);
  curr_offset += 4;

  P2PComm::GetInstance().SendMessage(lookup_node, dsblockmsg);
}

BOOST_AUTO_TEST_CASE(testDSBlockRetrieval) {
  INIT_STDOUT_LOGGER();

  LOG_MARKER();

  long long i = 0;
  for (; i < 1000000000; i++) {
    ;
  }
  LOG_GENERAL(INFO, i);

  uint32_t listen_port = 5000;
  struct in_addr ip_addr {};
  inet_pton(AF_INET, "127.0.0.1", &ip_addr);
  Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

  bytes getDSBlockMessage = {MessageType::LOOKUP,
                             LookupInstructionType::GETDSBLOCKFROMSEED};

  if (!Messenger::SetLookupGetDSBlockFromSeed(
          getDSBlockMessage, MessageOffset::BODY, 0, 1, 5000, false)) {
    LOG_GENERAL(WARNING, "Messenger::SetLookupGetDSBlockFromSeed failed.");
  } else {
    P2PComm::GetInstance().SendMessage(lookup_node, getDSBlockMessage);
  }
}

// BOOST_AUTO_TEST_CASE (testTxBlockRetrieval)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     uint32_t listen_port = 5000;
//     struct in_addr ip_addr;
//     inet_pton(AF_INET, "127.0.0.1", &ip_addr);
//     Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

//     bytes txblockmsg = { MessageType::NODE,
//     NodeInstructionType::FINALBLOCK }; unsigned int curr_offset =
//     MessageOffset::BODY;

//     // 32-byte DS blocknum
//     Serializable::SetNumber<uint128_t>(txblockmsg, curr_offset, 0,
//     sizeof(uint128_t)); curr_offset += sizeof(uint128_t);

//     // 4-byte consensusid
//     Serializable::SetNumber<uint32_t>(txblockmsg, curr_offset, 0,
//     sizeof(uint32_t)); curr_offset += sizeof(uint32_t);

//     // shard-id
//     txblockmsg.resize(curr_offset + 1);
// 	Serializable::SetNumber<uint8_t>(txblockmsg, curr_offset, (uint8_t) 0,
// 1);
//     curr_offset += 1;

//     // std::array<unsigned char, TRAN_HASH_SIZE> emptyHash = {0};

//     PairOfKey pubKey1 = Schnorr::GenKeyPair();

//     TxBlockHeader header(TXBLOCKTYPE::FINAL, BLOCKVERSION::VERSION1, 1, 1,
//     BlockHash(), 0,
//                             get_time_as_int(), TxnHash(), 0, 5,
//                             pubKey1.second, 0, BlockHash());

//     array<unsigned char, BLOCK_SIG_SIZE> emptySig = { 0 };

//     std::vector<TxnHash> tranHashes;

//     for(int i=0; i<5; i++)
//     {
//         tranHashes.emplace_back(TxnHash());
//     }

//     TxBlock txblock(header, emptySig, tranHashes);

//     curr_offset += txblock.Serialize(txblockmsg, curr_offset);

//    	P2PComm::GetInstance().SendMessage(lookup_node, txblockmsg);

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value
// from DB not equal to inserted value");
// }

// BOOST_AUTO_TEST_CASE (testTxBodyRetrieval)
// {
//     INIT_STDOUT_LOGGER();

//     LOG_MARKER();

//     uint32_t listen_port = 5000;
//     struct in_addr ip_addr;
//     inet_pton(AF_INET, "127.0.0.1", &ip_addr);
//     Peer lookup_node((uint128_t)ip_addr.s_addr, listen_port);

//     bytes txbodymsg = { MessageType::NODE,
//     NodeInstructionType::FORWARDTRANSACTION }; unsigned int curr_offset =
//     MessageOffset::BODY;

//     txbodymsg.resize(curr_offset + UINT256_SIZE);
// 	Serializable::SetNumber<uint128_t>(txbodymsg, curr_offset, (uint8_t) 0,
// UINT256_SIZE);
//     curr_offset += UINT256_SIZE;

//     for (unsigned int i = 0; i < 3; i++)
//     {
//     	Address addr;
//     	array<unsigned char, BLOCK_SIG_SIZE> sign;
//     	Transaction transaction(0, i, addr, addr, i+1, sign);
//         transaction.Serialize(txbodymsg, curr_offset);
//         curr_offset += Transaction::GetSerializedSize();
//     }

//    	P2PComm::GetInstance().SendMessage(lookup_node, txbodymsg);

// //    BOOST_CHECK_MESSAGE("vegetable" == "vegetable", "ERROR: return value
// from DB not equal to inserted value");
// }

BOOST_AUTO_TEST_SUITE_END()
