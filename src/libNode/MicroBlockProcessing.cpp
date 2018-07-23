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

#include <array>
#include <chrono>
#include <functional>
#include <thread>

#include "Node.h"
#include "common/Constants.h"
#include "common/Messages.h"
#include "common/Serializable.h"
#include "depends/common/RLP.h"
#include "depends/libDatabase/MemoryDB.h"
#include "depends/libTrie/TrieDB.h"
#include "depends/libTrie/TrieHash.h"
#include "libConsensus/ConsensusUser.h"
#include "libCrypto/Sha2.h"
#include "libData/AccountData/Account.h"
#include "libData/AccountData/AccountStore.h"
#include "libData/AccountData/Transaction.h"
#include "libMediator/Mediator.h"
#include "libPOW/pow.h"
#include "libUtils/BitVector.h"
#include "libUtils/DataConversion.h"
#include "libUtils/DetachedFunction.h"
#include "libUtils/Logger.h"
#include "libUtils/SanityChecks.h"
#include "libUtils/TimeLockedFunction.h"
#include "libUtils/TimeUtils.h"
#include "libUtils/TxnRootComputation.h"
#include <boost/multiprecision/cpp_int.hpp>

using namespace std;
using namespace boost::multiprecision;

#ifndef IS_LOOKUP_NODE
void Node::SubmitMicroblockToDSCommittee() const
{
    // Message = [32-byte DS blocknum] [4-byte consensusid] [4-byte shard ID] [Tx microblock]
    vector<unsigned char> microblock
        = {MessageType::DIRECTORY, DSInstructionType::MICROBLOCKSUBMISSION};
    unsigned int cur_offset = MessageOffset::BODY;

    // 32-byte DS blocknum
    uint256_t DSBlockNum = m_mediator.m_dsBlockChain.GetBlockCount() - 1;
    Serializable::SetNumber<uint256_t>(microblock, cur_offset, DSBlockNum,
                                       UINT256_SIZE);
    cur_offset += UINT256_SIZE;

    // 4-byte consensusid
    Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_consensusID,
                                      sizeof(uint32_t));
    cur_offset += sizeof(uint32_t);

    // // 4-byte shard ID
    // Serializable::SetNumber<uint32_t>(microblock, cur_offset, m_myShardID,
    //                                   sizeof(uint32_t));
    // cur_offset += sizeof(uint32_t);

    // Tx microblock
    m_microblock->SerializeCore(microblock, cur_offset);

    LOG_STATE("[MICRO][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] SENT");
    deque<Peer> peerList;

    for (auto const& i : m_mediator.m_DSCommittee)
    {
        peerList.push_back(i.second);
    }

    P2PComm::GetInstance().SendBroadcastMessage(peerList, microblock);
}
#endif // IS_LOOKUP_NODE

