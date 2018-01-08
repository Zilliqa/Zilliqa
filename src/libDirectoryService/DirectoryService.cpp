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

#include <algorithm>
#include <thread>
#include <chrono>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "depends/libDatabase/MemoryDB.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

DirectoryService::DirectoryService(Mediator & mediator) : m_mediator(mediator)
{
#ifndef IS_LOOKUP_NODE
    SetState(POW1_SUBMISSION);
#endif // IS_LOOKUP_NODE    
    m_mode = IDLE;

    m_consensusLeaderID = 0;
    m_consensusID = 0;
}

DirectoryService::~DirectoryService()
{

}

#ifndef IS_LOOKUP_NODE
bool DirectoryService::CheckState(Action action)
{
    if(m_mode == Mode::IDLE)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: I am a non-DS node now. Why am I getting this message?");
        return false;
    }

    bool result = true;

    switch(action)
    {
        case PROCESS_POW1SUBMISSION:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW1SUBMISSION but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case VERIFYPOW1:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW1 but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_DSBLOCKCONSENSUS:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_DSBLOCKCONSENSUS but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_POW2SUBMISSION:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_POW2SUBMISSION but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case VERIFYPOW2:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing VERIFYPOW2 but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_SHARDINGCONSENSUS:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_SHARDINGCONSENSUS but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_MICROBLOCKSUBMISSION:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_MICROBLOCKSUBMISSION but already in FINALBLOCK_CONSENSUS");
                    result = false;
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        case PROCESS_FINALBLOCKCONSENSUS:
            switch(m_state)
            {
                case POW1_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in POW1_SUBMISSION");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in DSBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case DSBLOCK_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in DSBLOCK_CONSENSUS");
                    result = false;
                    break;
                case POW2_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in POW2_SUBMISSION");
                    result = false;
                    break;
                case SHARDING_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in SHARDING_CONSENSUS_PREP");
                    result = false;
                    break;
                case SHARDING_CONSENSUS:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in SHARDING_CONSENSUS");
                    result = false;
                    break;
                case MICROBLOCK_SUBMISSION:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in MICROBLOCK_SUBMISSION");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS_PREP:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Doing PROCESS_FINALBLOCKCONSENSUS but already in FINALBLOCK_CONSENSUS_PREP");
                    result = false;
                    break;
                case FINALBLOCK_CONSENSUS:
                    break;
                case ERROR:
                default:
                    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized or error state");
                    result = false;
                    break;
            }
            break;
        default:
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: Unrecognized action");
            result = false;
            break;
    }

    return result;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessSetPrimary(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
#ifndef IS_LOOKUP_NODE
    // Note: This function should only be invoked during bootstrap sequence
    // Message = [Primary node IP] [Primary node port]
    LOG_MARKER();

    Peer primary(message, offset);

    if (primary == m_mediator.m_selfPeer)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I am the DS committee leader");
        m_mode = PRIMARY_DS;
    }
    else
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "I am a DS committee backup. " << m_mediator.m_selfPeer.GetPrintableIPAddress() << ":" << m_mediator.m_selfPeer.m_listenPortHost );
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Current DS committee leader is " << primary.GetPrintableIPAddress() << " at port " << primary.m_listenPortHost)
        m_mode = BACKUP_DS;
    }

    // For now, we assume the following when ProcessSetPrimary() is called:
    //  1. All peers in the peer list are my fellow DS committee members for this first epoch
    //  2. The list of DS nodes is sorted by PubKey, including my own
    //  3. The peer with the smallest PubKey is also the first leader assigned in this call to ProcessSetPrimary()

    PeerStore & peerstore = PeerStore::GetStore();
    peerstore.AddPeer(m_mediator.m_selfKey.second, Peer());                  // Add myself, but with dummy IP info

    vector<Peer> tmp1 = peerstore.GetAllPeers();
    m_mediator.m_DSCommitteeNetworkInfo.resize(tmp1.size());
    copy(tmp1.begin(), tmp1.end(), m_mediator.m_DSCommitteeNetworkInfo.begin());     // This will be sorted by PubKey

    vector<PubKey> tmp2 = peerstore.GetAllKeys();
    m_mediator.m_DSCommitteePubKeys.resize(tmp2.size());
    copy(tmp2.begin(), tmp2.end(), m_mediator.m_DSCommitteePubKeys.begin()); // These are the sorted PubKeys

    peerstore.RemovePeer(m_mediator.m_selfKey.second);                       // Remove myself

    // Now I need to find my index in the sorted list (this will be my ID for the consensus)
    m_consensusMyID = 0;
    for (auto i = m_mediator.m_DSCommitteePubKeys.begin(); i != m_mediator.m_DSCommitteePubKeys.end(); i++)
    {
        if (*i == m_mediator.m_selfKey.second)
        {
            LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "My node ID for this PoW1 consensus is " << m_consensusMyID);
            break;
        }
        m_consensusMyID++;
    }
    m_consensusLeaderID = 0;
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "START OF EPOCH 0");

