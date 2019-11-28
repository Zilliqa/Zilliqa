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

#include <vector>

#include <Schnorr.h>
#include "common/Constants.h"
#include "common/MessageNames.h"
#include "common/Serializable.h"
#include "libCrypto/Sha2.h"
#include "libData/BlockChainData/BlockChain.h"
#include "libDirectoryService/DirectoryService.h"
#include "libTestUtils/TestUtils.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/UpgradeManager.h"

#define BOOST_TEST_MODULE coinbase
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace TestUtils;

BOOST_AUTO_TEST_SUITE(test_coinbase)

BOOST_AUTO_TEST_CASE(test_coinbase_correctness) {
  INIT_STDOUT_LOGGER();
  LOG_MARKER();

  Mediator mediator(GenerateRandomKeyPair(), GenerateRandomPeer());
  DirectoryService dummyDS(mediator);
  Node dummyNode(mediator, 0, true);  // Unused in code anyways
  Lookup dummyLookup(mediator, SyncType::NO_SYNC);
  const uint64_t nonce{0};
  auto dummyValidator = make_shared<Validator>(mediator);
  AccountStore::GetInstance().Init();

  AccountStore::GetInstance().AddAccount(Address(),
                                         {TOTAL_COINBASE_REWARD, nonce});
  AccountStore::GetInstance().UpdateStateTrieAll();

  const uint32_t min_ds_size = 600;
  const uint32_t min_num_shards = 5;
  const uint32_t min_lookup_size = 5;

  mediator.RegisterColleagues(&dummyDS, &dummyNode, &dummyLookup,
                              dummyValidator.get());

  auto dummy_shard_size = (DistUint8() % min_num_shards) + min_num_shards;

  auto dummy_ds_size = DistUint8() % min_ds_size + min_ds_size;
  auto dummy_shards = GenerateDequeueOfShard(dummy_shard_size);

  auto dummy_ds_comm = GenerateRandomDSCommittee(dummy_ds_size);

  auto dummy_lookup_num = DistUint8() % min_lookup_size + min_lookup_size;

  LOG_GENERAL(INFO, "Shard size: " << dummy_shard_size
                                   << " DS size: " << dummy_ds_size
                                   << " Lookup Num: " << dummy_lookup_num);
  VectorOfNode lookupNodes;

  for (uint i = 0; i < dummy_lookup_num; i++) {
    lookupNodes.emplace_back(GenerateRandomPubKey(), GenerateRandomPeer());
  }

  dummyLookup.SetLookupNodes(lookupNodes);

  vector<bool> b1, b2;

  *mediator.m_DSCommittee = dummy_ds_comm;
  dummyDS.m_shards = dummy_shards;
  DSBlock lastBlock =
      DSBlock(TestUtils::createDSBlockHeader(1), CoSignatures());
  mediator.m_dsBlockChain.AddBlock(lastBlock);

  const uint num_test_epoch = NUM_FINAL_BLOCK_PER_POW;

  for (uint i = 0; i < num_test_epoch; i++) {
    uint j = 0;
    for (const auto& shard : dummy_shards) {
      b1 = GenerateRandomBooleanVector(shard.size());

      b2 = GenerateRandomBooleanVector(shard.size());
      dummyDS.SaveCoinbaseCore(b1, b2, shard, j++, i + 1);
    }

    b1 = GenerateRandomBooleanVector(dummy_ds_comm.size());
    b2 = GenerateRandomBooleanVector(dummy_ds_comm.size());

    dummyDS.SaveCoinbaseCore(b1, b2, dummy_ds_comm,
                             CoinbaseReward::FINALBLOCK_REWARD, i + 1);
  }

  dummyDS.InitCoinbase();

  AccountStore::GetInstance().SerializeDelta();

  AccountStore::GetInstance().CommitTempRevertible();

  uint128_t totalReward = 0;

  auto calcReward = [&totalReward](const auto& shard) {
    for (const auto& shardMember : shard) {
      const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(shardMember);
      const auto& address = Account::GetAddressFromPublicKey(pubKey);
      const Account* account = AccountStore::GetInstance().GetAccount(address);
      BOOST_CHECK_MESSAGE(account != nullptr,
                          "Address: " << address << " PubKey: " << pubKey
                                      << " did not get reward");
      totalReward += account->GetBalance();
    }
  };

  calcReward(dummy_ds_comm);
  for (const auto& shard : dummy_shards) {
    calcReward(shard);
  }

  auto normalReward = totalReward;

  calcReward(lookupNodes);

  auto lookupReward = totalReward - normalReward;

  const auto lookupRewardPerc = (lookupReward * 100) / COINBASE_REWARD_PER_DS;

  const auto percRewarded = (normalReward * 100) / COINBASE_REWARD_PER_DS;

  const auto expectedPercNormal = 100 - LOOKUP_REWARD_IN_PERCENT;

  BOOST_CHECK_MESSAGE(percRewarded - 1 <= expectedPercNormal &&
                          percRewarded + 1 >= expectedPercNormal,
                      "Percent: " << percRewarded << " does not match");

  BOOST_CHECK_MESSAGE(totalReward == COINBASE_REWARD_PER_DS,
                      "total reward wrong");

  BOOST_CHECK_MESSAGE(lookupRewardPerc - 1 <= LOOKUP_REWARD_IN_PERCENT &&
                          lookupRewardPerc + 1 >= LOOKUP_REWARD_IN_PERCENT,
                      "Lookup reward doesn't match");
}

BOOST_AUTO_TEST_SUITE_END()
