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
#include <map>
#include <queue>
#include <vector>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"

using namespace std;
using namespace boost::multiprecision;

#if 1  // clark
template <class Container>
bool DirectoryService::SaveCoinbaseCore(const vector<bool>& b1,
                                        const vector<bool>& b2,
                                        const Container& shard,
                                        const uint32_t& shard_id,
                                        const uint64_t& epochNum) {
#else
template <class Container>
bool DirectoryService::SaveCoinbaseCore(const vector<bool>& b1,
                                        const vector<bool>& b2,
                                        const Container& shard,
                                        const uint32_t& shard_id) {
#endif
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbaseCore not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

#if 1  // clark
  auto it = m_coinbaseRewardees.find(epochNum);
#else
  auto it = m_coinbaseRewardees.find(m_mediator.m_currentEpochNum);
#endif
  if (it != m_coinbaseRewardees.end()) {
    if (it->second.find(shard_id) != it->second.end()) {
      LOG_GENERAL(INFO, "Already have cosigs of shard " << shard_id);
      return false;
    }
  }

  if (shard.size() != b1.size()) {
    LOG_GENERAL(WARNING, "B1 and shard members pub keys size do not match "
                             << b1.size() << "  " << shard.size());
    return false;
  }
  if (shard.size() != b2.size()) {
    LOG_GENERAL(WARNING, "B2 and shard members pub keys size do not match "
                             << b2.size() << "  " << shard.size());
    return false;
  }

  unsigned int i = 0;
  constexpr uint16_t MAX_REPUTATION =
      4096;  // This means the max priority is 12. A node need to continually
             // run for 5 days to achieve this reputation.

  for (const auto& kv : shard) {
    const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(kv);
    if (b1.at(i)) {
#if 1  // clark
      m_coinbaseRewardees[epochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
#else
      m_coinbaseRewardees[m_mediator.m_currentEpochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
#endif
      if (m_mapNodeReputation[pubKey] < MAX_REPUTATION) {
        ++m_mapNodeReputation[pubKey];
      }
    }
    if (b2.at(i)) {
#if 1  // clark
      m_coinbaseRewardees[epochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
#else
      m_coinbaseRewardees[m_mediator.m_currentEpochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
#endif
      if (m_mapNodeReputation[pubKey] < MAX_REPUTATION) {
        ++m_mapNodeReputation[pubKey];
      }
    }
    i++;
  }

  /*deque<PubKey> toKeys;

  for (auto it = m_myShardMembers->begin(); it != m_myShardMembers->end();
       ++it)
  {
      toKeys.emplace_back(it->first);
  }

  Reward(lastMicroBlock.GetB1(), lastMicroBlock.GetB2(), toKeys,
         genesisAccount);*/

  return true;
}

#if 1  // clark
bool DirectoryService::SaveCoinbase(const vector<bool>& b1,
                                    const vector<bool>& b2,
                                    const int32_t& shard_id,
                                    const uint64_t& epochNum) {
#else
bool DirectoryService::SaveCoinbase(const vector<bool>& b1,
                                    const vector<bool>& b2,
                                    const int32_t& shard_id) {
#endif
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbase not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();
#if 1  // clark
  LOG_GENERAL(INFO, "Save coin base for shardId: " << shard_id << ", epochNum: "
                                                   << epochNum);
#endif
  if (shard_id == (int32_t)m_shards.size() || shard_id == -1) {
    // DS
    lock(m_mediator.m_mutexDSCommittee, m_mutexCoinbaseRewardees);
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee, adopt_lock);
    lock_guard<mutex> g1(m_mutexCoinbaseRewardees, adopt_lock);
#if 1  // clark
    return SaveCoinbaseCore(b1, b2, *m_mediator.m_DSCommittee, shard_id,
                            epochNum);
#else
    return SaveCoinbaseCore(b1, b2, *m_mediator.m_DSCommittee, shard_id);
#endif
  } else {
    lock_guard<mutex> g(m_mutexCoinbaseRewardees);
#if 1  // clark
    return SaveCoinbaseCore(b1, b2, m_shards.at(shard_id), shard_id, epochNum);
#else
    return SaveCoinbaseCore(b1, b2, m_shards.at(shard_id), shard_id);
#endif
  }
}

void DirectoryService::InitCoinbase() {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::InitCoinbase not "
                "expected to be called from LookUp node.");
    return;
  }

  LOG_MARKER();
  lock_guard<mutex> g(m_mutexCoinbaseRewardees);

  if (m_coinbaseRewardees.size() < NUM_FINAL_BLOCK_PER_POW) {
    LOG_GENERAL(INFO, "[CNBSE]"
                          << "Less then expected rewardees "
                          << m_coinbaseRewardees.size());
  } else if (m_coinbaseRewardees.size() > NUM_FINAL_BLOCK_PER_POW) {
    LOG_GENERAL(INFO, "[CNBSE]"
                          << "More then expected rewardees "
                          << m_coinbaseRewardees.size());
  }

  if (GENESIS_WALLETS.empty()) {
    LOG_GENERAL(WARNING, "no genesis wallet");
    return;
  }

  Address genesisAccount(GENESIS_WALLETS[0]);

  uint256_t sig_count = 0;

  for (auto const& epochNum : m_coinbaseRewardees) {
    for (auto const& shardId : epochNum.second) {
      sig_count += shardId.second.size();
    }
  }
  LOG_GENERAL(INFO, "Total signatures count: " << sig_count);

  uint256_t total_reward;

  if (!SafeMath<uint256_t>::add(COINBASE_REWARD, m_totalTxnFees,
                                total_reward)) {
    LOG_GENERAL(WARNING, "total_reward addition unsafe!");
    return;
  }

  LOG_GENERAL(INFO, "Total reward: " << total_reward);

  uint256_t reward_each;

  if (!SafeMath<uint256_t>::div(total_reward, sig_count, reward_each)) {
    LOG_GENERAL(WARNING, "reward_each dividing unsafe!");
    return;
  }

  LOG_GENERAL(INFO, "Each reward: " << reward_each);

  uint256_t suc_counter = 0;
  for (auto const& epochNum : m_coinbaseRewardees) {
    LOG_GENERAL(INFO, "[CNBSE] Rewarding " << epochNum.first << " epoch");

    for (auto const& shardId : epochNum.second) {
      LOG_GENERAL(INFO, "[CNBSE] Rewarding " << shardId.first << " shard");

      for (auto const& addr : shardId.second) {
        if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                addr, genesisAccount, reward_each)) {
          LOG_GENERAL(WARNING, "Could Not reward " << addr);
        } else {
          suc_counter++;
        }
      }
    }
  }

  uint256_t balance_left = total_reward - (suc_counter * reward_each);

  LOG_GENERAL(INFO, "Left reward: " << balance_left);

  uint16_t lastBlockHash = DataConversion::charArrTo16Bits(
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
  uint16_t shardIndex =
      lastBlockHash % m_coinbaseRewardees[m_mediator.m_currentEpochNum].size();
  uint16_t count = 0;
  for (const auto& shard : m_coinbaseRewardees[m_mediator.m_currentEpochNum]) {
    if (count == shardIndex) {
      uint16_t rdm_index = lastBlockHash % shard.second.size();
      const Address& winnerAddr = shard.second[rdm_index];
      LOG_GENERAL(INFO, "Lucky draw winner: " << winnerAddr);
      if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
              winnerAddr, genesisAccount, balance_left)) {
        LOG_GENERAL(WARNING, "Could not reward lucky draw!");
      }
      return;
    } else {
      ++count;
    }
  }

  LOG_GENERAL(INFO, "Didn't find any miner to reward the lucky draw");
}