bool Node::ProcessMicroblockConsensus(const vector<unsigned char>& message,
                                      unsigned int offset, const Peer& from)
{
#ifndef IS_LOOKUP_NODE
    LOG_MARKER();

    {
        lock_guard<mutex> g(m_mutexConsensus);

        // Consensus messages must be processed in correct sequence as they come in
        // It is possible for ANNOUNCE to arrive before correct DS state
        // In that case, state transition will occurs and ANNOUNCE will be processed.

        if ((m_state == TX_SUBMISSION) || (m_state == TX_SUBMISSION_BUFFER)
            || (m_state == MICROBLOCK_CONSENSUS_PREP))
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Received microblock announcement from shard leader. I "
                      "will move on to consensus");
            cv_microblockConsensus.notify_all();

            std::unique_lock<std::mutex> cv_lk(
                m_MutexCVMicroblockConsensusObject);

            if (cv_microblockConsensusObject.wait_for(
                    cv_lk, std::chrono::seconds(CONSENSUS_OBJECT_TIMEOUT),
                    [this] { return (m_state == MICROBLOCK_CONSENSUS); }))
            {
                // condition passed without timeout
            }
            else
            {
                LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                          "Time out while waiting for state transition and "
                          "consensus object creation ");
            }

            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "State transition is completed and consensus object "
                      "creation.");
        }
    }

    if (!CheckState(PROCESS_MICROBLOCKCONSENSUS))
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Not in MICROBLOCK_CONSENSUS state");
        return false;
    }

    // Consensus message must be processed in order. The following will block till it is the right order.

    std::unique_lock<mutex> cv_lk(m_mutexProcessConsensusMessage);
    if (cv_processConsensusMessage.wait_for(
            cv_lk, std::chrono::seconds(CONSENSUS_MSG_ORDER_BLOCK_WINDOW),
            [this, message, offset]() -> bool {
                lock_guard<mutex> g(m_mutexConsensus);
                if (m_mediator.m_lookup->m_syncType != SyncType::NO_SYNC)
                {
                    LOG_GENERAL(WARNING,
                                "The node started the process of rejoining, "
                                "Ignore rest of "
                                "consensus msg.")
                    return false;
                }

                if (m_consensusObject == nullptr)
                {
                    LOG_GENERAL(WARNING,
                                "m_consensusObject should have been created "
                                "but it is not")
                    return false;
                }
                return m_consensusObject->CanProcessMessage(message, offset);
            }))
    {
        // Correct order preserved
    }
    else
    {
        LOG_GENERAL(WARNING,
                    "Timeout while waiting for correct order of consensus "
                    "messages");
        return false;
    }

    lock_guard<mutex> g(m_mutexConsensus);

    bool result = m_consensusObject->ProcessMessage(message, offset, from);

    ConsensusCommon::State state = m_consensusObject->GetState();

    if (state == ConsensusCommon::State::DONE)
    {
        if (m_isPrimary == true)
        {
            LOG_STATE("[MICON]["
                      << std::setw(15) << std::left
                      << m_mediator.m_selfPeer.GetPrintableIPAddress() << "]["
                      << m_mediator.m_currentEpochNum << "] DONE");

            // Update the micro block with the co-signatures from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        if (m_isMBSender == true)
        {
            LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Designated as Microblock sender");

            // Update the micro block with the co-signature from the consensus
            m_microblock->SetCoSignatures(*m_consensusObject);

            // Multicast micro block to all DS nodes
            SubmitMicroblockToDSCommittee();
        }

        SetState(WAITING_FINALBLOCK);
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Micro block consensus "
                      << "is DONE!!! (Epoch " << m_mediator.m_currentEpochNum
                      << ")");
        m_lastMicroBlockCoSig.first = m_mediator.m_currentEpochNum;
        m_lastMicroBlockCoSig.second.SetCoSignatures(*m_consensusObject);

        lock_guard<mutex> cv_lk(m_MutexCVFBWaitMB);
        cv_FBWaitMB.notify_all();
    }
    else if (state == ConsensusCommon::State::ERROR)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Oops, no consensus reached - what to do now???");

        if (m_consensusObject->GetConsensusErrorCode()
            == ConsensusCommon::MISSING_TXN)
        {
            // Missing txns in microblock proposed by leader. Will attempt to fetch
            // missing txns from leader, set to a valid state to accept cosig1 and cosig2
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << (m_consensusObject->GetConsensusErrorMsg()));

            // Block till txn is fetched
            unique_lock<mutex> lock(m_mutexCVMicroBlockMissingTxn);
            if (cv_MicroBlockMissingTxn.wait_for(
                    lock, chrono::seconds(FETCHING_MISSING_TXNS_TIMEOUT))
                == std::cv_status::timeout)
            {
                LOG_EPOCH(WARNING,
                          to_string(m_mediator.m_currentEpochNum).c_str(),
                          "fetching missing txn timeout");
            }
            else
            {
                // Re-run consensus
                m_consensusObject->RecoveryAndProcessFromANewState(
                    ConsensusCommon::INITIAL);

                auto rerunconsensus = [this, message, offset, from]() {
                    ProcessMicroblockConsensus(message, offset, from);
                };
                DetachedFunction(1, rerunconsensus);
                return true;
            }
        }
        else
        {
            LOG_EPOCH(
                WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                "Oops, no consensus reached - unhandled consensus error. "
                "error number: "
                    << to_string(m_consensusObject->GetConsensusErrorCode())
                    << " error message: "
                    << m_consensusObject->GetConsensusErrorMsg());
        }

        // return false;
        // TODO: Optimize state transition.
        LOG_GENERAL(WARNING,
                    "ConsensusCommon::State::ERROR here, but we move on.");

        SetState(WAITING_FINALBLOCK); // Move on to next Epoch.

        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "If I received a new Finalblock from DS committee. I will "
                  "still process it");
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Consensus state = " << m_consensusObject->GetStateString());

        cv_processConsensusMessage.notify_all();
    }

    return result;
