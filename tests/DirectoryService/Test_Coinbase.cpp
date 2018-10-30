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

#include "libDirectoryService/DirectoryService.h"
#include "libCrypto/Schnorr.h"
#include "libMediator/Mediator.h"
#include "libNetwork/Peer.h"
#include "testLib/testLibFunctions.h"

#define BOOST_TEST_MODULE Coinbase
#define BOOST_TEST_DYN_LINK
//#include <boost/multiprecision/cpp_int.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/range/irange.hpp>
#include <tuple>

using namespace std;
using namespace boost::multiprecision;

Peer generateRandomPeer() {
  uint128_t ip_address = distUint64();
  uint32_t listen_port_host = distUint32();
  return Peer(ip_address, listen_port_host);
}

using KeyPair = std::pair<PrivKey, PubKey>;

KeyPair generateKeyPair(){
  PrivKey privk;
  return KeyPair(privk, generateRandomPubKey(privk));
}

Mediator* generateHeapMediator(const KeyPair& kp, const Peer& p){
  return new Mediator(kp, p);
}

DirectoryService* generateHeapDirectoryService(Mediator& m){
  return new DirectoryService(m);
}

//Shard = std::vector<std::tuple<PubKey, Peer, uint16_t>>;
Shard generateRandomShard(size_t size){
  Shard s;
  for (size_t i = 1; i <= size; size++)
    (void)i; //  avoid not used varuable
    s.push_back(std::make_tuple(generateRandomPubKey(PrivKey()), generateRandomPeer(), (short unsigned int)distUint16()));
  return s;
}

DequeOfShard generateDequeueOfShard(size_t size){
  DequeOfShard dos;
  for (size_t i = 1; i <= size; size++)
    dos.push_front(generateRandomShard(i));
  return dos;
}

using coinbaseRewardees_t = std::map<uint64_t, std::unordered_map<int32_t, std::vector<Address>>>;


vector<Address> generateAddressVector(size_t size){
  vector<Address> v_a;
  for (size_t addr_i = 1; addr_i <= size; addr_i++){
    v_a.push_back(Address(distUint32()));
  }
  return v_a;
}


/* std::map<uint64_t - echochNum
 * std::unordered_map<int32_t - shardID
 * std::vector<Address>>>; - addresses
 */
coinbaseRewardees_t generateRandomCoinbaseRewardees(){
  coinbaseRewardees_t coinbaseRewardees;
  uint16_t a = randomIntInRng<uint16_t>(1,100);
  a = a + 1;
  uint8_t epoch_size = randomIntInRng<uint8_t>(1,100);
  for (size_t epoch = 1; epoch <= epoch_size; epoch++){
    uint8_t shard_ID_size = randomIntInRng<uint8_t>(1,100);
    std::unordered_map<int32_t, std::vector<Address>> shardID_address_m;
    for (size_t shard_ID = 1; shard_ID <= shard_ID_size; shard_ID++){
      shardID_address_m[(int32_t)shard_ID] = generateAddressVector(shard_ID);
    }
    coinbaseRewardees[(int32_t)epoch] = shardID_address_m;
  }
  return coinbaseRewardees;
}

DirectoryService* ds;
Mediator* m;

KeyPair kp;
Peer peer;

BOOST_AUTO_TEST_SUITE(Coinbase)

BOOST_AUTO_TEST_CASE(init) {
  rng.seed(std::random_device()());
  INIT_STDOUT_LOGGER();
  kp = generateKeyPair();
  peer = generateRandomPeer();
  m = new Mediator(kp, peer);
  ds = new DirectoryService(*m);
}

// save_coinbase_core will be coverred separately
BOOST_AUTO_TEST_CASE(test_save_coinbase) {
  ds->InitCoinbase();
  generateRandomCoinbaseRewardees();
  //ds->m_coinbaseRewardees = generateRandomCoinbaseRewardees();

	DequeOfShard dos = generateDequeueOfShard(2);
	ds->m_shards = dos;

	const vector<bool> b1 = {true,false};
	const vector<bool> b2 = {true, false};
//	uint32_t shard_id = -1;

//  BOOST_CHECK(ds->SaveCoinbase(b1, b2, shard_id));
}

// save_coinbase_core will be coverred separately
BOOST_AUTO_TEST_CASE(test_save_coinbase_core) {

}

BOOST_AUTO_TEST_SUITE_END()
