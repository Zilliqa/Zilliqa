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
#include <chrono>
#include <thread>

#include "DirectoryService.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libCrypto/Sha2.h"
#include "libMediator/Mediator.h"
#include "libNetwork/P2PComm.h"
#include "libNetwork/Whitelist.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TxnRootComputation.h"

using namespace std;
using namespace boost::multiprecision;

DirectoryService::DirectoryService(Mediator& mediator)
    : m_mediator(mediator)
{
#ifndef IS_LOOKUP_NODE
    SetState(POW_SUBMISSION);
    cv_POWSubmission.notify_all();
#endif // IS_LOOKUP_NODE
    m_mode = IDLE;
    m_requesting_last_ds_block = false;
    m_consensusLeaderID = 0;
    m_consensusID = 1;
    m_viewChangeCounter = 0;
}

DirectoryService::~DirectoryService() {}

#ifndef IS_LOOKUP_NODE

void DirectoryService::StartSynchronization()
{
    LOG_MARKER();

    this->CleanVariables();

    auto func = [this]() -> void {
        m_synchronizer.FetchOfflineLookups(m_mediator.m_lookup);

        {
            unique_lock<mutex> lock(
                m_mediator.m_lookup->m_mutexOfflineLookupsUpdation);
            while (!m_mediator.m_lookup->m_fetchedOfflineLookups)
            {
                if (m_mediator.m_lookup->cv_offlineLookups.wait_for(
                        lock,
                        chrono::seconds(POW_WINDOW_IN_SECONDS
                                        + BACKUP_POW2_WINDOW_IN_SECONDS))
                    == std::cv_status::timeout)
                {
                    LOG_GENERAL(WARNING, "FetchOfflineLookups Timeout...");
                    return;
                }
            }
            m_mediator.m_lookup->m_fetchedOfflineLookups = false;
        }

        m_synchronizer.FetchDSInfo(m_mediator.m_lookup);
        while (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
        {
            m_synchronizer.FetchLatestDSBlocks(
                m_mediator.m_lookup, m_mediator.m_dsBlockChain.GetBlockCount());
            m_synchronizer.FetchLatestTxBlocks(
                m_mediator.m_lookup, m_mediator.m_txBlockChain.GetBlockCount());
            this_thread::sleep_for(chrono::seconds(NEW_NODE_SYNC_INTERVAL));
        }
    };

    DetachedFunction(1, func);
}

bool DirectoryService::CheckState(Action action)
{
    if (m_mode == Mode::IDLE)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am a non-DS node now. Why am I getting this message?");
        return false;
    }

    static const std::multimap<DirState, Action> ACTIONS_FOR_STATE
        = {{POW_SUBMISSION, PROCESS_POWSUBMISSION},
           {POW_SUBMISSION, VERIFYPOW},
           {DSBLOCK_CONSENSUS, PROCESS_DSBLOCKCONSENSUS},
           {POW2_SUBMISSION, PROCESS_POW2SUBMISSION},
           {POW2_SUBMISSION, VERIFYPOW2},
           {SHARDING_CONSENSUS, PROCESS_SHARDINGCONSENSUS},
           {MICROBLOCK_SUBMISSION, PROCESS_MICROBLOCKSUBMISSION},
           {FINALBLOCK_CONSENSUS, PROCESS_FINALBLOCKCONSENSUS},
           {VIEWCHANGE_CONSENSUS, PROCESS_VIEWCHANGECONSENSUS}};

    bool found = false;

    for (auto pos = ACTIONS_FOR_STATE.lower_bound(m_state);
         pos != ACTIONS_FOR_STATE.upper_bound(m_state); pos++)
    {
        if (pos->second == action)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Action " << GetActionString(action)
                            << " not allowed in state " << GetStateString());
        return false;
    }

    return true;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ProcessSetPrimary(const vector<unsigned char>& message,
                                         unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    // Note: This function should only be invoked during bootstrap sequence
    // Message = [Primary node IP] [Primary node port]
    LOG_MARKER();

    // Peer primary(message, offset);
    Peer primary;
    if (primary.Deserialize(message, offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize Peer.");
        return false;
    }

    if (primary == m_mediator.m_selfPeer)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am the DS committee leader");
        LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                      DS_LEADER_MSG);
        m_mode = PRIMARY_DS;
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "I am a DS committee backup. "
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << ":"
                      << m_mediator.m_selfPeer.m_listenPortHost);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Current DS committee leader is "
                      << primary.GetPrintableIPAddress() << " at port "
                      << primary.m_listenPortHost)
        LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                      DS_BACKUP_MSG);
        m_mode = BACKUP_DS;
    }

    // For now, we assume the following when ProcessSetPrimary() is called:
    //  1. All peers in the peer list are my fellow DS committee members for this first epoch
    //  2. The list of DS nodes is sorted by PubKey, including my own
    //  3. The peer with the smallest PubKey is also the first leader assigned in this call to ProcessSetPrimary()

    // Let's notify lookup node of the DS committee during bootstrap
    // TODO: Refactor this code
    if (primary == m_mediator.m_selfPeer)
    {

        PeerStore& dsstore = PeerStore::GetStore();
        dsstore.AddPeerPair(
            m_mediator.m_selfKey.second,
            m_mediator.m_selfPeer); // Add myself, but with dummy IP info
        vector<pair<PubKey, Peer>> ds = dsstore.GetAllPeerPairs();
        m_mediator.m_DSCommittee.resize(ds.size());
        copy(ds.begin(), ds.end(), m_mediator.m_DSCommittee.begin());
        // Message = [numDSPeers][DSPeer][DSPeer]... numDSPeers times
        vector<unsigned char> setDSBootstrapNodeMessage
            = {MessageType::LOOKUP, LookupInstructionType::SETDSINFOFROMSEED};
        unsigned int curr_offset = MessageOffset::BODY;

        Serializable::SetNumber<uint32_t>(setDSBootstrapNodeMessage,
                                          curr_offset, ds.size(),
                                          sizeof(uint32_t));
        curr_offset += sizeof(uint32_t);

        for (unsigned int i = 0; i < ds.size(); i++)
        {
            // PubKey
            curr_offset += ds.at(i).first.Serialize(setDSBootstrapNodeMessage,
                                                    curr_offset);
            // Peer
            curr_offset += ds.at(i).second.Serialize(setDSBootstrapNodeMessage,
                                                     curr_offset);
        }
        m_mediator.m_lookup->SendMessageToLookupNodes(
            setDSBootstrapNodeMessage);
    }

    PeerStore& peerstore = PeerStore::GetStore();
    peerstore.AddPeerPair(m_mediator.m_selfKey.second,
                          Peer()); // Add myself, but with dummy IP info

    vector<pair<PubKey, Peer>> tmp1 = peerstore.GetAllPeerPairs();
    m_mediator.m_DSCommittee.resize(tmp1.size());
    copy(tmp1.begin(), tmp1.end(), m_mediator.m_DSCommittee.begin());
    peerstore.RemovePeer(m_mediator.m_selfKey.second); // Remove myself

    // Now I need to find my index in the sorted list (this will be my ID for the consensus)
    m_consensusMyID = 0;
    for (auto const& i : m_mediator.m_DSCommittee)
    {
        if (i.first == m_mediator.m_selfKey.second)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "My node ID for this PoW consensus is "
                          << m_consensusMyID);
            break;
        }
        m_consensusMyID++;
    }
    m_consensusLeaderID = 0;
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "START OF EPOCH " << m_mediator.m_dsBlockChain.GetBlockCount());

    if (primary == m_mediator.m_selfPeer)
    {
        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][0     ] DSLD");
    }
    else
    {
        LOG_STATE("[IDENT][" << std::setw(15) << std::left
                             << m_mediator.m_selfPeer.GetPrintableIPAddress()
                             << "][" << std::setw(6) << std::left
                             << m_consensusMyID << "] DSBK");
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Waiting " << POW_WINDOW_IN_SECONDS
                         << " seconds, accepting PoW submissions...");
    this_thread::sleep_for(chrono::seconds(POW_WINDOW_IN_SECONDS));
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Starting consensus on ds block");
    RunConsensusOnDSBlock();
