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
#include <map>
#include <queue>
#include <vector>

#include "Node.h"
#include "common/Constants.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libMediator/Mediator.h"

using namespace std;

void Reward(const vector<bool>& b1, const vector<bool>& b2,
            deque<PubKey>& toKeys, Address& genesisAccount)
{
    auto RewardEveryRound = [&genesisAccount, &toKeys](auto const& bits) {
        for (size_t i = 0; i < bits.size(); i++)
        {
            if (!bits[i])
            {
                continue;
            }

            auto to = Account::GetAddressFromPublicKey(toKeys[i]);
            if (AccountStore::GetInstance().UpdateCoinbaseTemp(
                    to, genesisAccount, COINBASE_REWARD))
            {
                LOG_GENERAL(WARNING, "Could not reward " << to);
            }
        }
    };

    RewardEveryRound(b1);
    RewardEveryRound(b2);
}

bool Node::Coinbase(const BlockBase& lastMicroBlock, const TxBlock& lastTxBlock)
{
    if (GENESIS_WALLETS.empty())
    {
        LOG_GENERAL(WARNING, "no genesis wallet");
        return false;
    }

    Address genesisAccount(GENESIS_WALLETS[0]);

    if (m_mediator.m_DSCommitteePubKeys.size() != lastTxBlock.GetB1().size())
    {
        LOG_GENERAL(WARNING, "B1 and DS pub keys size do not match");
        return false;
    }
    if (m_mediator.m_DSCommitteePubKeys.size() != lastTxBlock.GetB2().size())
    {
        LOG_GENERAL(WARNING, "B2 and DS pub keys size do not match");
        return false;
    }
    if (m_myShardMembersPubKeys.size() != lastMicroBlock.GetB1().size())
    {
        LOG_GENERAL(WARNING,
                    "B1 and shard members pub keys size do not match "
                        << lastMicroBlock.GetB1().size() << "  "
                        << m_myShardMembersPubKeys.size());
        return false;
    }
    if (m_myShardMembersPubKeys.size() != lastMicroBlock.GetB2().size())
    {
        LOG_GENERAL(WARNING,
                    "B2 and shard members pub keys size do not match "
                        << lastMicroBlock.GetB2().size() << "  "
                        << m_myShardMembersPubKeys.size());
        return false;
    }

    // only 0th shard needs to reward DS
    if (m_myShardID == 0)
    {
        Reward(lastTxBlock.GetB1(), lastTxBlock.GetB2(),
               m_mediator.m_DSCommitteePubKeys, genesisAccount);
    }
    Reward(lastMicroBlock.GetB1(), lastMicroBlock.GetB2(),
           m_myShardMembersPubKeys, genesisAccount);

    return true;
}

void Node::InitCoinbase()
{

    uint32_t epochModuloNum
        = (m_mediator.m_currentEpochNum + 1) % NUM_FINAL_BLOCK_PER_POW;
    if (epochModuloNum == 1 || epochModuloNum == 0)
    {
        LOG_GENERAL(INFO, "Skip coinbase ");

        return;
    }

    TxBlock lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
    LOG_GENERAL(INFO,
                "Txblock num " << lastTxBlock.GetHeader().GetBlockNum()
                               << " microblock num "
                               << m_lastMicroBlockCoSig.first);
    if (m_mediator.m_currentEpochNum > 1)
    {
        if (m_lastMicroBlockCoSig.first != m_mediator.m_currentEpochNum - 1)
        {
            LOG_GENERAL(WARNING, "Stale saved cosignatures");
            return;
        }

        if (!Coinbase(m_lastMicroBlockCoSig.second, lastTxBlock))
        {
            LOG_GENERAL(WARNING, "Unable to process Coinbase");
            return;
        }

        LOG_GENERAL(INFO, "Coinbase Success");
    }
}
