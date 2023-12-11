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

#include <fstream>
#include <map>
#include <vector>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountStore/AccountStore.h"
#include "libData/CoinbaseData/RewardControlContractState.h"
#include "libNetwork/Guard.h"
#include "libUtils/SafeMath.h"

using namespace std;
using namespace boost::multiprecision;

template <typename Container>
bool DirectoryService::SaveCoinbaseCore(const vector<bool>& b1,
                                        const vector<bool>& b2,
                                        const Container& shard,
                                        const int32_t& shard_id,
                                        const uint64_t& epochNum) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbaseCore not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();

  auto it = m_coinbaseRewardees.find(epochNum);
  if (it != m_coinbaseRewardees.end()) {
    if (it->second.find(shard_id) != it->second.end()) {
      LOG_GENERAL(INFO, "Already have cosigs of shard " << shard_id);
      return false;
    }
  }

  unsigned int i = 0;

  for (const auto& kv : shard) {
    const auto& pubKey = std::get<SHARD_NODE_PUBKEY>(kv);
    if (i < b1.size() && b1.at(i)) {
      m_coinbaseRewardees[epochNum][shard_id].push_back(pubKey);
      if (m_mapNodeReputation[pubKey] < MAX_REPUTATION) {
        ++m_mapNodeReputation[pubKey];
      }
    }
    if (i < b2.size() && b2.at(i)) {
      m_coinbaseRewardees[epochNum][shard_id].push_back(pubKey);
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
                                    const int32_t& shard_id,
                                    const uint64_t& epochNum) {
  if (LOOKUP_NODE_MODE) {
    LOG_GENERAL(WARNING,
                "DirectoryService::SaveCoinbase not "
                "expected to be called from LookUp node.");
    return true;
  }

  LOG_MARKER();
  LOG_GENERAL(INFO, "Save coin base for shardId: " << shard_id << ", epochNum: "
                                                   << epochNum);

  if (shard_id == (int32_t)m_shards.size()) {
    LOG_GENERAL(INFO, "Skip the micro block with shardId = shard size.");
    return true;
  }

  if (shard_id == CoinbaseReward::FINALBLOCK_REWARD) {
    // DS
    lock(m_mediator.m_mutexDSCommittee, m_mutexCoinbaseRewardees,
         m_mutexMapNodeReputation);
    lock_guard<mutex> g(m_mediator.m_mutexDSCommittee, adopt_lock);
    lock_guard<mutex> g1(m_mutexCoinbaseRewardees, adopt_lock);
    lock_guard<mutex> g2(m_mutexMapNodeReputation, adopt_lock);
    return SaveCoinbaseCore(b1, b2, *m_mediator.m_DSCommittee, shard_id,
                            epochNum);
  } else {
    lock(m_mutexCoinbaseRewardees, m_mutexMapNodeReputation);
    lock_guard<mutex> g1(m_mutexCoinbaseRewardees, adopt_lock);
    lock_guard<mutex> g2(m_mutexMapNodeReputation, adopt_lock);
    return SaveCoinbaseCore(b1, b2, m_shards, shard_id, epochNum);
  }
}

std::optional<RewardInformation> DirectoryService::GetRewardInformation()
    const {
  uint128_t sig_count = 0;
  uint32_t lookup_count = 0;
  for (auto const& epochNum : m_coinbaseRewardees) {
    for (auto const& shardId : epochNum.second) {
      if (shardId.first == CoinbaseReward::LOOKUP_REWARD) {
        lookup_count += shardId.second.size();
      } else {
        sig_count += shardId.second.size();
      }
    }
  }
  LOG_GENERAL(INFO, "Total signatures count: " << sig_count << " lookup count "
                                               << lookup_count);

  RewardControlContractState parsed_state =
      RewardControlContractState::GetCurrentRewards();
  uint128_t total_reward = parsed_state.coinbase_reward_per_ds;
  LOG_GENERAL(INFO, "Total reward: " << total_reward);

  uint128_t base_reward = 0;

  if (!SafeMath<uint128_t>::mul(
          total_reward, parsed_state.base_reward_in_percent, base_reward)) {
    LOG_GENERAL(WARNING, "base_reward multiplication unsafe!");
    return std::nullopt;
  }
  // @TODO we should really do this division just once - rrw 2023-10-03
  base_reward /= 100 * parsed_state.percent_prec;

  LOG_GENERAL(INFO, "Total base reward: " << base_reward);

  uint128_t base_reward_each = 0;
  uint128_t node_count = m_mediator.m_DSCommittee->size();
  LOG_GENERAL(INFO, "Total num of node: " << node_count);
  if (!SafeMath<uint128_t>::div(base_reward, node_count, base_reward_each)) {
    LOG_GENERAL(WARNING, "base_reward_each dividing unsafe!");
    return std::nullopt;
    ;
  }
  LOG_GENERAL(INFO, "Base reward for each node: " << base_reward_each);

  uint128_t lookupReward = 0;
  if (!SafeMath<uint128_t>::mul(
          total_reward, parsed_state.lookup_reward_in_percent, lookupReward)) {
    LOG_GENERAL(WARNING, "lookupReward multiplication unsafe!");
    return std::nullopt;
    ;
  }
  lookupReward /= 100 * parsed_state.percent_prec;

  uint128_t nodeReward = 0;
  if (!SafeMath<uint128_t>::mul(
          total_reward,
          parsed_state.percent_prec - 3 * parsed_state.base_reward_in_percent,
          nodeReward)) {
    LOG_GENERAL(WARNING, "nodeReward multiplication unsafe!");
    return std::nullopt;
  }

  nodeReward /= 100 * parsed_state.percent_prec;
  uint128_t reward_each = 0;
  uint128_t reward_each_lookup = 0;

  if (!SafeMath<uint128_t>::div(nodeReward, sig_count, reward_each)) {
    LOG_GENERAL(WARNING, "reward_each dividing unsafe!");
    return std::nullopt;
    ;
  }

  if (!SafeMath<uint128_t>::div(lookupReward, lookup_count,
                                reward_each_lookup)) {
    LOG_GENERAL(WARNING, "reward_each_lookup dividing unsafe");
    return std::nullopt;
  }

  LOG_GENERAL(INFO, "Each reward: " << reward_each << " lookup each "
                                    << reward_each_lookup);

  return RewardInformation{
      .base_reward = base_reward,
      .base_each_reward = base_reward_each,
      .each_reward = reward_each,
      .lookup_reward = lookupReward,
      .lookup_each_reward = reward_each_lookup,
      .lookup_count = lookup_count,
      .total_reward = total_reward,
      .sig_count = sig_count,
      .node_count = node_count,
      .node_reward = nodeReward,
      .base_reward_mul_in_millis = parsed_state.base_reward_mul_in_millis,
      .reward_each_mul_in_millis = parsed_state.reward_each_mul_in_millis};
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

  // cleanup - entries from older ds epoch
  if (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() == 0) {
    LOG_GENERAL(WARNING, "Still only have genesis block");
    return;
  }
  uint64_t firstTxEpoch =
      (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() - 1) *
      NUM_FINAL_BLOCK_PER_POW;

  auto it = m_coinbaseRewardees.begin();
  while (it != m_coinbaseRewardees.end()) {
    if (it->first < firstTxEpoch)
      it = m_coinbaseRewardees.erase(it);
    else
      ++it;
  }

  const auto& vecLookup = m_mediator.m_lookup->GetLookupNodesStatic();
  const auto& epochNum = m_mediator.m_currentEpochNum;

  for (const auto& lookupNode : vecLookup) {
    m_coinbaseRewardees[epochNum][CoinbaseReward::LOOKUP_REWARD].push_back(
        lookupNode.first);
  }

  if (m_coinbaseRewardees.size() < NUM_FINAL_BLOCK_PER_POW - 1) {
    LOG_GENERAL(INFO, "[CNBSE]"
                          << "Less then expected epoch rewardees "
                          << m_coinbaseRewardees.size());
  } else if (m_coinbaseRewardees.size() > NUM_FINAL_BLOCK_PER_POW - 1) {
    LOG_GENERAL(INFO, "[CNBSE]"
                          << "More then expected epoch rewardees "
                          << m_coinbaseRewardees.size());
  }

  Address coinbaseAddress = Address();

  const auto rewardInformation = GetRewardInformation();

  if (!rewardInformation) {
    LOG_GENERAL(WARNING, "Calculating reward parameters failed");
    return;
  }

  auto base_reward_each = rewardInformation->base_each_reward;
  auto reward_each = rewardInformation->each_reward;
  auto reward_each_lookup = rewardInformation->lookup_each_reward;

  // Add rewards come from gas fee back to the coinbase account
  if (!AccountStore::GetInstance().IncreaseBalanceTemp(coinbaseAddress,
                                                       m_totalTxnFees)) {
    LOG_GENERAL(WARNING, "IncreaseBalanceTemp for coinbaseAddress failed");
  }

  const auto& myAddr =
      Account::GetAddressFromPublicKey(m_mediator.m_selfKey.second);

  // This list is for lucky draw candidates
  vector<Address> nonGuard;

  // Give the base reward to all DS and shard nodes in the network

  // This list will be used in the cosig reward part to help avoid unnecessary
  // repeated checking of guard list
  unordered_map<PubKey, bool> pubKeyAndIsGuard;

  constexpr auto FILE_PATH = "rewards.txt";
  std::optional<std::ofstream> file;
  if (ENABLE_REWARD_DEBUG_FILE) {
    LOG_GENERAL(INFO, "Writing reward data to rewards.txt");
    file.emplace(std::ofstream());
    file.value().open(FILE_PATH, std::ios::out | std::ios::app);
  }

  const uint128_t ONE_THOUSAND = 1000;
  uint128_t base_reward_each_desharded;

  if (!SafeMath<uint128_t>::mul(base_reward_each,
                                rewardInformation->base_reward_mul_in_millis,
                                base_reward_each_desharded)) {
    LOG_GENERAL(WARNING, "base_reward_desharded multiplication unsafe!");
    return;
  }

  if (!SafeMath<uint128_t>::div(base_reward_each_desharded, ONE_THOUSAND,
                                base_reward_each_desharded)) {
    LOG_GENERAL(WARNING, "base_reward_desharded division unsafe!");
    return;
  }

  // DS nodes

  if (file.has_value()) {
    auto& fileval = file.value();
    fileval << "Starting Base reward section for epoch: "
            << m_mediator.m_currentEpochNum << '\n';
    fileval << "RewardStruct information:" << '\n';
    fileval << "base_reward: " << rewardInformation->base_reward << '\n';
    fileval << "base_each_reward: " << rewardInformation->base_each_reward
            << '\n';
    fileval << "each_reward: " << rewardInformation->each_reward << '\n';
    fileval << "lookup_reward: " << rewardInformation->lookup_reward << '\n';
    fileval << "lookup_each_reward: " << rewardInformation->lookup_each_reward
            << '\n';
    fileval << "lookup_count: " << rewardInformation->lookup_count << '\n';
    fileval << "total_reward: " << rewardInformation->total_reward << '\n';
    fileval << "sig_count: " << rewardInformation->sig_count << '\n';
    fileval << "node_count: " << rewardInformation->node_count << '\n';
    fileval << "node_reward: " << rewardInformation->node_reward << '\n';
  }
  // DS nodes
  LOG_GENERAL(INFO, "[CNBSE] Rewarding base reward to DS nodes...");
  for (const auto& ds : *m_mediator.m_DSCommittee) {
    const auto& pk = ds.first;
    Address addr = Account::GetAddressFromPublicKey(pk);
    if (GUARD_MODE) {
      auto& isGuard = pubKeyAndIsGuard[pk];
      if (Guard::GetInstance().IsNodeInDSGuardList(pk)) {
        isGuard = true;
        if (addr == myAddr) {
          LOG_GENERAL(INFO, "I am a Guard Node, skip coinbase");
        }
        continue;
      }
      isGuard = false;
    }
    nonGuard.emplace_back(addr);

    if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
            addr, coinbaseAddress, base_reward_each_desharded)) {
      LOG_GENERAL(WARNING, "Could not reward base reward  " << addr);
      continue;
    } else {
      if (addr == myAddr) {
        LOG_EPOCH(
            INFO, m_mediator.m_currentEpochNum,
            "[REWARD] Rewarded base reward " << base_reward_each_desharded);
        LOG_STATE("[REWARD][" << setw(15) << left
                              << m_mediator.m_selfPeer.GetPrintableIPAddress()
                              << "][" << m_mediator.m_currentEpochNum << "]["
                              << base_reward_each_desharded << "] base reward");
      }
    }
    if (file.has_value()) {
      file.value() << "[CNBSE] Rewarding account: " << addr.hex()
                   << ", with value: " << base_reward_each_desharded << '\n';
    }
    LOG_GENERAL(WARNING,
                "Rewarding Base address: "
                    << addr.hex() << ", with value: "
                    << base_reward_each_desharded.convert_to<std::string>());
  }

  // Reward based on cosigs

  uint128_t suc_counter = 0;
  uint128_t suc_lookup_counter = 0;

  uint128_t reward_each_desharded;

  if (!SafeMath<uint128_t>::mul(reward_each,
                                rewardInformation->reward_each_mul_in_millis,
                                reward_each_desharded)) {
    LOG_GENERAL(WARNING, "reward_each_desharded multiplication unsafe!");
    return;
  }

  if (!SafeMath<uint128_t>::div(reward_each_desharded, ONE_THOUSAND,
                                reward_each_desharded)) {
    LOG_GENERAL(WARNING, "reward_each_desharded division unsafe!");
    return;
  }

  LOG_GENERAL(
      INFO,
      "[CNBSE] Rewarding cosig rewards to lookup, DS, and shard nodes...");

  if (file.has_value()) {
    file.value() << "Old reward_each is: " << reward_each
                 << ", reward_each_desharded: " << reward_each_desharded
                 << '\n';
    file.value()
        << "[CNBSE] Rewarding cosig rewards to lookup, DS, and shard nodes..."
        << '\n';
  }

  LOG_GENERAL(WARNING, "Rewardees has size: " << m_coinbaseRewardees.size());
  for (const auto& epochNumShardRewardee : m_coinbaseRewardees) {
    const auto& epochNum = epochNumShardRewardee.first;
    const auto& shards = epochNumShardRewardee.second;
    LOG_GENERAL(INFO, "[CNBSE] Rewarding epoch " << epochNum);
    for (const auto& shardIdRewardee : shards) {
      const auto& shardId = shardIdRewardee.first;
      const auto& rewardees = shardIdRewardee.second;
      LOG_GENERAL(INFO, "[CNBSE] Rewarding shard " << shardId);

      // These are in fact the SSNs in disguise - rewards are disbursed to
      // lookups, and then funneled by external scripts back to the SSNs - rrw
      // 2023-10-02
      if (shardId == CoinbaseReward::LOOKUP_REWARD) {
        for (const auto& pk : rewardees) {
          const auto& addr = Account::GetAddressFromPublicKey(pk);
          LOG_GENERAL(WARNING,
                      "Rewarding lookup address: "
                          << addr.hex() << ", with value: "
                          << reward_each_lookup.convert_to<std::string>());
          if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                  addr, coinbaseAddress, reward_each_lookup)) {
            LOG_GENERAL(WARNING, "Could not reward " << addr << " - " << pk);
          } else {
            nonGuard.emplace_back(addr);
            suc_lookup_counter++;
          }
        }
      } else {
        for (const auto& pk : rewardees) {
          if (GUARD_MODE && pubKeyAndIsGuard[pk]) {
            suc_counter++;
          } else {
            const auto& addr = Account::GetAddressFromPublicKey(pk);
            LOG_GENERAL(WARNING,
                        "Rewarding Each address: "
                            << addr.hex() << ", with value: "
                            << reward_each_desharded.convert_to<std::string>());
            if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                    addr, coinbaseAddress, reward_each_desharded)) {
              LOG_GENERAL(WARNING, "Could not reward " << addr << " - " << pk);
            } else {
              if (addr == myAddr) {
                LOG_EPOCH(INFO, m_mediator.m_currentEpochNum,
                          "[REWARD] Rewarded " << reward_each_desharded
                                               << " for blk " << epochNum);
                LOG_STATE("[REWARD]["
                          << setw(15) << left
                          << m_mediator.m_selfPeer.GetPrintableIPAddress()
                          << "][" << m_mediator.m_currentEpochNum << "]["
                          << reward_each_desharded << "] for blk " << epochNum);
              }
              suc_counter++;
            }
            if (file.has_value()) {
              file.value() << "[CNBSE] Rewarding account: " << addr.hex()
                           << ", with value: " << reward_each_desharded << '\n';
            }
          }
        }
      }
    }
  }
  if (file.has_value()) {
    file.value().close();
    file.reset();
  }

  uint128_t balance_left = rewardInformation->total_reward -
                           (suc_counter * reward_each) -
                           (suc_lookup_counter * reward_each_lookup) -
                           (rewardInformation->node_count * base_reward_each);

  LOG_GENERAL(INFO, "Left reward: " << balance_left);

  // LuckyDraw

  uint16_t lastBlockHash = DataConversion::charArrTo16Bits(
      m_mediator.m_txBlockChain.GetLastBlock().GetBlockHash().asBytes());
  DiagnosticDataCoinbase entry = {rewardInformation->node_count,
                                  rewardInformation->sig_count,
                                  rewardInformation->lookup_count,
                                  rewardInformation->total_reward,
                                  rewardInformation->base_reward,
                                  base_reward_each,
                                  rewardInformation->lookup_reward,
                                  reward_each_lookup,
                                  rewardInformation->node_reward,
                                  reward_each,
                                  balance_left,
                                  PubKey(PrivKey()),
                                  Address()};

  if (nonGuard.empty()) {
    LOG_GENERAL(WARNING, "No non-guard found, skip LuckyDraw");
    StoreCoinbaseInDiagnosticDB(entry);
    return;
  }

  uint16_t luckyIndex = lastBlockHash % nonGuard.size();

  auto const& luckyAddr = nonGuard.at(luckyIndex);

  LOG_GENERAL(INFO, "Lucky draw winner: " << luckyAddr);
  if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
          luckyAddr, coinbaseAddress, balance_left)) {
    LOG_GENERAL(WARNING, "Could not reward lucky draw!");
  }

  // Only log reward for my self so can find out the reward of mine in state
  // log
  if (luckyAddr == myAddr) {
    LOG_STATE("[REWARD][" << setw(15) << left
                          << m_mediator.m_selfPeer.GetPrintableIPAddress()
                          << "][" << m_mediator.m_currentEpochNum << "]["
                          << balance_left << "] lucky draw");
  }
  entry.luckyDrawWinnerAddr = luckyAddr;
  StoreCoinbaseInDiagnosticDB(entry);
}