#endif // IS_LOOKUP_NODE

    return true;
}

#ifndef IS_LOOKUP_NODE
bool DirectoryService::CheckWhetherDSBlockIsFresh(const uint256_t dsblock_num)
{
    // uint256_t latest_block_num_in_blockchain = m_mediator.m_dsBlockChain.GetLastBlock().GetHeader().GetBlockNum();
    uint256_t latest_block_num_in_blockchain
        = m_mediator.m_dsBlockChain.GetBlockCount();

    if (dsblock_num < latest_block_num_in_blockchain)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "We are processing duplicated blocks");
        return false;
    }
    else if (dsblock_num > latest_block_num_in_blockchain)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Warning: We are missing of some DS blocks. Cur: "
                      << dsblock_num
                      << ". New: " << latest_block_num_in_blockchain);
        // Todo: handle missing DS blocks.
        return false;
    }
    return true;
}

void DirectoryService::SetState(DirState state)
{
    m_state = state;
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DS State is now " << GetStateString());
}

vector<Peer>
DirectoryService::GetBroadcastList(unsigned char ins_type,
                                   const Peer& broadcast_originator)
{
    // LOG_MARKER();

    // Regardless of the instruction type, right now all our "broadcasts" are just redundant multicasts from DS nodes to non-DS nodes
    return vector<Peer>();
}