#else // IS_LOOKUP_NODE
    return true;
#endif // IS_LOOKUP_NODE
}

#ifndef IS_LOOKUP_NODE
bool Node::ComposeMicroBlock()
{
    // To-do: Replace dummy values with the required ones
    LOG_MARKER();

    // TxBlockHeader
    uint8_t type = TXBLOCKTYPE::MICRO;
    uint32_t version = BLOCKVERSION::VERSION1;
    uint32_t shardID = m_myShardID;
    uint256_t gasLimit = 100;
    uint256_t gasUsed = 1;
    BlockHash prevHash;
    fill(prevHash.asArray().begin(), prevHash.asArray().end(), 0x77);
    uint256_t blockNum = (uint256_t)m_mediator.m_currentEpochNum;
    uint256_t timestamp = get_time_as_int();
    TxnHash txRootHash;
    uint32_t numTxs = 0;
    const PubKey& minerPubKey = m_mediator.m_selfKey.second;
    uint256_t dsBlockNum = (uint256_t)m_mediator.m_currentEpochNum;
    BlockHash dsBlockHeader;
    fill(dsBlockHeader.asArray().begin(), dsBlockHeader.asArray().end(), 0x11);
    StateHash stateDeltaHash = AccountStore::GetInstance().GetStateDeltaHash();

    // TxBlock
    vector<TxnHash> tranHashes;

    unsigned int index = 0;
    {

        lock_guard<mutex> g(m_mutexProcessedTransactions);

        auto& processedTransactions = m_processedTransactions[blockNum];

        txRootHash = ComputeTransactionsRoot(processedTransactions);

        numTxs = processedTransactions.size();
        tranHashes.resize(numTxs);
        for (const auto& tx : processedTransactions)
        {
            const auto& txid = tx.first.asArray();
            copy(txid.begin(), txid.end(),
                 tranHashes.at(index).asArray().begin());
            index++;
        }
    }
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Creating new micro block.")
    m_microblock.reset(new MicroBlock(
        MicroBlockHeader(type, version, shardID, gasLimit, gasUsed, prevHash,
                         blockNum, timestamp, txRootHash, numTxs, minerPubKey,
                         dsBlockNum, dsBlockHeader, stateDeltaHash),
        tranHashes, CoSignatures()));

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Micro block proposed with "
                  << m_microblock->GetHeader().GetNumTxs()
                  << " transactions for epoch "
                  << m_mediator.m_currentEpochNum);

    return true;
}

bool Node::OnNodeMissingTxns(const std::vector<unsigned char>& errorMsg,
                             unsigned int offset, const Peer& from)
{
    LOG_MARKER();

    if (errorMsg.size() < 2 * sizeof(uint32_t) + offset)
    {
        LOG_GENERAL(WARNING, "Malformed Message");
        return false;
    }

    uint32_t numOfAbsentHashes
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    uint64_t blockNum
        = Serializable::GetNumber<uint64_t>(errorMsg, offset, sizeof(uint64_t));
    offset += sizeof(uint64_t);

    vector<TxnHash> missingTransactions;

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        TxnHash txnHash;
        copy(errorMsg.begin() + offset,
             errorMsg.begin() + offset + TRAN_HASH_SIZE,
             txnHash.asArray().begin());
        offset += TRAN_HASH_SIZE;

        missingTransactions.push_back(txnHash);
    }

    uint32_t portNo
        = Serializable::GetNumber<uint32_t>(errorMsg, offset, sizeof(uint32_t));

    uint128_t ipAddr = from.m_ipAddress;
    Peer peer(ipAddr, portNo);

    lock_guard<mutex> g(m_mutexProcessedTransactions);

    auto& processedTransactions = m_processedTransactions[blockNum];

    unsigned int cur_offset = 0;
    vector<unsigned char> tx_message
        = {MessageType::NODE, NodeInstructionType::SUBMITTRANSACTION};
    cur_offset += MessageOffset::BODY;
    tx_message.push_back(SUBMITTRANSACTIONTYPE::MISSINGTXN);
    cur_offset += MessageOffset::INST;
    Serializable::SetNumber<uint64_t>(tx_message, cur_offset, blockNum,
                                      sizeof(uint64_t));
    cur_offset += sizeof(uint64_t);

    for (uint32_t i = 0; i < numOfAbsentHashes; i++)
    {
        // LOG_GENERAL(INFO, "Peer " << from << " : " << portNo << " missing txn " << missingTransactions[i])

        Transaction t;
        // if (submittedTransactions.find(missingTransactions[i])
        //     != submittedTransactions.end())
        // {
        //     t = submittedTransactions[missingTransactions[i]];
        // }
        // else if (receivedTransactions.find(missingTransactions[i])
        //          != receivedTransactions.end())
        // {
        //     t = receivedTransactions[missingTransactions[i]];
        // }
        if (processedTransactions.find(missingTransactions[i])
            != processedTransactions.end())
        {
            t = processedTransactions[missingTransactions[i]];
        }
        else
        {
            LOG_GENERAL(INFO,
                        "Leader unable to find txn proposed in microblock "
                            << missingTransactions[i]);
            // throw exception();
            // return false;
            continue;
        }
        t.Serialize(tx_message, cur_offset);
        cur_offset += t.GetSerializedSize();
    }
    P2PComm::GetInstance().SendMessage(peer, tx_message);

    return true;
}

