/**
* Copyright (c) 2018 Zilliqa
* This source code is being disclosed to you solely for the purpose of your participation in
* testing Zilliqa. You may view, compile and run the code for that purpose and pursuant to
* the protocols and algorithms that are programmed into, and intended by, the code. You may
* not do anything else with the code without express permission from Zilliqa Research Pte. Ltd.,
* including modifying or publishing the code (or any part of it), and developing or forming
* another public or private blockchain network. This source code is provided ‘as is’ and no
* warranties are given as to title or non-infringement, merchantability or fitness for purpose
* and, to the extent permitted by law, all liability for your use of the code is disclaimed.
* Some programs in this code are governed by the GNU General Public License v3.0 (available at
* https://www.gnu.org/licenses/gpl-3.0.en.html) (‘GPLv3’). The programs that are governed by
* GPLv3.0 are those programs that are located in the folders src/depends and tests/depends
* and which include a reference to GPLv3 in their program files.
**/
#ifndef IS_LOOKUP_NODE
#include <map>
#include <queue>
#include <vector>

#include "Node.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"

using namespace std;

void Reward()
{
    if (GENESIS_WALLETS.empty())
    {
        LOG_GENERAL(WARNING, "no genesis wallet");
        return;
    }

    Address genesisAccount(GENESIS_WALLETS[0]);

    for (auto const& epochNum : m_coinbaseRewardees)
    {
        LOG_GENERAL(INFO, "[CNBSE] Rewarding " << epochNum.first << " epoch");

        for (auto const& shardId : epochNum.second)
        {
            LOG_GENERAL(INFO,
                        "[CNBSE] Rewarding " << shardId.first << " shard");

            for (auto const& addr : shardId.second)
            {
                if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                        addr, genesisAccount, COINBASE_REWARD))
                {
                    LOG_GENERAL(WARNING, "Could Not reward " << addr);
                }
            }
        }
    }
    /*auto RewardEveryRound = [&genesisAccount, &toKeys](auto const& bits) {
        for (size_t i = 0; i < bits.size(); i++)
        {
            if (!bits[i])
            {
                continue;
            }

            auto to = Account::GetAddressFromPublicKey(toKeys[i]);
            if (!AccountStore::GetInstance().UpdateCoinbaseTemp(
                    to, genesisAccount, COINBASE_REWARD))
            {
                LOG_GENERAL(WARNING, "Could not reward " << to);
            }
        }
    };

    RewardEveryRound(b1);
    RewardEveryRound(b2);
    */
}

bool DirectoryService::SaveCoinbase(const vector<bool>& b1,
                                    const vector<bool>& b2, uint32_t shard_id)
{

    lock_guard<mutex> g(m_mutexCoinbaseRewardees);

    const map<Peer, Pubkey>& shard;

    if (shard_id == m_shards.size())
    {
        //DS
        shard = m_DSCommittee;
    }
    else
    {
        shard = m_shards.at(shard_id);
    }

    auto it = m_coinbaseRewardees.find(m_currentEpochNum);
    if (it != m_coinbaseRewardees.end())
    {
        if (it->find(shardId) != it->end())
        {
            LOG_GENERAL(INFO, "Already have cosigs of shard " << shard_id);
            return false;
        }
    }

    if (shard.size() != b1.size())
    {
        LOG_GENERAL(WARNING,
                    "B1 and shard members pub keys size do not match "
                        << b1.size() << "  " << shard.size());
        return false;
    }
    if (shard.size() != b2.size())
    {
        LOG_GENERAL(WARNING,
                    "B2 and shard members pub keys size do not match "
                        << b2.size() << "  " << shard.size());
        return false;
    }

    unsigned int i = 0;

    for (const auto& kv : shard)
    {
        if (b1.at(i))
        {
            m_coinbaseRewardees[m_currentEpochNum][shard_id].push_back(
                Account::GetAddressFromPublicKey(kv.first));
        }
        if (b2.at(i))
        {
            m_coinbaseRewardees[m_currentEpochNum][shard_id].push_back(
                Account::GetAddressFromPublicKey(kv.first));
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

void DirectoryService::InitCoinbase()
{
    lock_guard<mutex> g(m_mutexCoinbaseRewardees);

    if (m_coinbaseRewardees.size() < NUM_FINAL_BLOCK_PER_POW)
    {
        LOG_GENERAL(INFO,
                    "[CNBSE]"
                        << "Less then expected rewardees "
                        << m_coinbaseRewardees.size());
    }
    else if (m_coinbaseRewardees > NUM_FINAL_BLOCK_PER_POW)
    {
        LOG_GENERAL(INFO,
                    "[CNBSE]"
                        << "More then expected rewardees "
                        << m_coinbaseRewardees.size());
    }

    Reward();
}
#endif // IS_LOOKUP_NODE
