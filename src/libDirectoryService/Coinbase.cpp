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

template <class Container>
bool DirectoryService::SaveCoinbaseCore(const vector<bool>& b1,
                                        const vector<bool>& b2,
                                        const Container& shard,
                                        const uint32_t& shard_id) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbaseCore not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  auto it = m_coinbaseRewardees.find(m_mediator.m_currentEpochNum);
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
      m_coinbaseRewardees[m_mediator.m_currentEpochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
      if (m_mapNodeReputation[pubKey] < MAX_REPUTATION) {
        ++m_mapNodeReputation[pubKey];
      }
    }
    if (b2.at(i)) {
      m_coinbaseRewardees[m_mediator.m_currentEpochNum][shard_id].push_back(
          Account::GetAddressFromPublicKey(pubKey));
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

bool DirectoryService::SaveCoinbase(const vector<bool>& b1,
                                    const vector<bool>& b2,
                                    const int32_t& shard_id) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbase not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();
  if (shard_id == (int32_t)m_shards.size() || shard_id == -1) {
    // DS
    lock(m_mediator.m_mutexDSCommittee, m_mutexCoinbaseRewardees);
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee, adopt_lock);
    lock_guard<mutex> g1(m_mutexCoinbaseRewardees, adopt_lock);
    return SaveCoinbaseCore(b1, b2, *m_mediator.m_DSCommittee, shard_id);
  } else {
    lock_guard<mutex> g(m_mutexCoinbaseRewardees);
    return SaveCoinbaseCore(b1, b2, m_shards.at(shard_id), shard_id);
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

  for (auto const& epochNum : m_coinbaseRewardees) {
    LOG_GENERAL(INFO, "[CNBSE] Rewarding " << epochNum.first << " epoch");

    for (auto const& shardId : epochNum.second) {
      LOG_GENERAL(INFO, "[CNBSE] Rewarding " << shardId.first << " shard");

      for (auto const& addr : shardId.second) {
        if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                addr, genesisAccount, COINBASE_REWARD)) {
          LOG_GENERAL(WARNING, "Could Not reward " << addr);
        }
      }
    }
  }
}
