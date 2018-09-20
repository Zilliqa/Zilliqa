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

#include "Archival.h"
#include "libUtils/DetachedFunction.h"
#include <chrono>
#include <mutex>
#include <thread>

using namespace std;

unsigned int REFRESH_DELAY = 3;

void Archival::InitSync()
{
    auto func = [this]() -> void {
        m_synchronizer.FetchDSInfo(m_mediator.m_lookup);
        m_synchronizer.FetchOfflineLookups(m_mediator.m_lookup);
        uint64_t dsBlockNum = 0;
        uint64_t txBlockNum = 0;
        while (true)
        {
            if (m_mediator.m_dsBlockChain.GetBlockCount() != 1)
            {
                dsBlockNum = m_mediator.m_dsBlockChain.GetBlockCount();
            }
            if (m_mediator.m_txBlockChain.GetBlockCount() != 1)
            {
                txBlockNum = m_mediator.m_dsBlockChain.GetBlockCount();
            }
            m_synchronizer.FetchLatestDSBlocks(m_mediator.m_lookup, dsBlockNum);
            m_synchronizer.FetchLatestTxBlocks(m_mediator.m_lookup, txBlockNum);
            m_synchronizer.FetchLatestState(m_mediator.m_lookup);
            if (!m_mediator.m_lookup->CheckStateRoot())
            {
                LOG_GENERAL(WARNING, "Archival State Root mis-match");
            }
            m_mediator.m_lookup->GetShardFromLookup();
            this_thread::sleep_for(chrono::seconds(REFRESH_DELAY));
        }
    };
    DetachedFunction(1, func);
}

void Archival::Init()
{
    m_mediator.m_dsBlockChain.Reset();
    m_mediator.m_txBlockChain.Reset();
    {
        std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
        m_mediator.m_DSCommittee->clear();
    }
    AccountStore::GetInstance().Init();

    m_synchronizer.InitializeGenesisBlocks(m_mediator.m_dsBlockChain,
                                           m_mediator.m_txBlockChain);
}

Archival::Archival(Mediator& mediator)
    : m_mediator(mediator)
{
}