bool Node::OnCommitFailure(
    const std::map<unsigned int, std::vector<unsigned char>>& commitFailureMap)
{
    LOG_MARKER();

    // for(auto failureEntry: commitFailureMap)
    // {

    // }

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Going to sleep before restarting consensus");

    // std::this_thread::sleep_for(30s);
    // RunConsensusOnMicroBlockWhenShardLeader();

    // LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
    //           "Woke from sleep after consensus restart");

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "Microblock consensus failed, going to wait for final block "
              "announcement");

    return true;
}

void Node::ProcessTransactionWhenShardLeader()
{
    LOG_MARKER();

    unsigned int txn_sent_count = 0;

    std::list<Transaction> curTxns;

    auto findOneFromCreated = [this](Transaction& t) -> bool {
        lock_guard<mutex> g(m_mutexCreatedTransactions);

        auto& listIdx = m_createdTransactions.get<0>();
        if (!listIdx.size())
        {
            return false;
        }

        t = move(listIdx.front());
        listIdx.pop_front();
        return true;
    };

    auto findOneFromPrefilled = [this](Transaction& t) -> bool {
        lock_guard<mutex> g{m_mutexPrefilledTxns};

        for (auto& txns : m_prefilledTxns)
        {
            auto& txnsList = txns.second;
            if (txnsList.empty())
            {
                continue;
            }

            t = move(txnsList.front());
            txnsList.pop_front();
            m_nRemainingPrefilledTxns--;

            return true;
        }

        return false;
    };

    while (txn_sent_count
           < MAXSUBMITTXNPERNODE * m_myShardMembersPubKeys.size())
    {
        Transaction t;

        if (findOneFromCreated(t))
        {
            curTxns.emplace_back(t);
        }
        else if (findOneFromPrefilled(t))
        {
            curTxns.emplace_back(t);
        }
        else
        {
            break;
        }
        txn_sent_count++;
    }

    OrderingTxns(curTxns);

    boost::multiprecision::uint256_t blockNum
        = (uint256_t)m_mediator.m_currentEpochNum;

    auto appendOne = [this, &blockNum](const Transaction& t) {
        lock_guard<mutex> g(m_mutexProcessedTransactions);
        auto& processedTransactions = m_processedTransactions[blockNum];
        processedTransactions.insert(make_pair(t.GetTranID(), t));
    };

    for (const auto& t : curTxns)
    {
        if (m_mediator.m_validator->CheckCreatedTransaction(t)
            || !t.GetCode().empty() || !t.GetData().empty())
        {
            appendOne(t);
        }
    }
}

void Node::OrderingTxns(list<Transaction>& txns)
{
    // TODO: Implement the proper ordering of txns
    (void)txns;
}