void DirectoryService::RequestAllPoWConn()
{
    LOG_MARKER();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am requeesting AllPowConn");
    // message: [listening port]

    // In this implementation, we are only requesting it from ds leader only.
    vector<unsigned char> requestAllPoWConnMsg
        = {MessageType::DIRECTORY, DSInstructionType::AllPoWConnRequest};
    unsigned int cur_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(requestAllPoWConnMsg, cur_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    P2PComm::GetInstance().SendMessage(m_mediator.m_DSCommittee.front().second,
                                       requestAllPoWConnMsg);

    // TODO: Request from a total of 20 ds members
}

#endif // IS_LOOKUP_NODE

// Current this is only used by ds. But ideally, 20 ds nodes should
bool DirectoryService::ProcessAllPoWConnRequest(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am sending AllPowConn to requester");

    uint32_t requesterListeningPort
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));

    //  Contruct the message and send to the requester
    //  Message: [size of m_allPowConn] [pub key, peer][pub key, peer] ....
    vector<unsigned char> allPowConnMsg
        = {MessageType::DIRECTORY, DSInstructionType::AllPoWConnResponse};
    unsigned int cur_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(allPowConnMsg, cur_offset,
                                      m_allPoWConns.size(), sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    unsigned int offset_to_increment;
    for (auto& kv : m_allPoWConns)
    {
        if (kv.first == m_mediator.m_selfKey.second)
        {
            m_mediator.m_selfKey.second.Serialize(allPowConnMsg, cur_offset);
            cur_offset += PUB_KEY_SIZE;
            offset_to_increment
                = m_mediator.m_selfPeer.Serialize(allPowConnMsg, cur_offset);
            cur_offset += offset_to_increment;
        }
        else
        {
            kv.first.Serialize(allPowConnMsg, cur_offset);
            cur_offset += PUB_KEY_SIZE;
            offset_to_increment
                = kv.second.Serialize(allPowConnMsg, cur_offset);
            cur_offset += offset_to_increment;
        }
    }

    Peer peer(from.m_ipAddress, requesterListeningPort);
    P2PComm::GetInstance().SendMessage(peer, allPowConnMsg);
    return true;
}