void DirectoryService::StoreCoinbaseInDiagnosticDB(
    const DiagnosticDataCoinbase& entry) {
  bool canPutNewEntry = true;

  // There's no quick way to get the oldest entry in leveldb
  // Hence, we manage deleting old entries here instead
  if ((MAX_ENTRIES_FOR_DIAGNOSTIC_DATA > 0) &&  // If limit is 0, skip deletion
      (BlockStorage::GetBlockStorage().GetDiagnosticDataCoinbaseCount() >=
       MAX_ENTRIES_FOR_DIAGNOSTIC_DATA) &&  // Limit reached
      (m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() >=
       MAX_ENTRIES_FOR_DIAGNOSTIC_DATA)) {  // DS Block number is not below
                                            // limit

    const uint64_t oldBlockNum =
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum() -
        MAX_ENTRIES_FOR_DIAGNOSTIC_DATA;

    canPutNewEntry =
        BlockStorage::GetBlockStorage().DeleteDiagnosticDataCoinbase(
            oldBlockNum);

    if (canPutNewEntry) {
      LOG_GENERAL(INFO,
                  "Deleted old diagnostic data for DS block " << oldBlockNum);
    } else {
      LOG_GENERAL(WARNING, "Failed to delete old diagnostic data for DS block "
                               << oldBlockNum);
    }
  }

  if (canPutNewEntry) {
    BlockStorage::GetBlockStorage().PutDiagnosticDataCoinbase(
        m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum(),
        entry);
  }
}