bool Node::VerifyTxnsOrdering(const list<Transaction>& txns)
{
    // TODO: Implement after the proper ordering of txns is done
    (void)txns;
    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardLeader()
{
    LOG_MARKER();

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. Creating microblock for epoch"
                  << m_mediator.m_currentEpochNum);

    bool isVacuousEpoch
        = (m_consensusID >= (NUM_FINAL_BLOCK_PER_POW - NUM_VACUOUS_EPOCHS));
    if (!isVacuousEpoch)
    {
        ProcessTransactionWhenShardLeader();
    }
    else
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Vacuous epoch: Skipping submit transactions");
    }

    // composed microblock stored in m_microblock
    ComposeMicroBlock();

    vector<unsigned char> microblock;
    m_microblock->Serialize(microblock, 0);

    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard leader. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << m_myShardMembersNetworkInfo[m_consensusLeaderID]);

    auto nodeMissingTxnsFunc
        = [this](const vector<unsigned char>& errorMsg, unsigned int offset,
                 const Peer& from) mutable -> bool {
        return OnNodeMissingTxns(errorMsg, offset, from);
    };

    auto commitFailureFunc
        = [this](const map<unsigned int, vector<unsigned char>>& m) mutable
        -> bool { return OnCommitFailure(m); };

    deque<pair<PubKey, Peer>> peerList;
    auto it1 = m_myShardMembersPubKeys.begin();
    auto it2 = m_myShardMembersNetworkInfo.begin();

    while (it1 != m_myShardMembersPubKeys.end())
    {
        peerList.push_back(make_pair(*it1, *it2));
        ++it1;
        ++it2;
    }

    m_consensusObject.reset(new ConsensusLeader(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_mediator.m_selfKey.first, peerList, static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), nodeMissingTxnsFunc,
        commitFailureFunc));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    LOG_STATE("[MICON][" << std::setw(15) << std::left
                         << m_mediator.m_selfPeer.GetPrintableIPAddress()
                         << "][" << m_mediator.m_currentEpochNum << "] BGIN");

    ConsensusLeader* cl
        = dynamic_cast<ConsensusLeader*>(m_consensusObject.get());
    cl->StartConsensus(microblock, MicroBlockHeader::SIZE);

    return true;
}

bool Node::RunConsensusOnMicroBlockWhenShardBackup()
{
    LOG_MARKER();

    LOG_EPOCH(
        INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
        "I am a backup node. Waiting for microblock announcement for epoch "
            << m_mediator.m_currentEpochNum);
    //m_consensusID = 0;
    m_consensusBlockHash.resize(BLOCK_HASH_SIZE);
    fill(m_consensusBlockHash.begin(), m_consensusBlockHash.end(), 0x77);
    auto func = [this](const vector<unsigned char>& message,
                       vector<unsigned char>& errorMsg) mutable -> bool {
        return MicroBlockValidator(message, errorMsg);
    };

    LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
              "I am shard backup. "
                  << " m_consensusID: " << m_consensusID
                  << " m_consensusMyID: " << m_consensusMyID
                  << " m_consensusLeaderID: " << m_consensusLeaderID
                  << " Shard Leader: "
                  << m_myShardMembersNetworkInfo[m_consensusLeaderID]);

    deque<pair<PubKey, Peer>> peerList;
    auto it1 = m_myShardMembersPubKeys.begin();
    auto it2 = m_myShardMembersNetworkInfo.begin();

    while (it1 != m_myShardMembersPubKeys.end())
    {
        peerList.push_back(make_pair(*it1, *it2));
        ++it1;
        ++it2;
    }

    m_consensusObject.reset(new ConsensusBackup(
        m_consensusID, m_consensusBlockHash, m_consensusMyID,
        m_consensusLeaderID, m_mediator.m_selfKey.first, peerList,
        static_cast<unsigned char>(NODE),
        static_cast<unsigned char>(MICROBLOCKCONSENSUS), func));

    if (m_consensusObject == nullptr)
    {
        LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Unable to create consensus object");
        return false;
    }

    return true;
}