bool DirectoryService::ProcessAllPoWConnResponse(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Updating AllPowConn");

    unsigned int cur_offset = offset;
    // 32-byte block number
    uint32_t sizeeOfAllPowConn = Serializable::GetNumber<uint32_t>(
        message, cur_offset, sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    std::map<PubKey, Peer> allPowConn;

    for (uint32_t i = 0; i < sizeeOfAllPowConn; i++)
    {
        // PubKey key(message, cur_offset);
        PubKey key;
        if (key.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize PubKey.");
            return false;
        }
        cur_offset += PUB_KEY_SIZE;

        // Peer peer(message, cur_offset);
        Peer peer;
        if (peer.Deserialize(message, cur_offset) != 0)
        {
            LOG_GENERAL(WARNING, "We failed to deserialize Peer.");
            return false;
        }

        cur_offset += IP_SIZE + PORT_SIZE;
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "updating = " << peer.GetPrintableIPAddress() << ":"
                                << peer.m_listenPortHost);

        if (m_allPoWConns.find(key) == m_allPoWConns.end())
        {
            m_allPoWConns.emplace(key, peer);
        }
    }

    {
        std::unique_lock<std::mutex> lk(m_MutexCVAllPowConn);
        m_hasAllPoWconns = true;
    }
    cv_allPowConns.notify_all();
    return true;
}

#ifndef IS_LOOKUP_NODE

void DirectoryService::LastDSBlockRequest()
{
    LOG_MARKER();
    if (m_requesting_last_ds_block)
    {
        // Already requesting for last ds block. Should re-request again.
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "DEBUG: I am already waiting for the last ds block from "
                  "ds leader.");
    }

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG: I am requesting the last ds block from ds leader.");

    // message: [listening port]
    // In this implementation, we are only requesting it from ds leader only.
    vector<unsigned char> requestAllPoWConnMsg
        = {MessageType::DIRECTORY, DSInstructionType::LastDSBlockRequest};
    unsigned int cur_offset = MessageOffset::BODY;

    Serializable::SetNumber<uint32_t>(requestAllPoWConnMsg, cur_offset,
                                      m_mediator.m_selfPeer.m_listenPortHost,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    P2PComm::GetInstance().SendMessage(m_mediator.m_DSCommittee.front().second,
                                       requestAllPoWConnMsg);
    // TODO: Request from a total of 20 ds members
}

bool DirectoryService::ProcessLastDSBlockRequest(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG: I am sending the last ds block to the requester.");

    // Deserialize the message and get the port
    uint32_t requesterListeningPort
        = Serializable::GetNumber<uint32_t>(message, offset, sizeof(uint32_t));

    // Craft the last block message
    vector<unsigned char> lastDSBlockMsg
        = {MessageType::DIRECTORY, DSInstructionType::LastDSBlockResponse};
    unsigned int cur_offset = MessageOffset::BODY;

    m_mediator.m_dsBlockChain.GetLastBlock().Serialize(lastDSBlockMsg,
                                                       cur_offset);

    Peer peer(from.m_ipAddress, requesterListeningPort);
    P2PComm::GetInstance().SendMessage(peer, lastDSBlockMsg);

    return true;
}

bool DirectoryService::ProcessLastDSBlockResponse(
    const vector<unsigned char>& message, unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (m_state != DirectoryService::DSBLOCK_CONSENSUS
        and m_requesting_last_ds_block)
    {
        // This recovery stage is meant for nodes that may get stuck in ds block consensus only.
        // Only proceed if I still need the last ds block
        return false;
    }

    // TODO: Should check whether ds block chain contain this block or not.
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "DEBUG: I received the last ds block from ds leader.");
    m_requesting_last_ds_block = false;
    unsigned int cur_offset = offset;

    DSBlock dsblock;
    if (dsblock.Deserialize(message, cur_offset) != 0)
    {
        LOG_GENERAL(WARNING, "We failed to deserialize dsblock.");
        return false;
    }
    int result = m_mediator.m_dsBlockChain.AddBlock(dsblock);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Storing DS Block Number: "
                  << dsblock.GetHeader().GetBlockNum()
                  << " with Nonce: " << dsblock.GetHeader().GetNonce()
                  << ", Difficulty: " << dsblock.GetHeader().GetDifficulty()
                  << ", Timestamp: " << dsblock.GetHeader().GetTimestamp());

    if (result == -1)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "We failed to add dsblock to dsblockchain.");
        return false;
    }

    vector<unsigned char> serializedDSBlock;
    dsblock.Serialize(serializedDSBlock, 0);
    BlockStorage::GetBlockStorage().PutDSBlock(
        dsblock.GetHeader().GetBlockNum(), serializedDSBlock);
    BlockStorage::GetBlockStorage().PushBackTxBodyDB(
        dsblock.GetHeader().GetBlockNum());

    if (TEST_NET_MODE)
    {
        LOG_GENERAL(INFO, "Updating shard whitelist");
        Whitelist::GetInstance().UpdateShardWhitelist();
    }

    SetState(POW2_SUBMISSION);
    NotifyPOW2Submission();
    ScheduleShardingConsensus(BACKUP_POW2_WINDOW_IN_SECONDS
                              - BUFFER_TIME_BEFORE_DS_BLOCK_REQUEST);
    return true;
}