#ifdef STAT_TEST
    if (primary == m_mediator.m_selfPeer)
    {
        LOG_STATE("[IDENT][" << std::setw(15) << std::left << m_mediator.m_selfPeer.GetPrintableIPAddress() << "][0     ] DSLD");
    }
    else
    {
        LOG_STATE("[IDENT][" << std::setw(15) << std::left << m_mediator.m_selfPeer.GetPrintableIPAddress() << "][" << std::setw(6) << std::left << m_consensusMyID << "] DSBK");
    }
#endif // STAT_TEST

    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Waiting " << POW1_WINDOW_IN_SECONDS << " seconds, accepting PoW1 submissions...");
    this_thread::sleep_for(chrono::seconds(POW1_WINDOW_IN_SECONDS));
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Starting consensus on ds block");
    RunConsensusOnDSBlock();
#endif // IS_LOOKUP_NODE

    return true;
}

#ifndef IS_LOOKUP_NODE
bool DirectoryService::CheckWhetherDSBlockIsFresh(const uint256_t dsblock_num)
{
    // uint256_t latest_block_num_in_blockchain = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    uint256_t latest_block_num_in_blockchain = m_mediator.m_dsBlockChain.GetBlockCount();

    if (dsblock_num < latest_block_num_in_blockchain)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Error: We are processing duplicated blocks");
        return false;
    }
    else if (dsblock_num > latest_block_num_in_blockchain)
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Warning: We are missing of some DS blocks. Cur: " << dsblock_num << ". New: " <<
                                                                       latest_block_num_in_blockchain);
        // Todo: handle missing DS blocks.
        return false;

    }
    return true;
}

void DirectoryService::SetState(DirState state)
{
    m_state = state;
    LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "DS State is now " << m_state);
}

vector<Peer> DirectoryService::GetBroadcastList(unsigned char ins_type, const Peer & broadcast_originator)
{
    LOG_MARKER();

    // Regardless of the instruction type, right now all our "broadcasts" are just redundant multicasts from DS nodes to non-DS nodes
    return vector<Peer>();
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::Execute(const vector<unsigned char> & message, unsigned int offset, const Peer & from)
{
    LOG_MARKER();

    bool result = false;

    typedef bool(DirectoryService::*InstructionHandler)(const vector<unsigned char> &, unsigned int, const Peer &);

    InstructionHandler ins_handlers[] =
    {
        &DirectoryService::ProcessSetPrimary,
        &DirectoryService::ProcessPoW1Submission,
        &DirectoryService::ProcessDSBlockConsensus,
        &DirectoryService::ProcessPoW2Submission,
        &DirectoryService::ProcessShardingConsensus,
        &DirectoryService::ProcessMicroblockSubmission,
        &DirectoryService::ProcessFinalBlockConsensus
    };

    const unsigned char ins_byte = message.at(offset);

    const unsigned int ins_handlers_count = sizeof(ins_handlers) / sizeof(InstructionHandler);

    if (ins_byte < ins_handlers_count)
    {
        result = (this->*ins_handlers[ins_byte])(message, offset + 1, from);

        if (result == false)
        {
            // To-do: Error recovery
        }
    }
    else
    {
        LOG_MESSAGE2(to_string(m_mediator.m_currentEpochNum).c_str(), "Unknown instruction byte " << hex << (unsigned int)ins_byte);
    }

    return result;
}