bool Node::RunConsensusOnMicroBlock()
{
    LOG_MARKER();

    // set state first and then take writer lock so that SubmitTransactions
    // if it takes reader lock later breaks out of loop

    InitCoinbase();

    SetState(MICROBLOCK_CONSENSUS_PREP);

    {
        lock_guard<mutex> g2(m_mutexNewRoundStarted);
        m_newRoundStarted = false;
    }

    if (m_isPrimary == true)
    {
        if (!RunConsensusOnMicroBlockWhenShardLeader())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardLeader");
            // throw exception();
            return false;
        }
    }
    else
    {
        if (!RunConsensusOnMicroBlockWhenShardBackup())
        {
            LOG_EPOCH(WARNING, to_string(m_mediator.m_currentEpochNum).c_str(),
                      "Error at RunConsensusOnMicroBlockWhenShardBackup");
            // throw exception();
            return false;
        }
    }

    SetState(MICROBLOCK_CONSENSUS);
    cv_microblockConsensusObject.notify_all();
    return true;
}

bool Node::CheckBlockTypeIsMicro()
{
    // Check type (must be micro block type)
    if (m_microblock->GetHeader().GetType() != TXBLOCKTYPE::MICRO)
    {
        LOG_GENERAL(WARNING,
                    "Type check failed. Expected: "
                        << (unsigned int)TXBLOCKTYPE::MICRO << " Actual: "
                        << (unsigned int)m_microblock->GetHeader().GetType());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK);

        return false;
    }

    LOG_GENERAL(INFO, "Type check passed");

    return true;
}

bool Node::CheckMicroBlockVersion()
{
    // Check version (must be most current version)
    if (m_microblock->GetHeader().GetVersion() != BLOCKVERSION::VERSION1)
    {
        LOG_GENERAL(
            WARNING,
            "Version check failed. Expected: "
                << (unsigned int)BLOCKVERSION::VERSION1 << " Actual: "
                << (unsigned int)m_microblock->GetHeader().GetVersion());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_VERSION);

        return false;
    }

    LOG_GENERAL(INFO, "Version check passed");

    return true;
}

bool Node::CheckMicroBlockTimestamp()
{
    // Check timestamp (must be greater than timestamp of last Tx block header in the Tx blockchain)
    if (m_mediator.m_txBlockChain.GetBlockCount() > 0)
    {
        const TxBlock& lastTxBlock = m_mediator.m_txBlockChain.GetLastBlock();
        uint256_t thisMicroblockTimestamp
            = m_microblock->GetHeader().GetTimestamp();
        uint256_t lastTxBlockTimestamp = lastTxBlock.GetHeader().GetTimestamp();
        if (thisMicroblockTimestamp <= lastTxBlockTimestamp)
        {
            LOG_GENERAL(WARNING,
                        "Timestamp check failed. Last Tx Block: "
                            << lastTxBlockTimestamp
                            << " Microblock: " << thisMicroblockTimestamp);

            m_consensusObject->SetConsensusErrorCode(
                ConsensusCommon::INVALID_TIMESTAMP);

            return false;
        }
    }

    LOG_GENERAL(INFO, "Timestamp check passed");

    return true;
}