bool DirectoryService::CleanVariables()
{
    LOG_MARKER();
    m_requesting_last_ds_block = false;
    {
        std::lock_guard<mutex> lock(m_MutexCVAllPowConn);
        m_hasAllPoWconns = true;
    }
    m_shards.clear();
    m_publicKeyToShardIdMap.clear();
    m_allPoWConns.clear();

    {
        std::lock_guard<mutex> lock(m_mutexConsensus);
        m_consensusObject.reset();
    }

    m_consensusBlockHash.clear();
    {
        std::lock_guard<mutex> lock(m_mutexPendingDSBlock);
        m_pendingDSBlock.reset();
    }
    {
        std::lock_guard<mutex> lock(m_mutexAllPOW);
        m_allPoWs.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexAllPOW2);
        m_allPoW2s.clear();
        m_sortedPoW2s.clear();
    }
    {
        std::lock_guard<mutex> lock(m_mutexMicroBlocks);
        m_microBlocks.clear();
    }
    m_finalBlock.reset();
    m_finalBlockMessage.clear();
    m_sharingAssignment.clear();
    m_viewChangeCounter = 0;
    m_mode = IDLE;
    m_consensusLeaderID = 0;
    m_consensusID = 0;
    return true;
}

void DirectoryService::RejoinAsDS()
{
    LOG_MARKER();
    if (m_mediator.m_lookup->m_syncType == SyncType::NO_SYNC
        && m_mode == BACKUP_DS)
    {
        auto func = [this]() mutable -> void {
            m_mediator.m_lookup->m_syncType = SyncType::DS_SYNC;
            m_mediator.m_node->CleanVariables();
            m_mediator.m_node->Install(SyncType::DS_SYNC, true);
            this->StartSynchronization();
        };
        DetachedFunction(1, func);
    }
}

bool DirectoryService::FinishRejoinAsDS()
{
    LOG_MARKER();
    m_mode = BACKUP_DS;

    m_consensusMyID = 0;
    {
        std::lock_guard<mutex> lock(m_mediator.m_mutexDSCommittee);
        LOG_GENERAL(
            INFO,
            "m_DSCommitteePubKeys size: " << m_mediator.m_DSCommittee.size());
        for (auto const& i : m_mediator.m_DSCommittee)
        {
            LOG_GENERAL(INFO, "Loop of m_DSCommitteePubKeys");
            if (i.first == m_mediator.m_selfKey.second)
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "My node ID for this PoW consensus is "
                              << m_consensusMyID);
                break;
            }
            m_consensusMyID++;
        }
    }
    // in case the recovery program is under different directory
    LOG_EPOCHINFO(to_string(m_mediator.m_currentEpochNum).c_str(),
                  DS_BACKUP_MSG);
    RunConsensusOnDSBlock(true);
    return true;
}
#endif // IS_LOOKUP_NODE

bool DirectoryService::ToBlockMessage(unsigned char ins_byte)
{
    if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
    {
        return true;
    }
    return false;
}