bool Node::ProcessTransactionWhenShardBackup(const vector<TxnHash>& tranHashes,
                                             vector<TxnHash>& missingtranHashes)
{
    LOG_MARKER();

    auto findFromCreated = [this](Transaction& t, const TxnHash& th) -> bool {
        lock_guard<mutex> g(m_mutexCreatedTransactions);

        auto& hashIdx = m_createdTransactions.get<1>();
        if (!hashIdx.size())
        {
            return false;
        }

        // auto it = find_if(
        //     begin(m_createdTransactions), end(m_createdTransactions),
        //     [&th](const Transaction& t) { return t.GetTranID() == th; });

        // if (m_createdTransactions.end() == it)
        // {
        //     LOG_GENERAL(WARNING, "txn is not found");
        //     return false;
        // }

        // t = move(*it);
        // m_createdTransactions.erase(it);

        auto it = hashIdx.find(th);

        if (hashIdx.end() == it)
        {
            LOG_GENERAL(WARNING, "txn is not found");
            return false;
        }

        t = move(*it);
        hashIdx.erase(it);

        return true;
    };

    auto findFromPrefilled = [this](Transaction& t, const TxnHash& th) -> bool {
        lock_guard<mutex> g{m_mutexPrefilledTxns};

        for (auto& txns : m_prefilledTxns)
        {
            auto& txnsList = txns.second;
            if (txnsList.empty())
            {
                continue;
            }

            auto it = find_if(
                begin(txnsList), end(txnsList),
                [&th](const Transaction& t) { return t.GetTranID() == th; });

            if (txnsList.end() == it)
            {
                continue;
            }

            t = move(*it);
            txnsList.erase(it);
            m_nRemainingPrefilledTxns--;

            return true;
        }

        return false;
    };

    std::list<Transaction> curTxns;

    for (const auto& tranHash : tranHashes)
    {
        Transaction t;

        if (findFromCreated(t, tranHash))
        {
            curTxns.emplace_back(t);
        }
        else if (findFromPrefilled(t, tranHash))
        {
            curTxns.emplace_back(t);
        }
        else
        {
            missingtranHashes.emplace_back(tranHash);
        }
    }

    if (!missingtranHashes.empty())
    {
        return true;
    }

    if (!VerifyTxnsOrdering(curTxns))
    {
        return false;
    }

    boost::multiprecision::uint256_t blockNum
        = (uint256_t)m_mediator.m_currentEpochNum;

    auto appendOne = [this, &blockNum](const Transaction& t) {
        lock_guard<mutex> g(m_mutexProcessedTransactions);
        auto& processedTransactions = m_processedTransactions[blockNum];
        processedTransactions.insert(make_pair(t.GetTranID(), t));
    };

    for (const auto& t : curTxns)
    {
        if (m_mediator.m_validator->CheckCreatedTransaction(t)
            || !t.GetCode().empty() || !t.GetData().empty())
        {
            appendOne(t);
        }
    }

    AccountStore::GetInstance().SerializeDelta();

    return true;
}

unsigned char Node::CheckLegitimacyOfTxnHashes(vector<unsigned char>& errorMsg)
{
    lock_guard<mutex> g(m_mutexProcessedTransactions);

    vector<TxnHash> missingTxnHashes;
    if (!ProcessTransactionWhenShardBackup(m_microblock->GetTranHashes(),
                                           missingTxnHashes))
    {
        return LEGITIMACYRESULT::WRONGORDER;
    }

    m_numOfAbsentTxnHashes = 0;

    int offset = 0;

    for (auto const& hash : missingTxnHashes)
    {
        LOG_EPOCH(INFO, to_string(m_mediator.m_currentEpochNum).c_str(),
                  "Missing txn: " << hash)
        if (errorMsg.size() == 0)
        {
            errorMsg.resize(sizeof(uint32_t) + sizeof(uint64_t)
                            + TRAN_HASH_SIZE);
            offset += (sizeof(uint32_t) + sizeof(uint64_t));
        }
        else
        {
            errorMsg.resize(offset + TRAN_HASH_SIZE);
        }
        copy(hash.asArray().begin(), hash.asArray().end(),
             errorMsg.begin() + offset);
        offset += TRAN_HASH_SIZE;
        m_numOfAbsentTxnHashes++;
    }

    if (m_numOfAbsentTxnHashes > 0)
    {
        Serializable::SetNumber<uint32_t>(errorMsg, 0, m_numOfAbsentTxnHashes,
                                          sizeof(uint32_t));
        Serializable::SetNumber<uint64_t>(errorMsg, sizeof(uint32_t),
                                          m_mediator.m_currentEpochNum,
                                          sizeof(uint64_t));

        m_txnsOrdering = m_microblock->GetTranHashes();
        return LEGITIMACYRESULT::MISSEDTXN;
    }

    return LEGITIMACYRESULT::SUCCESS;
}

bool Node::CheckMicroBlockHashes(vector<unsigned char>& errorMsg)
{
    // Check transaction hashes (number of hashes must be = Tx count field)
    uint32_t txhashessize = m_microblock->GetTranHashes().size();
    uint32_t numtxs = m_microblock->GetHeader().GetNumTxs();
    if (txhashessize != numtxs)
    {
        LOG_GENERAL(WARNING,
                    "Tx hashes check failed. Tx hashes size: "
                        << txhashessize << " Num txs: " << numtxs);

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_BLOCK_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Hash count check passed");

    switch (CheckLegitimacyOfTxnHashes(errorMsg))
    {
    case LEGITIMACYRESULT::SUCCESS:
        break;
    case LEGITIMACYRESULT::MISSEDTXN:
        LOG_GENERAL(WARNING,
                    "Missing a txn hash included in proposed microblock");
        m_consensusObject->SetConsensusErrorCode(ConsensusCommon::MISSING_TXN);
        return false;
    case LEGITIMACYRESULT::WRONGORDER:
        LOG_GENERAL(WARNING, "Order of txns proposed by leader is wrong");
        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::WRONG_TXN_ORDER);
        return false;
    default:
        return false;
    }

    LOG_GENERAL(INFO, "Hash legitimacy check passed");

    return true;
}