bool DirectoryService::Execute(const vector<unsigned char>& message,
                               unsigned int offset, const Peer& from)
{
    //LOG_MARKER();

    bool result = false;

    typedef bool (DirectoryService::*InstructionHandler)(
        const vector<unsigned char>&, unsigned int, const Peer&);

#ifndef IS_LOOKUP_NODE
    InstructionHandler ins_handlers[]
        = {&DirectoryService::ProcessSetPrimary,
           &DirectoryService::ProcessPoWSubmission,
           &DirectoryService::ProcessDSBlockConsensus,
           &DirectoryService::ProcessPoW2Submission,
           &DirectoryService::ProcessShardingConsensus,
           &DirectoryService::ProcessMicroblockSubmission,
           &DirectoryService::ProcessFinalBlockConsensus,
           &DirectoryService::ProcessAllPoWConnRequest,
           &DirectoryService::ProcessAllPoWConnResponse,
           &DirectoryService::ProcessLastDSBlockRequest,
           &DirectoryService::ProcessLastDSBlockResponse,
           &DirectoryService::ProcessViewChangeConsensus};
#else
    InstructionHandler ins_handlers[]
        = {&DirectoryService::ProcessSetPrimary,
           &DirectoryService::ProcessPoWSubmission,
           &DirectoryService::ProcessDSBlockConsensus,
           &DirectoryService::ProcessPoW2Submission,
           &DirectoryService::ProcessShardingConsensus,
           &DirectoryService::ProcessMicroblockSubmission,
           &DirectoryService::ProcessFinalBlockConsensus,
           &DirectoryService::ProcessAllPoWConnRequest,
           &DirectoryService::ProcessAllPoWConnResponse};
#endif // IS_LOOKUP_NODE

    const unsigned char ins_byte = message.at(offset);

    const unsigned int ins_handlers_count
        = sizeof(ins_handlers) / sizeof(InstructionHandler);

    if (ToBlockMessage(ins_byte))
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Ignore DS message");
        return false;
    }

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
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unknown instruction byte " << hex << (unsigned int)ins_byte);
    }

    return result;
}

#define MAKE_LITERAL_PAIR(s)                                                   \
    {                                                                          \
        s, #s                                                                  \
    }

map<DirectoryService::DirState, string> DirectoryService::DirStateStrings
    = {MAKE_LITERAL_PAIR(POW_SUBMISSION),
       MAKE_LITERAL_PAIR(DSBLOCK_CONSENSUS_PREP),
       MAKE_LITERAL_PAIR(DSBLOCK_CONSENSUS),
       MAKE_LITERAL_PAIR(POW2_SUBMISSION),
       MAKE_LITERAL_PAIR(SHARDING_CONSENSUS_PREP),
       MAKE_LITERAL_PAIR(SHARDING_CONSENSUS),
       MAKE_LITERAL_PAIR(MICROBLOCK_SUBMISSION),
       MAKE_LITERAL_PAIR(FINALBLOCK_CONSENSUS_PREP),
       MAKE_LITERAL_PAIR(FINALBLOCK_CONSENSUS),
       MAKE_LITERAL_PAIR(VIEWCHANGE_CONSENSUS_PREP),
       MAKE_LITERAL_PAIR(VIEWCHANGE_CONSENSUS),
       MAKE_LITERAL_PAIR(ERROR)};

string DirectoryService::GetStateString() const
{
    return (DirStateStrings.find(m_state) == DirStateStrings.end())
        ? "Unknown"
        : DirStateStrings.at(m_state);
}

map<DirectoryService::Action, string> DirectoryService::ActionStrings
    = {MAKE_LITERAL_PAIR(PROCESS_POWSUBMISSION),
       MAKE_LITERAL_PAIR(VERIFYPOW),
       MAKE_LITERAL_PAIR(PROCESS_DSBLOCKCONSENSUS),
       MAKE_LITERAL_PAIR(PROCESS_POW2SUBMISSION),
       MAKE_LITERAL_PAIR(VERIFYPOW2),
       MAKE_LITERAL_PAIR(PROCESS_SHARDINGCONSENSUS),
       MAKE_LITERAL_PAIR(PROCESS_MICROBLOCKSUBMISSION),
       MAKE_LITERAL_PAIR(PROCESS_FINALBLOCKCONSENSUS),
       MAKE_LITERAL_PAIR(PROCESS_VIEWCHANGECONSENSUS)};

std::string DirectoryService::GetActionString(Action action) const
{
    return (ActionStrings.find(action) == ActionStrings.end())
        ? "Unknown"
        : ActionStrings.at(action);
}