bool Node::CheckMicroBlockTxnRootHash()
{
    // Check transaction root
    TxnHash expectedTxRootHash
        = ComputeTransactionsRoot(m_microblock->GetTranHashes());

    LOG_GENERAL(
        INFO,
        "Microblock root computation done "
            << DataConversion::charArrToHexStr(expectedTxRootHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetTxRootHash().asArray()));

    if (expectedTxRootHash != m_microblock->GetHeader().GetTxRootHash())
    {
        LOG_GENERAL(WARNING, "Txn root does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_ROOT_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "Root check passed");

    return true;
}

bool Node::CheckMicroBlockStateDeltaHash()
{
    StateHash expectedStateDeltaHash
        = AccountStore::GetInstance().GetStateDeltaHash();

    LOG_GENERAL(INFO,
                "Microblock state delta generation done "
                    << DataConversion::charArrToHexStr(
                           expectedStateDeltaHash.asArray()));
    LOG_GENERAL(INFO,
                "Expected root: " << DataConversion::charArrToHexStr(
                    m_microblock->GetHeader().GetStateDeltaHash().asArray()));

    if (expectedStateDeltaHash != m_microblock->GetHeader().GetStateDeltaHash())
    {
        LOG_GENERAL(WARNING, "State delta hash does not match");

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_STATE_DELTA_HASH);

        return false;
    }

    LOG_GENERAL(INFO, "State delta hash check passed");

    return true;
}

bool Node::CheckMicroBlockShardID()
{
    // Check version (must be most current version)
    if (m_microblock->GetHeader().GetShardID() != m_myShardID)
    {
        LOG_GENERAL(WARNING,
                    "ShardID check failed. Expected: "
                        << m_myShardID << " Actual: "
                        << m_microblock->GetHeader().GetShardID());

        m_consensusObject->SetConsensusErrorCode(
            ConsensusCommon::INVALID_MICROBLOCK_SHARD_ID);

        return false;
    }

    LOG_GENERAL(INFO, "ShardID check passed");

    return true;
}

bool Node::MicroBlockValidator(const vector<unsigned char>& microblock,
                               vector<unsigned char>& errorMsg)
{
    LOG_MARKER();

    // [TODO] To put in the logic
    m_microblock = make_shared<MicroBlock>(MicroBlock(microblock, 0));

    bool valid = false;

    do
    {
        if (!CheckBlockTypeIsMicro() || !CheckMicroBlockVersion()
            || !CheckMicroBlockTimestamp() || !CheckMicroBlockHashes(errorMsg)
            || !CheckMicroBlockTxnRootHash() || !CheckMicroBlockStateDeltaHash()
            || !CheckMicroBlockShardID())
        {
            break;
        }

        // Check gas limit (must satisfy some equations)
        // Check gas used (must be <= gas limit)
        // Check state root (TBD)
        // Check pubkey (must be valid and = shard leader)
        // Check parent DS hash (must be = digest of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)
        // Check parent DS block number (must be = block number of last DS block header in the DS blockchain)
        // Need some rework to be able to access DS blockchain (or we switch to using the persistent storage lib)

        valid = true;
    } while (false);

    if (!valid)
    {
        m_microblock = nullptr;
        Serializable::SetNumber<uint32_t>(
            errorMsg, errorMsg.size(), m_mediator.m_selfPeer.m_listenPortHost,
            sizeof(uint32_t));
        // LOG_GENERAL(INFO, "To-do: What to do if proposed microblock is not valid?");
        return false;
    }

    return valid;

    // #else // IS_LOOKUP_NODE

    // return true;
}
#endif // IS_LOOKUP_NODE
